// Copyright 2026 The ndof Authors
// SPDX-License-Identifier: Apache-2.0
#ifndef NDOF_CORE_CONFIGS_HPP
#define NDOF_CORE_CONFIGS_HPP
// TODO: Check the character set that was used to compile the library and provide a way to query it at runtime.
//       We can use this setting as the default character set for the library and provide a way to override it at runtime if necessary.
//       Probably by defining ndof::object to be ndof::basic_object<ndof::default_char_type> and providing a way to change the default_char_type at runtime.
//       Same with ndof::exception and ndof::basic_exception<ndof::default_char_type>.

#include <string_view>
#include <cstdint>
#include <typeindex>
#include <expected> 

// TODO: Come up with a clearer strategy of dependendies here.

// Captures a boolean test value together with the original expression text.
// Example: auto [passed, name] = NDOF_TEST(x > 0);
#define NDOF_TEST(test_expression) static_cast<bool>(test_expression), #test_expression

#if !defined(NDOF_AVOID_IMPLICIT_HEAP_ALLOCATION) 
#define NDOF_AVOID_IMPLICIT_HEAP_ALLOCATION 0
#endif

// If this definition has not been set by the user, 
// we provide a default value based on the heap allocation setting.
#if !defined(NDOF_EXPECTED_RETURN_PREFERRED) 
#define NDOF_EXPECTED_RETURN_PREFERRED NDOF_AVOID_IMPLICIT_HEAP_ALLOCATION
#endif

#if !defined(NDOF_RTTI_FEATURE_ENABLED)
//-fno-rtti (GCC/Clang) or /GR- (MSVC
#if defined(__cpp_rtti)        || (defined(_CPPRTTI)  && defined(_MSC_VER)) \
        || ((defined(__GNUC__) || defined(__clang__)) && defined(__GXX_RTTI))
#define NDOF_RTTI_FEATURE_ENABLED 1
#else
#define NDOF_RTTI_FEATURE_ENABLED 0
#endif
#endif

// Determine whether C++ exceptions are enabled based on standard compiler switches.
// Since exceptions may allocate on the heap, we disable them if implicit heap allocation is avoided.
#if !defined(NDOF_EXCEPTIONS_FEATURE_ENABLED)
#if (defined(__cpp_exceptions) || (defined(_CPPUNWIND) && defined(_MSC_VER)))
#define NDOF_EXCEPTIONS_FEATURE_ENABLED 1
#else
#define NDOF_EXCEPTIONS_FEATURE_ENABLED 0
#endif
#endif

// Determine whether C++ thread support is enabled. __STDCPP_THREADS__ is the
// standard library feature macro; the remaining checks cover common standard
// library/compiler configurations that do not expose it.
// If the heap is to be avoided, we also disable thread support.
#if !defined(NDOF_THREADS_FEATURE_ENABLED)
#if   ((defined(__STDCPP_THREADS__) && __STDCPP_THREADS__ == 1) \
    || (defined(_MSC_VER) && defined(_MT)) \
    || (defined(_GLIBCXX_HAS_GTHREADS) && _GLIBCXX_HAS_GTHREADS) \
    ||  defined(__GTHREADS) \
    ||  defined(_LIBCPP_HAS_THREAD_API_PTHREAD) \
    ||  defined(_LIBCPP_HAS_THREAD_API_WIN32)) \
    && NDOF_AVOID_IMPLICIT_HEAP_ALLOCATION == 0
#define NDOF_THREADS_FEATURE_ENABLED 1
#else
#define NDOF_THREADS_FEATURE_ENABLED 0
#endif
#endif

namespace ndof {
consteval bool avoid_implicit_heap_allocation() noexcept {
    return NDOF_AVOID_IMPLICIT_HEAP_ALLOCATION == 1;
}

[[nodiscard]] consteval bool expected_return_preferred() noexcept {
    return NDOF_EXPECTED_RETURN_PREFERRED == 1;
}

[[nodiscard]] consteval bool exceptions_feature_enabled() noexcept {
    return NDOF_EXCEPTIONS_FEATURE_ENABLED == 1;
}


#if defined(NDOF_RTTI_FEATURE_ENABLED) && NDOF_RTTI_FEATURE_ENABLED == 1
using type_token = std::type_index;
#else
#include "ndof/core/type_index.hpp"
using type_token = ndof_type_index;
#endif


template<typename T>
[[nodiscard]] constexpr type_token type_token_of() noexcept {
#if defined(NDOF_RTTI_FEATURE_ENABLED) && NDOF_RTTI_FEATURE_ENABLED == 1
    return std::type_index(typeid(T));
#else
    return ndof_type_index::for_type<T>();
#endif
}

[[nodiscard]] consteval bool rtti_feature_enabled() noexcept {
    return NDOF_RTTI_FEATURE_ENABLED == 1;
}

[[nodiscard]] consteval bool threads_feature_enabled() noexcept {
    return (NDOF_THREADS_FEATURE_ENABLED == 1) && !avoid_implicit_heap_allocation();
}

[[nodiscard]] consteval bool apply_except_policy(bool expression) noexcept {
    if constexpr (exceptions_feature_enabled()) {
        return expression;
    } else {
        return false;
    }
}

// Determine the default character type based on standard compiler switches / the standard.
// - On Windows with _UNICODE (or UNICODE) defined, wchar_t is the conventional default.
// - If the compiler supports char8_t (C++20, __cpp_char8_t), prefer it as the "native" UTF-8 char type.
// - Otherwise fall back to plain char.
#ifndef NDOF_DEFAULT_CHAR_TYPE
#if defined(_WIN32) && (defined(_UNICODE) || defined(UNICODE))
#define NDOF_DEFAULT_CHAR_TYPE wchar_t
#elif defined(__cpp_char8_t)
#define NDOF_DEFAULT_CHAR_TYPE char8_t
#else
#define NDOF_DEFAULT_CHAR_TYPE char
#endif
#endif

using default_char_t = NDOF_DEFAULT_CHAR_TYPE;

#ifndef NDOF_DEFAULT_CHAR_TRAITS_TYPE
#define NDOF_DEFAULT_CHAR_TRAITS_TYPE std::char_traits 
#endif 

template<typename CharT, template<typename> class Traits = NDOF_DEFAULT_CHAR_TRAITS_TYPE>
using default_char_traits_t = Traits<CharT>;

enum class build_mode : std::uint8_t {
    undefined,
    debug,
    release
};
 
enum class type_index_mode : std::uint8_t {
    undefined,
    ndof_type_index,
    rtti_type_index
};

// Determine the build mode based on standard compiler switches.
// - NDEBUG defined typically indicates a release build (assert() is a no-op).
// - Otherwise, treat it as a debug build.
#ifndef NDOF_BUILD_MODE
#if defined(NDEBUG)
#define NDOF_BUILD_MODE ::ndof::build_mode::release
#else
#define NDOF_BUILD_MODE ::ndof::build_mode::debug
#endif
#endif

[[nodiscard]] consteval ndof::build_mode get_build_mode() noexcept {
    return NDOF_BUILD_MODE;
}

using default_string_view = std::basic_string_view<ndof::default_char_t, default_char_traits_t<default_char_t>>;

// Note: This is used to prepend 'L' for unicode or wchar string literals on Windows, and 'u8' for UTF-8 string literals on compilers that support char8_t. 
//       Otherwise, it defaults to a regular string literal.
#ifndef NDOF_STR
#if defined(_WIN32) && (defined(_UNICODE) || defined(UNICODE))
#define NDOF_STR(s) L##s
#elif defined(__cpp_char8_t)
#define NDOF_STR(s) u8##s
#else
#define NDOF_STR(s) s
#endif
#endif


[[nodiscard]] consteval default_string_view get_build_mode_name() noexcept {
    switch (get_build_mode()) {
        case build_mode::debug:
            return default_string_view{NDOF_STR("debug")};
        case build_mode::release:
            return default_string_view{NDOF_STR("release")};
        case build_mode::undefined:
        default:
            return default_string_view{NDOF_STR("undefined")};
    }
}

[[nodiscard]] consteval ndof::type_index_mode get_type_index_mode() noexcept {
    if constexpr (rtti_feature_enabled()) {
        return type_index_mode::rtti_type_index;
    }
    return type_index_mode::ndof_type_index;
}

[[nodiscard]] consteval default_string_view get_type_index_mode_name() noexcept {
    switch (get_type_index_mode()) {
        case type_index_mode::rtti_type_index:
            return default_string_view{NDOF_STR("rtti_type_index")};
        case type_index_mode::ndof_type_index:
        default:
            return default_string_view{NDOF_STR("ndof_type_index")};
    }
}

} // namespace ndof

#endif // NDOF_CORE_CONFIGS_HPP
