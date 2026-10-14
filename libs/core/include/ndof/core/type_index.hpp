// Copyright 2026 The ndof Authors
// SPDX-License-Identifier: Apache-2.0
#ifndef NDOF_CORE_TYPE_INDEX_HPP
#define NDOF_CORE_TYPE_INDEX_HPP

#include <cstddef>
#include <functional>
#include <type_traits>

namespace ndof {

namespace detail {

template<typename T>
struct ndof_type_tag {
    // Note:
    // Since C++17, static constexpr data members are implicitly `inline`,
    // so this has a single unique address across all translation units,
    // making it safe to use as a per-type identity marker.
    //
    // Taking `&value` (as done on line 39) odr-uses it, forcing the
    // compiler/linker to actually allocate storage for it, even though its
    // value is never read. Because it is `inline`, every translation unit
    // that odr-uses it refers to the same merged definition, so the address
    // is guaranteed to be identical everywhere -- that merged address is
    // what gets stored as the `ndof_type_index` value.
    static constexpr char value = 0;
};

} // namespace detail

struct ndof_type_index {
    using value_type = const void*;

    constexpr ndof_type_index() noexcept = default;

    // Note: This uses the address of a static constexpr variable to uniquely identify the type at runtime.
    template<typename T>
    [[nodiscard]] static ndof_type_index for_type() noexcept {
        using normalized_type = std::remove_cvref_t<T>;
        // Note: This is okay. Taking the address of a static constexpr variable is well-defined and yields a unique pointer for each type.
        return ndof_type_index(static_cast<value_type>(&detail::ndof_type_tag<normalized_type>::value));
    }

    [[nodiscard]] constexpr value_type value() const noexcept {
        return value_;
    }

    friend constexpr bool operator==(ndof_type_index lhs, ndof_type_index rhs) noexcept = default;
    friend constexpr auto operator<=>(ndof_type_index lhs, ndof_type_index rhs) noexcept = default;

private:
    value_type value_ = nullptr;
    explicit constexpr ndof_type_index(value_type value) noexcept
        : value_(value) {
    }
};

} // namespace ndof

namespace std {

template<>
struct hash<ndof::ndof_type_index> {
    // Note on collisions:
    // Each distinct type has its own `ndof_type_tag<T>::value` with a unique
    // address, so distinct types always yield distinct pointer values here.
    // Any hash collisions that occur are therefore solely a property of
    // `std::hash<const void*>` (e.g. truncation to `std::size_t`, or the
    // standard library's chosen hash function for pointers), not of this
    // type-index scheme itself. Callers relying on hashing (e.g. unordered
    // containers) still must handle such collisions via equality comparison,
    // which `ndof_type_index::operator==` provides and is collision-free.
    std::size_t operator()(ndof::ndof_type_index index) const noexcept {
        return std::hash<ndof::ndof_type_index::value_type>{}(index.value());
    }
};

} // namespace std

#endif // NDOF_CORE_TYPE_INDEX_HPP
