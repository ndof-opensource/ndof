// Copyright 2026 The ndof Authors
// SPDX-License-Identifier: Apache-2.0
#ifndef NDOF_CALLABLE_VERSION_HPP
#define NDOF_CALLABLE_VERSION_HPP
#include <string_view>

namespace ndof::callable {

/// Name of this library as published (e.g. "ndof-callable").
[[nodiscard]] std::string_view library_name() noexcept;

/// Semantic version of this library (e.g. "0.1.0").
[[nodiscard]] std::string_view library_version() noexcept;

} // namespace ndof::callable

#endif // NDOF_CALLABLE_VERSION_HPP
