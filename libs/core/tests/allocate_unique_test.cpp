// Copyright 2026 The ndof Authors
// SPDX-License-Identifier: Apache-2.0

#include "ndof/core/allocate_unique.hpp"

#include <concepts>
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>

namespace {

struct allocation_counts {
    std::size_t allocated = 0;
    std::size_t deallocated = 0;
};

template<typename T>
class tracking_allocator {
public:
    using value_type = T;

    explicit tracking_allocator(allocation_counts& counts) noexcept
        : counts_(&counts) {}

    template<typename U>
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

    template<typename U>
    friend class tracking_allocator;

private:
    allocation_counts* counts_;
};

struct counted_object {
    counted_object() {
        ++live_count;
    }

    counted_object(const counted_object&) = delete;
    counted_object& operator=(const counted_object&) = delete;
    counted_object(counted_object&&) = delete;
    counted_object& operator=(counted_object&&) = delete;

    ~counted_object() {
        --live_count;
    }

    static inline int live_count = 0;
};

static_assert(std::same_as<
              typename ndof::allocated_unique_ptr<int>::deleter_type,
              ndof::deallocating_deleter>);
static_assert(std::same_as<
              typename ndof::allocated_unique_ptr<int[]>::deleter_type,
              ndof::deallocating_deleter>);

TEST(AllocateUnique, StateDestroysAndDeallocatesTypedObjects) {
    allocation_counts counts;

    {
        auto scalar = ndof::make_unique_with_allocator<counted_object>(
            tracking_allocator<counted_object>{counts});
        auto bounded = ndof::make_unique_with_allocator<counted_object[2]>(
            tracking_allocator<counted_object>{counts});
        auto unbounded = ndof::make_unique_with_allocator<counted_object[]>(
            tracking_allocator<counted_object>{counts}, 3);

        EXPECT_EQ(counted_object::live_count, 6);
        EXPECT_EQ(counts.allocated, 6U);
    }

    EXPECT_EQ(counted_object::live_count, 0);
    EXPECT_EQ(counts.deallocated, 6U);
}

} // namespace