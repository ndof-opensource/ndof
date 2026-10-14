// Copyright 2026 The ndof Authors
// SPDX-License-Identifier: Apache-2.0

#include "ndof/core/strings.hpp"

#include <string_view>

namespace ndof::core {

std::string_view trim_left(std::string_view s) noexcept {
    const auto first = s.find_first_not_of(whitespace);
    return first == std::string_view::npos ? std::string_view{} : s.substr(first);
}

std::string_view trim_right(std::string_view s) noexcept {
    const auto last = s.find_last_not_of(whitespace);
    return last == std::string_view::npos ? std::string_view{} : s.substr(0, last + 1);
}

std::string_view trim(std::string_view s) noexcept {
    return trim_left(trim_right(s));
}

} // namespace ndof::core
