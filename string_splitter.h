#pragma once

#include <string>
#include <array>
#include <vector>
#include <emmintrin.h>

namespace msu
{
    class StringPiece final
    {
    public:
        // Construct / copy
        constexpr StringPiece() noexcept
            : _begin(nullptr), _len(0) {}
        StringPiece(const char* b, size_t len) noexcept
            : _begin(b), _len(len) {}

        // Default copy/move
        constexpr StringPiece(const StringPiece& other) noexcept = default;
        StringPiece& operator=(const StringPiece& other) noexcept = default;

        const char& operator[](size_t i) const { return _begin[i]; }
        operator const char*() const { return _begin; }

        constexpr size_t size() const noexcept { return _len; }
        constexpr bool empty() const noexcept { return _len == 0; }
        std::string str() const { return std::string(_begin, _len); }
        constexpr const char* begin() const noexcept { return _begin; }
        constexpr const char* end() const noexcept { return _begin + _len; }

    private:
        const char* const _begin;
        const size_t _len;
    };

    namespace details
    {
        // Some template code to compare integral constant to 0
        enum class enabler_t { EMPTY };
        template <size_t T>
        struct is_empty { static const bool value = (T == 0); };

#define enable_if_eq_zero(val)        typename std::enable_if<std::integral_constant<bool, is_empty<val>::value>::value, enabler_t>::type = enabler_t::EMPTY
#define enable_if_not_eq_zero(val)    typename std::enable_if<!std::integral_constant<bool, is_empty<val>::value>::value, enabler_t>::type = enabler_t::EMPTY

#ifdef _MSC_VER
        constexpr __m128i build(char ch) { return { ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch }; }
#else
        typedef char __attribute__((vector_size(16 * sizeof(char)))) V16;
        constexpr __m128i build(char ch) { return (__m128i)((V16) { ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch }); }
#endif

        // Array helpers
        template<typename T, std::size_t S, T... Args>
        constexpr std::array<T, S> make_array()
        {
            return std::array<T, S> { { Args... } };
        }

        template <char... Args, typename F, std::size_t I1, std::size_t... I, enable_if_eq_zero(sizeof...(I))>
        void const_foreach_itr(F&& f, const std::index_sequence<I1, I...>& i)
        {
            constexpr auto tempChars = make_array<char, sizeof...(Args), Args...>();
            constexpr auto ch = std::get<I1>(tempChars);
            constexpr __m128i val = build(ch);

            f(std::forward<const __m128i>(val), std::bool_constant<I1 == 0>());
            (void)(i);
        }

        template <char... Args, typename F, std::size_t I1, std::size_t... I, enable_if_not_eq_zero(sizeof...(I))>
        void const_foreach_itr(F&& f, const std::index_sequence<I1, I...>& i)
        {
            constexpr auto tempChars = make_array<char, sizeof...(Args), Args...>();
            constexpr auto ch = std::get<I1>(tempChars);
            constexpr __m128i val = build(ch);

            f(std::forward<const __m128i>(val), std::bool_constant<I1 == 0>());
            const_foreach_itr<Args...>(f, std::index_sequence<I...>());
            (void)(i);
        }

        template <char... Args, typename F>
        void const_foreach(F&& f)
        {
            const_foreach_itr<Args...>(f, std::make_index_sequence<sizeof...(Args)>());
        }
    }

    template<char... Args>
    void split(std::vector<msu::StringPiece>& vec, const std::string& str)
    {
        const char* const ptr = str.c_str();
        const intptr_t size = static_cast<intptr_t>(str.size());
        intptr_t charspassed = 0;
        intptr_t tokenStartPos = 0;

        do
        {
            const __m128i xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + charspassed));

            __m128i mask;
            details::const_foreach<Args...>([&](const __m128i cmpVal, const bool first)
            {
                if (first)
                    mask = _mm_cmpeq_epi8(xmm1, cmpVal);
                else
                    mask = _mm_or_si128(mask, _mm_cmpeq_epi8(xmm1, cmpVal));
            });

            intptr_t tokenStopPos = 0;
            intptr_t movemask = _mm_movemask_epi8(mask);
            while (movemask != 0)
            {
                // Get next offset from bitmask
#ifdef _MSC_VER
                unsigned long temp;
                _BitScanForward(&temp, movemask);
                const auto offset = static_cast<intptr_t>(temp);
#else
                const auto offset = __builtin_ffs(movemask) - 1;
#endif

                // Insert stringpiece into vector
                vec.emplace_back(ptr + tokenStartPos, offset - tokenStopPos);

                tokenStopPos = offset + 1;
                tokenStartPos = charspassed + tokenStopPos;

                // Remove this offset from bitmask
                movemask &= ~(1 << offset);
            }

            charspassed += 16;
        } while (charspassed < size);

        // Add any rest of the string last
        vec.emplace_back(ptr + tokenStartPos, size - tokenStartPos);
    }
} // namespace
