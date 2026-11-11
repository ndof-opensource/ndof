// Copyright 2026 The ndof Authors
// SPDX-License-Identifier: Apache-2.0

#include "ndof/__NAME__/version.hpp"

#include <gtest/gtest.h>

namespace {

TEST(Version, LibraryNameMatchesPackage) {
    EXPECT_EQ(ndof::__NAME__::library_name(), "ndof-__NAME__");
}

TEST(Version, LibraryVersionIsNonEmpty) {
    EXPECT_FALSE(ndof::__NAME__::library_version().empty());
}

} // namespace
