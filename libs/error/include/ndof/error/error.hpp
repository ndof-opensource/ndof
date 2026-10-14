#ifndef NDOF_ERROR_ERROR_HPP
#define NDOF_ERROR_ERROR_HPP

#include <ndof/core/allocate_unique.hpp>
#include <ndof/core/allocator_support.hpp>
#include <ndof/core/configs.hpp>
#include <ndof/core/object.hpp>
#include <expected>
#include <memory>
#include <optional>
#include <source_location>

namespace ndof::error {

// Precedence for allocators should be:
// 1. Allocator explicitly provided to the function or type.
// 2. Allocator associated with the type T.
//    Use optional<allocator_type> to specify it as a method parameter, rather than
//    default_allocator_t.
//    TODO: Should check if use_allocator is defined: static [[nodiscard]] auto
//    use_allocator(C,R(C::*)(allocator_like))  const This will be a visitor to allow the callee to
//    to provide an allocator if needed.
// 3. Default allocator.

// TODO: basic_exception should be ICloneable, ISerializable.

using check_mode = ndof::build_mode;

// Forward declaration, needed by result_impl below.
template <typename, typename> struct basic_exception;

// Primary template, specialized below on whether exceptions are enabled, so
// that `result` can be defined as a single alias template regardless of the
// exceptions-enabled setting.
template <typename T, bool ExceptionsEnabled, typename CharT = ndof::default_char_t,
          typename Traits = ndof::default_char_traits_t<CharT>,
          typename Allocator = ndof::default_allocator_t>
struct result_impl;

// std::expected does not support reference types (or rvalue-reference
// types) as its value_type, so when T is a reference we must wrap it in
// std::reference_wrapper for the std::expected specialization below.
// Note: rvalue references (T&&) cannot be stored via std::reference_wrapper
// either, so wrapping an rvalue reference through result_t is not supported;
// callers should return by value or use a (const) lvalue reference instead.
template <typename T> struct result_value_type {
    using type = T;
};

template <typename T> struct result_value_type<T&> {
    using type = std::reference_wrapper<T>;
};

// std::reference_wrapper cannot bind to rvalue references (they refer to
// temporaries), so instead of rejecting T&& outright we store the decayed
// value type by value: the rvalue is moved into the std::expected/return
// value, which is the only sound way to propagate it.
template <typename T> struct result_value_type<T&&> {
    using type = std::decay_t<T>;
};

template <typename T> using result_value_type_t = typename result_value_type<T>::type;

template <typename T, typename Allocator>
using rebound_result_allocator_t =
    typename std::allocator_traits<Allocator>::template rebind_alloc<std::remove_cvref_t<T>>;

template <typename T, typename Allocator>
concept rebound_allocator_compatible =
    ndof::allocator_like<Allocator> &&
    requires { typename rebound_result_allocator_t<T, Allocator>; } &&
    (!std::constructible_from<std::remove_cvref_t<T>, rebound_result_allocator_t<T, Allocator>> ||
     requires {
         typename std::remove_cvref_t<T>::allocator_type;
         requires ndof::allocator_compatible_with<rebound_result_allocator_t<T, Allocator>,
                                                  typename std::remove_cvref_t<T>::allocator_type>;
     });

// When exceptions are disabled, error propagation is done via std::expected,
// carrying either the value T (or std::reference_wrapper<T> if T is a
// reference) or an ndof::exception on failure.

template <typename T, typename CharT, typename Traits, typename Allocator>
    requires rebound_allocator_compatible<T, Allocator>
struct result_impl<T, false, CharT, Traits, Allocator> {
    using expected_t =
        std::expected<result_value_type_t<T>, ndof::error::basic_exception<CharT, Traits>>;
    using rebound_allocator_t = rebound_result_allocator_t<T, Allocator>;
    // The type used for the error part of the expected, which is an allocated unique pointer to the
    // exception.
    using type =
        std::expected<expected_t, typename allocated_unique_ptr<CharT>::template rebind_alloc<
                                      error::basic_exception<CharT, Traits>>>;
};

// When exceptions are enabled, error propagation is done via throwing, so the
// return type is simply T.
// References can be returned directly because errors are reported by throwing,
// rather than being represented in a std::expected return value, which wraps references in
// std::reference_wrapper.
template <typename T, typename CharT, typename Traits, typename Allocator>
struct result_impl<T, true, CharT, Traits, Allocator> {
    using type = T;
};

template <typename T, typename CharT = ndof::default_char_t,
          typename Traits = ndof::default_char_traits_t<CharT>,
          typename Allocator = ndof::default_allocator_t>
using result_t =
    typename result_impl<T, exceptions_feature_enabled(), CharT, Traits, Allocator>::type;

template <typename Allocator = ndof::default_allocator_t>
using void_result_t = result_t<void, ndof::default_char_t,
                               ndof::default_char_traits_t<ndof::default_char_t>, Allocator>;

template <typename CharT = ndof::default_char_t,
          typename Traits = ndof::default_char_traits_t<CharT>>
struct basic_exception : std::exception {
  public:
    basic_exception() = delete;

    // TODO: Revisit these.
    basic_exception& operator=(const basic_exception&) = delete;
    basic_exception& operator=(basic_exception&&) = delete;
    virtual ~basic_exception() = 0;

    [[nodiscard]] const char* what() const noexcept override = 0;
    [[nodiscard]] const char* file_name() const noexcept;
    [[nodiscard]] const char* function_name() const noexcept;
    [[nodiscard]] std::uint_least32_t line() const noexcept;
    [[nodiscard]] std::uint_least32_t column() const noexcept;

    // TODO: Implement.
    void virtual rethrow() const = 0;

  protected:
    basic_exception(const basic_exception&) = default;
    basic_exception(basic_exception&&) = default;

  private:
    std::source_location location_;

    virtual result_t<void, CharT, Traits> to_object_impl(basic_object<CharT, Traits>& obj) = 0;

    // TODO: We need to pass the allocator.
    template <ndof::allocator_like Allocator>
    [[nodiscard]] result_t<void, CharT, Traits>
    to_object(basic_object<CharT, Traits, Allocator>& obj) const
        noexcept(!exceptions_feature_enabled()) {
        return to_object_impl(obj);
    }
};

// Type-independent (with respect to the captured exception type and allocator)
// base for inner-exception carriers. Holds the captured exception and provides
// the common behavior so that derived templates do not duplicate code.
template <typename CharT = ndof::default_char_t,
          typename Traits = ndof::default_char_traits_t<CharT>,
          ndof::allocator_like Allocator = std::allocator<CharT>>
struct basic_inner_exception : basic_exception<CharT, Traits> {
  public:
    using allocator_type = Allocator;

    basic_inner_exception() = delete;

    explicit basic_inner_exception(const std::exception_ptr& captured_exception,
                                   const Allocator& allocator = Allocator());

    basic_inner_exception(const std::exception_ptr& captured_exception,
                          const std::source_location& source_location_value,
                          const Allocator& allocator = Allocator());

    ~basic_inner_exception() override = 0;

    [[nodiscard]] const std::exception_ptr& captured_exception() const noexcept;
    [[nodiscard]] const allocator_type& get_allocator() const noexcept;
    [[nodiscard]] bool has_captured_exception() const noexcept;

    [[nodiscard]] const char* what() const noexcept override;

    void rethrow() const override;

  private:
    // TODO: figure out how to specialize for standard exceptions.
    result_t<void, CharT, Traits> to_object_impl(basic_object<CharT, Traits>& obj) override;
    std::exception_ptr captured_exception_;
    Allocator allocator_;
};

template <typename ExceptionType, typename CharT = ndof::default_char_t,
          typename Traits = ndof::default_char_traits_t<CharT>,
          ndof::allocator_like Allocator = std::allocator<CharT>>
struct basic_explicit_inner_exception : basic_inner_exception<CharT, Traits, Allocator> {

    explicit basic_explicit_inner_exception(std::exception_ptr const& captured_exception,
                                            const Allocator& allocator = Allocator());
    ~basic_explicit_inner_exception() override = default;

    template <typename ForwardedException>
        requires std::is_same_v<std::remove_cvref_t<ForwardedException>, ExceptionType>
    explicit basic_explicit_inner_exception(ForwardedException&& exception,
                                            const std::source_location& source_location_value,
                                            const Allocator& allocator = Allocator());

    // TODO: Implement.
    template <allocator_compatible_with<Allocator> OtherAllocator>
    explicit basic_explicit_inner_exception(const std::exception_ptr& captured_exception,
                                            const std::source_location& source_location_value,
                                            const OtherAllocator& allocator = OtherAllocator());

    // Fix:
    static_assert(false, "fix this.");
};

// TODO: Implement partial specializations for each standard exception.
//       In implementing them, create an inner class or something.
//       Partial specialization definitions outside of the declaration don't work apparently.

template <typename ExceptionType, typename CharT = ndof::default_char_t,
          typename Traits = ndof::default_char_traits_t<CharT>,
          typename Allocator = std::allocator<CharT>>
struct basic_condition_check_exception
    : basic_explicit_inner_exception<ExceptionType, CharT, Traits, Allocator> {
    using allocated_string = std::basic_string<CharT, Traits, Allocator>;

    basic_condition_check_exception() = delete;
    basic_condition_check_exception(const basic_condition_check_exception&) = default;
    basic_condition_check_exception(basic_condition_check_exception&&) = default;
    basic_condition_check_exception& operator=(const basic_condition_check_exception&) = default;
    basic_condition_check_exception& operator=(basic_condition_check_exception&&) = default;
    ~basic_condition_check_exception() = default;

    // TODO: Consider string_view for expression and message parameters.
    //       This would allow for more flexibility in passing string-like objects without requiring
    //       a specific string type.
    [[nodiscard]] const allocated_string& expression() const noexcept;
    [[nodiscard]] const allocated_string& message() const noexcept;
    [[nodiscard]] ndof::error::check_mode check_mode() const noexcept;
    [[nodiscard]] const char* what() const noexcept override;

  protected:
    template <ndof::allocator_like OtherAllocator>
    basic_condition_check_exception(
        const std::basic_string<CharT, Traits, OtherAllocator>& expression_value,
        const std::source_location& location_value,
        const std::basic_string<CharT, Traits, OtherAllocator>& message_value,
        ndof::error::check_mode check_mode_value, const Allocator& allocator = Allocator());

  private:
    result_t<void, CharT, Traits> to_object_impl(basic_object<CharT, Traits>& obj) override;
    allocated_string expression_;
    allocated_string message_;
    ndof::error::check_mode check_mode_;
};

template <typename ExceptionType, typename CharT = ndof::default_char_t,
          typename Traits = ndof::default_char_traits_t<CharT>,
          ndof::allocator_like Allocator = std::allocator<CharT>>
struct basic_precondition_check_exception
    : basic_condition_check_exception<ExceptionType, CharT, Traits, Allocator> {
    basic_precondition_check_exception() = default;
    basic_precondition_check_exception(const basic_precondition_check_exception&) = default;
    basic_precondition_check_exception(basic_precondition_check_exception&&) = default;
    basic_precondition_check_exception&
    operator=(const basic_precondition_check_exception&) = default;
    basic_precondition_check_exception& operator=(basic_precondition_check_exception&&) = default;
    ~basic_precondition_check_exception() = default;

    template <ndof::allocator_like OtherAllocator>
    basic_precondition_check_exception(
        const std::basic_string<CharT, Traits, OtherAllocator>& expression_value,
        const std::basic_string<CharT, Traits, OtherAllocator>& message_value,
        const std::source_location& location_value, ndof::error::check_mode check_mode_value,
        const Allocator& allocator = Allocator());

  private:
    result_t<void, CharT, Traits> to_object_impl(basic_object<CharT, Traits>& obj) override;
};

template <typename ExceptionType, typename CharT = ndof::default_char_t,
          typename Traits = ndof::default_char_traits_t<CharT>,
          ndof::allocator_like Allocator = std::allocator<CharT>>
struct basic_postcondition_check_exception
    : basic_condition_check_exception<ExceptionType, CharT, Traits, Allocator> {
    basic_postcondition_check_exception() = default;
    basic_postcondition_check_exception(const basic_postcondition_check_exception&) = default;
    basic_postcondition_check_exception(basic_postcondition_check_exception&&) = default;
    basic_postcondition_check_exception&
    operator=(const basic_postcondition_check_exception&) = default;
    basic_postcondition_check_exception& operator=(basic_postcondition_check_exception&&) = default;
    ~basic_postcondition_check_exception() = default;

    template <ndof::allocator_like OtherAllocator>
    basic_postcondition_check_exception(
        const std::basic_string<CharT, Traits, OtherAllocator>& expression_value,
        const std::basic_string<CharT, Traits, OtherAllocator>& message_value,
        const std::source_location& location_value, ndof::error::check_mode check_mode_value,
        const Allocator& allocator = Allocator());

  private:
    result_t<void, CharT, Traits> to_object_impl(basic_object<CharT, Traits>& obj) override;
};

template <typename ExceptionType, typename CharT = ndof::default_char_t,
          typename Traits = ndof::default_char_traits_t<CharT>,
          ndof::allocator_like Allocator = std::allocator<CharT>>
struct basic_invariant_condition_check_exception
    : basic_condition_check_exception<ExceptionType, CharT, Traits, Allocator> {
    basic_invariant_condition_check_exception() = default;
    basic_invariant_condition_check_exception(const basic_invariant_condition_check_exception&) =
        default;
    basic_invariant_condition_check_exception(basic_invariant_condition_check_exception&&) =
        default;
    basic_invariant_condition_check_exception&
    operator=(const basic_invariant_condition_check_exception&) = default;
    basic_invariant_condition_check_exception&
    operator=(basic_invariant_condition_check_exception&&) = default;
    ~basic_invariant_condition_check_exception() = default;

    template <ndof::allocator_like OtherAllocator>
    basic_invariant_condition_check_exception(
        const std::basic_string<CharT, Traits, OtherAllocator>& expression_value,
        const std::basic_string<CharT, Traits, OtherAllocator>& message_value,
        const std::source_location& location_value, ndof::error::check_mode check_mode_value,
        const Allocator& allocator = Allocator());

  private:
    result_t<void, CharT, Traits> to_object_impl(basic_object<CharT, Traits>& obj) override;
};

// TODO: Discuss.  The behavior will change if the exception type passed in is not_allocator_aware.
// TODO: Consider making the default allocator factory a singleton using the singleton template.
template <typename ExceptionType, typename CharT = ndof::default_char_t,
          typename Traits = ndof::default_char_traits_t<CharT>,
          // TODO: Fix this.
          ndof::allocator_like Allocator = default_allocator_t>
    requires(exceptions_feature_enabled() &&
             std::derived_from<std::remove_cvref_t<ExceptionType>,
                               ndof::error::basic_exception<CharT, Traits>> &&
             !ndof::allocator_aware<std::remove_cvref_t<ExceptionType>>)
[[nodiscard]] auto generate_or_throw_exception(
    ExceptionType&& exception,
    std::source_location source_location_value = std::source_location::current(),
    const Allocator& allocator = std::allocator<char>()) {

    using captured_exception_type = std::remove_cvref_t<ExceptionType>;
    return basic_explicit_inner_exception<captured_exception_type, CharT, Traits, Allocator>(
        std::forward<ExceptionType>(exception), source_location_value, allocator);
}

// Question: Should the behavior change based on the type of exception passed in, i.e.,
//           if it is allocator_aware or not?
//           Here, if no default is specified, the allocator is taken from the exception.
//           In other cases, it will be taken from the default allocator factory.
//           In all cases, if an allocator is passed in, it will be used.
//           This is the most flexible approach, but it may be confusing to users.
// Answer:   TBD
// TODO: Update this to use the default allocator factory for the default type.
template <
    typename ExceptionType, typename CharT = ndof::default_char_t,
    typename Traits = ndof::default_char_traits_t<CharT>,
    ndof::allocator_compatible_with_get_allocator<ExceptionType> Allocator = std::allocator<CharT>>
    requires(exceptions_feature_enabled() &&
             std::derived_from<std::remove_cvref_t<ExceptionType>,
                               ndof::error::basic_exception<CharT, Traits>> &&
             ndof::allocator_aware<std::remove_cvref_t<ExceptionType>>)
[[nodiscard]] auto generate_or_throw_exception(
    ExceptionType&& exception,
    std::source_location source_location_value = std::source_location::current(),
    std::optional<Allocator>& allocator = std::nullopt) {
    using allocator_type = Allocator;
    using captured_exception_type = std::remove_cvref_t<ExceptionType>;
    return basic_explicit_inner_exception<captured_exception_type, CharT, Traits, allocator_type>(
        std::forward<ExceptionType>(exception), source_location_value,
        allocator.value_or(exception.get_allocator()));
}

    // TODO: Reevaluate these functions.
    // Exception is not derived from basic_exception, and is not allocator aware,
    //  so we use the default allocator factory to get an allocator
    //  or the one passed in.
    // TODO: Update this one to find the default allocator factory.
    //       The default factory will have to be compatible with the allocator type of the exception.
    //       We can check this by using the allocator_compatible_with concept.
    //       If it is not compatible, we will reject the call.

    // TODO: Revisit the value_or part.  maybe allocator shouldn't be a default.
    //       We should maybe split these into two functions, so we can determine whether
    // template <typename ExceptionType, typename CharT = ndof::default_char_t,
    //         typename Traits = ndof::default_char_traits_t<CharT>,
    //         typename Allocator = ndof::default_allocator_t<CharT>>
    //     requires(exceptions_feature_enabled() &&
    //             !std::derived_from<std::remove_cvref_t<ExceptionType>,
    //                                 ndof::error::basic_exception<CharT, Traits>> &&
    //             !ndof::allocator_aware<std::remove_cvref_t<ExceptionType>>)
    // void generate_or_throw_exception(
    //     ExceptionType&& exception,
    //     [[maybe_unused]] std::source_location source_location_value = std::source_location::current(),
    //     [[maybe_unused]] std::optional<Allocator> allocator = std::nullopt) {
    //     if (allocator.has_value()) {
    //         using allocator_type = Allocator;
    //         using captured_exception_type = std::remove_cvref_t<ExceptionType>;
    //         throw basic_explicit_inner_exception<captured_exception_type, CharT, Traits,
    //                                             allocator_type>(
    //             std::forward<ExceptionType>(exception), source_location_value, allocator.value());
    //     }
    //     using allocator_type = ndof::default_allocator_t;
    //     throw basic_explicit_inner_exception<std::remove_cvref_t<ExceptionType>, CharT, Traits,
    //                                         allocator_type>(std::forward<ExceptionType>(exception),
    //                                                         source_location_value,
    //                                                         ndof::get_default_allocator());
    // }

    // TODO: Re-evaluate the necessity of this function.
    // // Exception is not derived from basic_exception, but is allocator aware,
    // // and no allocator is passed in.
    // // TODO: fetch the default allocator from exception if none is provided.
    // template <typename ExceptionType, typename CharT = ndof::default_char_t,
    //         typename Traits = ndof::default_char_traits_t<CharT>,
    //         ndof::allocator_compatible_with_get_allocator<ExceptionType> Allocator =
    //             get_allocator_result_type_t<ExceptionType>>
    //     requires(exceptions_feature_enabled() &&
    //             !std::derived_from<std::remove_cvref_t<ExceptionType>,
    //                                 ndof::error::basic_exception<CharT, Traits>>)
    // void generate_or_throw_exception(
    //     ExceptionType&& exception,
    //     std::source_location source_location_value = std::source_location::current(),
    //     std::optional<Allocator> allocator = std::nullopt) {
    //     using captured_exception_type = std::remove_cvref_t<ExceptionType>;

    //     if (allocator.has_value()) {
    //         throw basic_explicit_inner_exception<captured_exception_type, CharT, Traits,
    //                                             default_allocator_t>(
    //             std::forward<ExceptionType>(exception), source_location_value, allocator.value());
    //     } else {
    //         using allocator_type = decltype(exception.get_allocator());
    //         throw basic_explicit_inner_exception<captured_exception_type, CharT, Traits,
    //                                             allocator_type>(std::forward<ExceptionType>(exception),
    //                                                             source_location_value,
    //                                                             exception.get_allocator());
    //     }
    // }


} // namespace ndof::error
#endif
