#ifndef NDOF_CORE_ALLOCATE_UNIQUE_HPP
#define NDOF_CORE_ALLOCATE_UNIQUE_HPP
#include "configs.hpp"
#include "allocator_support.hpp"
#include "definitions.hpp"

#include <cstddef>
#include <cstdint>
 
#include <expected>
#include <memory>
#include <type_traits>

namespace ndof {

        enum allocation_error : std::uint8_t {
            allocation_failed
        };
        
        namespace detail {
        template<typename T, typename A, typename ...Args>
        consteval bool ctor_is_noexcept() {
            using traits = std::allocator_traits<A>;
            return noexcept(traits::construct(std::declval<A&>(), std::declval<typename traits::pointer>(), std::declval<Args>()...));
        }

        template<typename T, typename Alloc, typename... Args>
        void construct(Alloc& alloc, T* pointer, Args&&... args) noexcept(!ndof::exceptions_feature_enabled() || ctor_is_noexcept<T, Alloc, Args...>()) {
            using traits = std::allocator_traits<Alloc>;
            // Note: This will throw an exception if the constructor of T throws an exception.  
            //       If exceptions are disabled, this will call std::terminate()
            traits::construct(alloc, pointer, std::forward<Args>(args)...);
        }

        template<typename T, typename Alloc>
        void construct_array([[maybe_unused]] Alloc& alloc, T* pointer, std::size_t count) noexcept(!ndof::exceptions_feature_enabled()) {
            // Handles destroying already-constructed elements on exception; caller still owns deallocation.
            std::uninitialized_value_construct_n(pointer, count);
        }
   
    
    } // namespace ndof::detail

    

    // There is a memory penalty here that is unavoidable. 
    // In order to achieve type erasure of the allocator,
    // we need to store a type-erased version of the allocator along with the allocation count.
    struct deallocating_deleter {
    private:
        struct allocator_state {
            allocator_state() = default;
            allocator_state(const allocator_state&) = delete;
            allocator_state& operator=(const allocator_state&) = delete;
            allocator_state(allocator_state&&) = delete;
            allocator_state& operator=(allocator_state&&) = delete;
            virtual ~allocator_state() = default;

            [[nodiscard]] virtual std::unique_ptr<allocator_state> clone() const = 0;
            virtual void deallocate_and_destroy(void* pointer) noexcept = 0;
        };

        template<typename Alloc>
        struct allocator_state_for final : allocator_state {
            using allocator_type = Alloc;
            using element_type = typename std::allocator_traits<allocator_type>::value_type;

            explicit allocator_state_for(allocator_type allocator)
                : alloc(std::move(allocator)) {}

            [[nodiscard]] std::unique_ptr<allocator_state> clone() const override {
                return std::make_unique<allocator_state_for>(alloc);
            }

            void deallocate_and_destroy(void* pointer) noexcept override {
                auto* typed_pointer = static_cast<element_type*>(pointer);
                std::destroy_at(typed_pointer);
                std::allocator_traits<allocator_type>::deallocate(alloc, typed_pointer, 1);
            }

        private:
            [[no_unique_address]] allocator_type alloc;
        };

        template<typename Alloc>
        struct allocator_state_with_count_for final : allocator_state {
            using allocator_type = Alloc;
            using element_type = typename std::allocator_traits<allocator_type>::value_type;

            allocator_state_with_count_for(allocator_type allocator, std::size_t allocation_count)
                : alloc(std::move(allocator)), count(allocation_count) {}

            [[nodiscard]] std::unique_ptr<allocator_state> clone() const override {
                return std::make_unique<allocator_state_with_count_for>(alloc, count);
            }

            void deallocate_and_destroy(void* pointer) noexcept override {
                auto* typed_pointer = static_cast<element_type*>(pointer);
                std::destroy_n(typed_pointer, count);
                std::allocator_traits<allocator_type>::deallocate(alloc, typed_pointer, count);
            }

        private:
            [[no_unique_address]] allocator_type alloc;
            std::size_t count;
        };

        std::unique_ptr<allocator_state> state;
 
        template<typename Alloc>
        std::unique_ptr<allocator_state> initialize_state(Alloc alloc, std::size_t count) {
            if (count == 1) {
                return std::make_unique<allocator_state_for<Alloc>>(std::move(alloc));
            }
            return std::make_unique<allocator_state_with_count_for<Alloc>>(std::move(alloc), count);
        }

    public:
        deallocating_deleter() = default;
        ~deallocating_deleter() = default;

        template<typename Alloc>
        explicit deallocating_deleter([[maybe_unused]] Alloc alloc, std::size_t count = 1)
            : state(initialize_state(std::move(alloc), count)) {
                  }

        // TODO: Probably should be icloneable? But then we'd have to start worrying about
        //       a lot of circular dependencies. And then we'd have to create additional
        //       abstractions.  This might be the right path on a number of fronts.
        //       This might include elevating ndof::error as well.
        deallocating_deleter(const deallocating_deleter& other)
            : state(other.state ? other.state->clone() : nullptr) {}

        deallocating_deleter& operator=(const deallocating_deleter& other) {
            if (this != &other) {
                auto replacement =
                    other.state ? other.state->clone() : nullptr;
                state = std::move(replacement);
            }
            return *this;
        }

        deallocating_deleter(deallocating_deleter&&) noexcept = default;
        deallocating_deleter& operator=(deallocating_deleter&&) noexcept = default;

        void operator()(void* pointer) const noexcept {
            if (pointer != nullptr) {
                state->deallocate_and_destroy(pointer);
            }
        }
    };

    // TODO: Tighten this up by not losing any information (i.e., extent, etc.), maybe with a helper.
    //       Also consider multidimensional arrays.
    template<typename T>
    using allocated_unique_ptr = std::unique_ptr<
        std::conditional_t<std::is_array_v<T>, std::remove_extent_t<T>[], T>,
        deallocating_deleter>;

    template<typename T>
    using allocation_result_t = std::conditional_t<
        ndof::exceptions_feature_enabled(),
        allocated_unique_ptr<T>,
        std::expected<allocated_unique_ptr<T>, allocation_error>>;

    template<typename T, typename Alloc, typename... Args>
        requires (!std::is_array_v<T>)
    auto make_unique_with_allocator(Alloc alloc, Args&&... args)
        noexcept(!ndof::exceptions_feature_enabled())
        -> allocation_result_t<T>
    {
        using A = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
        using traits = std::allocator_traits<A>;

        A a{alloc};
        deallocating_deleter deleter{a};
        T* p = traits::allocate(a, 1);

        if constexpr (ndof::exceptions_feature_enabled()) {
#if defined(NDOF_EXCEPTIONS_FEATURE_ENABLED) && NDOF_EXCEPTIONS_FEATURE_ENABLED == 1
            try {
                detail::construct(
                    a, p, std::forward<Args>(args)...);
            }
            catch (...) {
                traits::deallocate(a, p, 1);
                throw;
            }
#endif
        }
        else {
            if (p == nullptr) {
                return std::unexpected(allocation_error::allocation_failed);
            }
            // Will terminate if this fails. No need to deallocate manually.
            traits::construct(a, p, std::forward<Args>(args)...);
        }

        return allocated_unique_ptr<T>{p, std::move(deleter)};
    }

    template<bounded_array T, allocator_like Alloc>
    auto make_unique_with_allocator(Alloc alloc) noexcept(!ndof::exceptions_feature_enabled())
        -> allocation_result_t<T>
    {
        using element_type = std::remove_extent_t<T>;
        using A = typename std::allocator_traits<Alloc>::template rebind_alloc<element_type>;
        using traits = std::allocator_traits<A>;

        A a{alloc};
        constexpr std::size_t count = std::extent_v<T>;
        element_type* p = traits::allocate(a, count);

        if constexpr (ndof::exceptions_feature_enabled()) {
            // An exception will be thrown if p is null or if construction fails.
            detail::construct_array(a, p, count);
        }
        else {
            if (!p) {
                return std::unexpected(allocation_error::allocation_failed);
            }
            detail::construct_array(a, p, count);

        }

        return allocated_unique_ptr<T>{p, deallocating_deleter{a, count}};
    }

    template<typename T, typename Alloc>
        requires unbounded_array<T>
    auto make_unique_with_allocator(Alloc alloc, std::size_t count) noexcept(!ndof::exceptions_feature_enabled())
        -> allocation_result_t<T> {
        using element_type = std::remove_extent_t<T>;
        using A = typename std::allocator_traits<Alloc>::template rebind_alloc<element_type>;
        using traits = std::allocator_traits<A>;

        A a{alloc};
        element_type* p = traits::allocate(a, count);

        if constexpr (ndof::exceptions_feature_enabled()) {
            detail::construct_array(a, p, count);
        }
        else {
            if (!p && count != 0) {
                return std::unexpected(allocation_error::allocation_failed);
            }
            for (std::size_t constructed = 0; constructed < count; ++constructed) {
                traits::construct(a, p + constructed);
            }
        }

        return allocated_unique_ptr<T> {
            p, deallocating_deleter{a, count}
        };
    }
} // namespace ndof
    // In exception-free mode, recoverable allocation failures are returned;
    // any unexpected exception terminates at this noexcept boundary.

#endif // NDOF_CORE_ALLOCATE_UNIQUE_HPP
