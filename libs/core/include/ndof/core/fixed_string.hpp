#ifndef NDOF_CORE_FIXED_STRING_HPP
#define NDOF_CORE_FIXED_STRING_HPP
#include <cstddef>
#include <array>
#include <string_view>
#include <algorithm>
#include <type_traits>

namespace ndof {

    // TODO: Replace with one_of and put this in core.
    template <typename T> 
    concept character_type = requires(T) {
            std::is_same_v<std::remove_cv_t<T>, char> ||
            std::is_same_v<std::remove_cv_t<T>, wchar_t> ||
            std::is_same_v<std::remove_cv_t<T>, char16_t> ||
            std::is_same_v<std::remove_cv_t<T>, char32_t>;
    };

    template <character_type CharT, std::size_t N>
    struct fixed_string  {
    private:
        std::array<CharT, N> data;

    public:
        using char_type = CharT;
        constexpr fixed_string(const CharT (&str)[N])  {
            std::copy_n(str, N, data.begin());
        }

        [[nodiscard]] constexpr std::basic_string_view<CharT> view() const {
            return std::basic_string_view<CharT>(data.data(), N);
        }

        [[nodiscard]] constexpr const CharT* c_str() const {
            return data.data();
        }

        [[nodiscard]] constexpr std::size_t length() const {
            return N;
        }
    };
    
} // namespace ndof

#endif // NDOF_CORE_FIXED_STRING_HPP

