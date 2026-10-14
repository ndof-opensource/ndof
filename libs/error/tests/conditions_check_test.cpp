#include "ndof/error/conditions_check.hpp"

#include <gtest/gtest.h>

#include <concepts>
#include <memory>
#include <string>

namespace {

TEST(ConditionsCheck, RunsBodyBetweenPreconditionsAndPostconditions) {
    std::string order;

    auto builder = ndof::precondition_check({
        ndof::check{"first precondition", [&] {
            order += "pre1 ";
            return true;
        }},
        ndof::check{"second precondition", [&] {
            order += "pre2 ";
            return true;
        }},
    }).postcondition_check({
        ndof::check{"result equals 42", [&](int value) {
            order += "post1 ";
            return value == 42;
        }},
        ndof::check{"result is positive", [&](int value) {
            order += "post2";
            return value > 0;
        }},
    });

    static_assert(std::same_as<decltype(builder), ndof::postcondition_check_builder<int, 2>>);

    const auto result = builder.body([&] {
        order += "body ";
        return 42;
    });

    EXPECT_EQ(result, 42);
    EXPECT_EQ(order, "pre1 pre2 body post1 post2");
}

TEST(ConditionsCheck, FailedPreconditionPreventsBodyExecution) {
    bool body_called = false;

    try {
        static_cast<void>(ndof::precondition_check({
            ndof::check{"first condition", [] { return true; }},
            ndof::check{"input is valid", [] { return false; }},
        }).body([&] { body_called = true; }));
        FAIL() << "Expected a precondition_check_error";
    } catch (const ndof::precondition_check_error& error) {
        EXPECT_EQ(error.index(), 1U);
        EXPECT_EQ(error.expression(), "input is valid");
        EXPECT_STREQ(error.what(), "precondition check failed: input is valid");
        EXPECT_STREQ(error.location().file_name(), __FILE__);
        EXPECT_GT(error.location().line(), 0U);
    }

    EXPECT_FALSE(body_called);
}

TEST(ConditionsCheck, FailedPostconditionReportsItsIndex) {
    try {
        static_cast<void>(ndof::precondition_check({
            ndof::check{"precondition passes", [] { return true; }},
        }).postcondition_check({
            ndof::check{"first result condition", [](int) { return true; }},
            ndof::check{"result is acceptable", [](int) { return false; }},
        }).body([] { return 42; }));
        FAIL() << "Expected a postcondition_check_error";
    } catch (const ndof::postcondition_check_error& error) {
        EXPECT_EQ(error.index(), 1U);
        EXPECT_EQ(error.expression(), "result is acceptable");
        EXPECT_STREQ(error.what(), "postcondition check failed: result is acceptable");
        EXPECT_STREQ(error.location().file_name(), __FILE__);
        EXPECT_GT(error.location().line(), 0U);
    }
}

TEST(ConditionsCheck, OwnsMoveOnlyCallableWithoutHeapFallback) {
    auto value = std::make_unique<int>(42);

    static_cast<void>(ndof::precondition_check({
        ndof::check{"captured value equals 42", [captured = std::move(value)] {
            return *captured == 42;
        }},
    }));

    EXPECT_EQ(value, nullptr);
}

static_assert(ndof::detail::erased_predicate_capacity == 2 * sizeof(void*));

}  // namespace