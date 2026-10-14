#ifndef NDOF_CORE_ALLOCATOR_SUPPORT_HPP
#define NDOF_CORE_ALLOCATOR_SUPPORT_HPP
#include "configs.hpp"

#include <concepts>
#include <expected>
#include <memory>
// TODO: Conditionally include.
#include <type_traits>


// Wrapping ndof exceptions as inner exceptions preserves a rethrow chain that can
// later be rendered as a stack trace.

// TODO: All exceptions should have an inner exception. Remove the link between the conditional exceptions and inherit from std::exception instead.
//      Change the optional return type of captured_exception() to std::exception_ptr instead of std::optional<std::exception_ptr>.
//      This will allow for a more consistent exception handling model and make it easier to propagate exceptions through different layers of the application.

// TODO: Does exception need to be allocator aware? If not, remove the allocator stuff from the base classes as necessary.


namespace ndof {


template<typename Allocator>
concept has_allocator_definitions = requires {
    typename std::allocator_traits<Allocator>::value_type;
    typename std::allocator_traits<Allocator>::pointer;
    typename std::allocator_traits<Allocator>::const_pointer;
    typename std::allocator_traits<Allocator>::void_pointer;
    typename std::allocator_traits<Allocator>::const_void_pointer;
};

template<typename T>
concept allocator_like =
    ndof::has_allocator_definitions<T> &&
    std::destructible<T>;

template<typename T>
concept allocator_aware =
    requires(T t) {
        typename T::allocator_type;
        {t.get_allocator()} -> std::same_as<typename T::allocator_type>;
    } && ndof::allocator_like<typename T::allocator_type>;
 

template<typename Allocator, typename ExpectedAllocator>
concept allocator_compatible_with =
    ndof::allocator_like<Allocator> &&
    ndof::allocator_like<ExpectedAllocator> &&
    std::convertible_to<
        typename std::allocator_traits<Allocator>::value_type,
        typename std::allocator_traits<ExpectedAllocator>::value_type> &&
    std::constructible_from<ExpectedAllocator, const Allocator&>;

template<typename T>
concept allocator_aware_copy_propagating =
    requires {
        typename T::allocator_type;
        typename std::allocator_traits<typename T::allocator_type>::propagate_on_container_copy_assignment;
    } && std::allocator_traits<typename T::allocator_type>::propagate_on_container_copy_assignment::value;

template<typename T>
concept allocator_aware_move_propagating =
    requires {
        typename T::allocator_type;
        typename std::allocator_traits<typename T::allocator_type>::propagate_on_container_move_assignment;
    } && std::allocator_traits<typename T::allocator_type>::propagate_on_container_move_assignment::value;
 
template <typename T>
using get_allocator_result_type_t = std::remove_cvref_t<decltype(
    std::declval<std::remove_cvref_t<T>&>().get_allocator())>;

template <typename T, typename Allocator>
concept allocator_compatible_with_get_allocator =
    requires {
        typename get_allocator_result_type_t<T>;
    } && ndof::allocator_compatible_with<get_allocator_result_type_t<T>, Allocator>;


// TODO: Fix this. Should be hidden and should use a singleton factory.
inline auto get_default_allocator() noexcept {
    return std::allocator<char>();
}

using default_allocator_t = decltype(get_default_allocator());

template<typename T>
using rebound_default_allocator_t = typename std::allocator_traits<default_allocator_t>::template rebind_alloc<T>;

template<typename T>
rebound_default_allocator_t<T> get_rebound_default_allocator() noexcept {
    return rebound_default_allocator_t<T>(get_default_allocator());
}

template<allocator_like Alloc>
[[nodiscard]] consteval bool allocator_is_nothrow_deallocate() {
    return noexcept(std::allocator_traits<Alloc>::deallocate(
        std::declval<Alloc&>(), 
        std::declval<typename std::allocator_traits<Alloc>::pointer>(), 
        std::declval<typename std::allocator_traits<Alloc>::size_type>())
    );
}

// TODO: Consider a better name.
template<allocator_like Alloc>
[[nodiscard]] consteval bool is_no_throw_with_allocator()  noexcept {
    return (!ndof::exceptions_feature_enabled()) || allocator_is_nothrow_deallocate<Alloc>();
}


} // namespace ndof::error

#endif // NDOF_CORE_ALLOCATOR_SUPPORT_HPP
