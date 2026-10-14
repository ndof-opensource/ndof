#ifndef NDOF_EXCEPTIONS_FEATURE_ENABLED
#define NDOF_EXCEPTIONS_FEATURE_ENABLED 0
#endif

#include <ndof/core/allocate_unique.hpp>

#include <concepts>
#include <cstddef>
#include <expected>
#include <memory>
#include <utility>

template<class T>
struct null_allocator {
    using value_type = T;

    null_allocator() = default;

    template<class U>
    null_allocator(const null_allocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t) noexcept {
        return nullptr;
    }

    void deallocate(T*, std::size_t) noexcept {}
};

using pointer_type = ndof::allocated_unique_ptr<int>;
using result_type = decltype(
    ndof::make_unique_with_allocator<int>(std::declval<null_allocator<int>>(), 42));

static_assert(std::same_as<result_type, std::expected<pointer_type, ndof::allocation_error>>);
static_assert(!ndof::exceptions_feature_enabled());

int main() {
    auto result = ndof::make_unique_with_allocator<int>(null_allocator<int>{}, 42);
    return !result && result.error() == ndof::allocation_error::allocation_failed ? 0 : 1;
}
