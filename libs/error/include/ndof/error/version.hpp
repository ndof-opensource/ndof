// Copyright 2026 The ndof Authors
// SPDX-License-Identifier: Apache-2.0
#ifndef NDOF_ERROR_VERSION_HPP
#define NDOF_ERROR_VERSION_HPP
#include <string_view>

namespace ndof::error {

/// Name of this library as published (e.g. "ndof-error").
[[nodiscard]] std::string_view library_name() noexcept;

/// Semantic version of this library (e.g. "0.1.0").
[[nodiscard]] std::string_view library_version() noexcept;

} // namespace ndof::error

#endif // NDOF_ERROR_VERSION_HPP
