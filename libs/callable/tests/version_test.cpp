// Copyright 2026 The ndof Authors
// SPDX-License-Identifier: Apache-2.0

#include "ndof/callable/version.hpp"

#include <gtest/gtest.h>

namespace {

TEST(Version, LibraryNameMatchesPackage) {
    EXPECT_EQ(ndof::callable::library_name(), "ndof-callable");
}

TEST(Version, LibraryVersionIsNonEmpty) {
    EXPECT_FALSE(ndof::callable::library_version().empty());
}

} // namespace
