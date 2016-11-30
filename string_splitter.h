#pragma once

#include <string>
#include <array>
#include <vector>

namespace msu
{
#if defined(__clang__) || defined(__GNUC__)
# define MSU_ALIGNED(size) __attribute__((__aligned__(size)))
#elif defined(_MSC_VER)
# define MSU_ALIGNED(size) __declspec(align(size))
#else
# error Cannot define MSU_ALIGNED
#endif

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

// Array helpers
template<typename T, std::size_t S, T... Args>
constexpr std::array<T, S> make_array()
{
    return std::array<T, S> { { Args... } };
}

template <char ch>
constexpr std::array<char, 16> fill_array()
{
    return std::array<char, 16> { ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch, ch };
}

template <char... Args, typename F, std::size_t I1, std::size_t... I, enable_if_eq_zero(sizeof...(I))>
void const_foreach_itr(F&& f, const std::index_sequence<I1, I...>& i)
{
    constexpr auto tempChars = make_array<char, sizeof...(Args), Args...>();
    MSU_ALIGNED(16) constexpr auto filledArray = fill_array<std::get<I1>(tempChars)>();
    const auto val = _mm_load_si128(reinterpret_cast<const __m128i*>(filledArray.data()));

    f(std::forward<const __m128i>(val), std::bool_constant<I1 == 0>());
    (void)(i);
}

template <char... Args, typename F, std::size_t I1, std::size_t... I, enable_if_not_eq_zero(sizeof...(I))>
void const_foreach_itr(F&& f, const std::index_sequence<I1, I...>& i)
{
    // this is wierd
    constexpr auto tempChars = make_array<char, sizeof...(Args), Args...>();
    MSU_ALIGNED(16) constexpr auto filledArray = fill_array<std::get<I1>(tempChars)>();
    const auto val = _mm_load_si128(reinterpret_cast<const __m128i*>(filledArray.data()));

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
void split(std::vector<meyer::StringPiece>& vec, const std::string& str)
{
    const char* const ptr = str.c_str();
    const size_t size = str.size();
    int charspassed = 0;
    int tokenStartPos = 0;

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

        int tokenStopPos = 0;
        int movemask = _mm_movemask_epi8(mask);
        while (movemask != 0)
        {
            // Get next offset from bitmask
            unsigned long temp;
            _BitScanForward(&temp, movemask);
            const int offset = static_cast<int>(temp);

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
