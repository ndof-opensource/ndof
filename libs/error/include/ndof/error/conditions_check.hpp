#ifndef NDOF_ERROR_CONDITIONS_CHECK_HPP
#define NDOF_ERROR_CONDITIONS_CHECK_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

// TODO: This is the first pass.  I will adapt this to the errors defined in error.hpp
//       All methods must be compatible with result_t defined in core.
//       Any try/catch blocks must be conditionally compiled based on NDOF_EXCEPTIONS_ENABLED.

namespace ndof {
namespace detail {
template <typename T> struct callable_signature;

template <typename Result, typename Type, typename... Args>
struct callable_signature<Result (Type::*)(Args...) const> {
    using type = bool(Args...);
};

template <typename Result, typename Type, typename... Args>
struct callable_signature<Result (Type::*)(Args...)> {
    using type = bool(Args...);
};

template <typename Result, typename... Args> struct callable_signature<Result (*)(Args...)> {
    using type = bool(Args...);
};

template <typename Callable>
using callable_signature_t =
    typename callable_signature<decltype(&std::remove_reference_t<Callable>::operator())>::type;
} // namespace detail

struct precondition_check_error : std::logic_error {
    precondition_check_error(std::size_t index, const std::string& expression,
                             std::source_location location)
        : std::logic_error("precondition check failed: " + expression), index_(index),
          expression_(expression), location_(location) {}

    [[nodiscard]] std::size_t index() const noexcept { return index_; }
    [[nodiscard]] const std::string& expression() const noexcept { return expression_; }
    [[nodiscard]] const std::source_location& location() const noexcept { return location_; }

  private:
    std::size_t index_;
    std::string expression_;
    std::source_location location_;
};

struct postcondition_check_error : std::logic_error {
    postcondition_check_error(std::size_t index, const std::string& expression,
                              std::source_location location)
        : std::logic_error("postcondition check failed: " + expression), index_(index),
          expression_(expression), location_(location) {}

    [[nodiscard]] std::size_t index() const noexcept { return index_; }
    [[nodiscard]] const std::string& expression() const noexcept { return expression_; }
    [[nodiscard]] const std::source_location& location() const noexcept { return location_; }

  private:
    std::size_t index_;
    std::string expression_;
    std::source_location location_;
};

template <typename Signature> struct check;

template <typename... Args> struct check<bool(Args...)> {
    template <typename Callable>
        requires std::predicate<Callable&, Args...>
    check(const std::string& expression, Callable&& callable,
          std::source_location location = std::source_location::current())
        : expression_(expression), location_(location),
          callable_(std::forward<Callable>(callable)) {}

    [[nodiscard]] bool operator()(Args&&... args) const {
        return std::invoke(callable_, std::forward<Args>(args)...);
    }

    [[nodiscard]] const std::string& expression() const noexcept { return expression_; }
    [[nodiscard]] const std::source_location& location() const noexcept { return location_; }

  private:
    std::string expression_;
    std::source_location location_;
    // TODO: Replace this with an type-erased functor.  Use the allocated_unique_ptr.
    std::function<bool(Args...)> callable_;
};

template <typename Callable>
check(const std::string&, Callable&&) -> check<detail::callable_signature_t<Callable>>;

template <typename Callable>
check(const std::string&, Callable&&, std::source_location)
    -> check<detail::callable_signature_t<Callable>>;

template <typename Result, typename... Args>
check(const std::string&, Result (*)(Args...)) -> check<bool(Args...)>;

template <typename Result, typename... Args>
check(const std::string&, Result (*)(Args...), std::source_location) -> check<bool(Args...)>;

template <typename Result, std::size_t N> struct postcondition_check_builder {
    explicit postcondition_check_builder(std::array<check<bool(Result)>, N> checks)
        : checks_(std::move(checks)) {}

    template <typename Body>
        requires std::invocable<Body&> && std::convertible_to<std::invoke_result_t<Body&>, Result>
    [[nodiscard]] Result body(Body&& body_callable) const {
        Result result = std::invoke(std::forward<Body>(body_callable));
        std::size_t index = 0;
        for (const auto& condition : checks_) {
            if (!condition(result)) {
                throw postcondition_check_error(index, condition.expression(),
                                                condition.location());
            }
            ++index;
        }
        return result;
    }

  private:
    std::array<check<bool(Result)>, N> checks_;
};

struct precondition_check_builder {
    template <typename Result, std::size_t N>
    [[nodiscard]] postcondition_check_builder<Result, N>
    postcondition_check(check<bool(Result)> (&&checks)[N]) const {
        return postcondition_check_builder<Result, N>{std::to_array(std::move(checks))};
    }

    template <typename Body>
        requires std::invocable<Body&>
    [[nodiscard]] decltype(auto) body(Body&& body_callable) const {
        return std::invoke(std::forward<Body>(body_callable));
    }
};

[[nodiscard]] inline precondition_check_builder
precondition_check(std::initializer_list<check<bool()>> checks) {
    std::size_t index = 0;
    for (const auto& condition : checks) {
        if (!condition()) {
            throw precondition_check_error(index, condition.expression(), condition.location());
        }
        ++index;
    }
    return precondition_check_builder{};
}

} // namespace ndof

#endif // NDOF_ERROR_CONDITIONS_CHECK_HPP
