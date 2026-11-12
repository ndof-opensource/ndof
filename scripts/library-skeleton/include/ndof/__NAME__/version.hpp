// Copyright 2026 The ndof Authors
// SPDX-License-Identifier: Apache-2.0
#ifndef NDOF___NAME_UPPER___VERSION_HPP
#define NDOF___NAME_UPPER___VERSION_HPP
#include <string_view>

namespace ndof::__NAME__ {

/// Name of this library as published (e.g. "ndof-__NAME__").
[[nodiscard]] std::string_view library_name() noexcept;

/// Semantic version of this library (e.g. "0.1.0").
[[nodiscard]] std::string_view library_version() noexcept;

} // namespace ndof::__NAME__

#endif // NDOF___NAME_UPPER___VERSION_HPP
