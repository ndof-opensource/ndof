// Copyright 2026 The ndof Authors
// SPDX-License-Identifier: Apache-2.0
#ifndef NDOF_CORE_STRINGS_HPP
#define NDOF_CORE_STRINGS_HPP
#include <string_view>

namespace ndof::core {

/// The whitespace set used by the trim functions: space, \t, \n, \r, \f, \v.
inline constexpr std::string_view whitespace = " \t\n\r\f\v";

/// Returns `s` with leading whitespace removed. The result is a view into the
/// same underlying storage; no allocation, no assumption of NUL termination.
[[nodiscard]] std::string_view trim_left(std::string_view s) noexcept;

/// Returns `s` with trailing whitespace removed.
[[nodiscard]] std::string_view trim_right(std::string_view s) noexcept;

/// Returns `s` with leading and trailing whitespace removed.
[[nodiscard]] std::string_view trim(std::string_view s) noexcept;

} // namespace ndof::core

#endif // NDOF_CORE_STRINGS_HPP
