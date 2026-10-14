#include <ndof/core/allocate_unique.hpp>

#include <concepts>
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <utility>

namespace {

struct allocation_counts {
    std::size_t allocated = 0;
    std::size_t deallocated = 0;
};

template<class T>
class tracking_allocator {
public:
    using value_type = T;

    tracking_allocator() = default;

    explicit tracking_allocator(allocation_counts& counts) noexcept
        : counts_(&counts) {}

    template<class U>
    tracking_allocator(const tracking_allocator<U>& other) noexcept
        : counts_(other.counts_) {}

    [[nodiscard]] T* allocate(std::size_t count) {
        counts_->allocated += count;
        return std::allocator<T>{}.allocate(count);
    }

    void deallocate(T* pointer, std::size_t count) noexcept {
        counts_->deallocated += count;
        std::allocator<T>{}.deallocate(pointer, count);
    }

    template<class U>
    friend class tracking_allocator;

    friend bool operator==(const tracking_allocator&, const tracking_allocator&) = default;

private:
    allocation_counts* counts_ = nullptr;
};

struct counted_object {
    counted_object() {
        ++live_count;
    }

    ~counted_object() {
        --live_count;
    }

    static inline int live_count = 0;
};

struct allocator_aware_object {
    using allocator_type = tracking_allocator<allocator_aware_object>;

    explicit allocator_aware_object(allocator_type allocator) noexcept
        : allocator_(allocator) {}

    [[nodiscard]] const allocator_type& get_allocator() const noexcept {
        return allocator_;
    }

private:
    allocator_type allocator_;
};

using tracking_pointer = decltype(
    ndof::make_unique_with_allocator<int>(std::declval<tracking_allocator<int>>(), 42));
using standard_pointer = decltype(
    ndof::make_unique_with_allocator<int>(std::declval<std::allocator<int>>(), 42));

static_assert(std::same_as<tracking_pointer, standard_pointer>);
static_assert(std::same_as<
              typename tracking_pointer::deleter_type,
              ndof::deallocating_deleter>);

TEST(AllocateUnique, UsesProvidedAllocator) {
    allocation_counts counts;

    {
        auto pointer = ndof::make_unique_with_allocator<int>(tracking_allocator<int>{counts}, 42);
        EXPECT_EQ(*pointer, 42);
        EXPECT_EQ(counts.allocated, 1U);
    }

    EXPECT_EQ(counts.deallocated, 1U);
}

TEST(AllocateUnique, DestroysEveryArrayElement) {
    allocation_counts counts;

    {
        auto bounded = ndof::make_unique_with_allocator<counted_object[3]>(
            tracking_allocator<counted_object>{counts});
        EXPECT_EQ(counted_object::live_count, 3);

        auto unbounded = ndof::make_unique_with_allocator<counted_object[]>(
            tracking_allocator<counted_object>{counts}, 2);
        EXPECT_EQ(counted_object::live_count, 5);
    }

    EXPECT_EQ(counted_object::live_count, 0);
    EXPECT_EQ(counts.allocated, 5U);
    EXPECT_EQ(counts.deallocated, 5U);
}

} // namespace
