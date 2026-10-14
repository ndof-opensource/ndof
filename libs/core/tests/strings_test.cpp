// Copyright 2026 The ndof Authors
// SPDX-License-Identifier: Apache-2.0

#include "ndof/core/strings.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using ndof::core::trim;
using ndof::core::trim_left;
using ndof::core::trim_right;

TEST(Trim, EmptyInput) {
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim_left(""), "");
    EXPECT_EQ(trim_right(""), "");
}

TEST(Trim, AllWhitespace) {
    EXPECT_EQ(trim(" \t\n\r\f\v"), "");
    EXPECT_EQ(trim_left(" \t"), "");
    EXPECT_EQ(trim_right("\n\n"), "");
}

TEST(Trim, NoWhitespace) {
    EXPECT_EQ(trim("abc"), "abc");
}

TEST(Trim, LeadingAndTrailing) {
    EXPECT_EQ(trim("  abc\t"), "abc");
    EXPECT_EQ(trim_left("  abc  "), "abc  ");
    EXPECT_EQ(trim_right("  abc  "), "  abc");
}

TEST(Trim, InteriorWhitespacePreserved) {
    EXPECT_EQ(trim(" a b\tc "), "a b\tc");
}

// The complete whitespace set is trimmed, including \f and \v.
TEST(Trim, FormFeedAndVerticalTab) {
    EXPECT_EQ(trim("\f\vabc\v\f"), "abc");
}

// Views into non-NUL-terminated storage are handled purely by length —
// nothing outside the view is read or considered.
TEST(Trim, NonNulTerminatedSubstringView) {
    const std::string storage = "xx  abc  yy";
    const std::string_view middle = std::string_view{storage}.substr(2, 7); // "  abc  "
    EXPECT_EQ(trim(middle), "abc");
}

// The result aliases the input's storage; no copy is made.
TEST(Trim, ResultAliasesInput) {
    const std::string storage = "  abc  ";
    const std::string_view expected = std::string_view{storage}.substr(2, 3);
    const std::string_view trimmed = trim(storage);
    EXPECT_EQ(static_cast<const void*>(trimmed.data()), static_cast<const void*>(expected.data()));
    EXPECT_EQ(trimmed.size(), expected.size());
}

} // namespace
