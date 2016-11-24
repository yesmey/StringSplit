#pragma once

#pragma intrinsic _BitScanForward

#include <string>
#include <array>
#include <vector>

namespace meyer
{
#if defined(__clang__) || defined(__GNUC__)
# define MEYER_ALIGNED(size) __attribute__((__aligned__(size)))
#elif defined(_MSC_VER)
# define MEYER_ALIGNED(size) __declspec(align(size))
#else
# error Cannot define MEYER_ALIGNED
#endif

class StringPiece final
{
public:
    StringPiece(const char* b, const char* e) : _begin(b), _end(e) {}

    size_t size() const { return _end - _begin; }
    bool empty() const { return _begin == _end; }
    const char& operator[](size_t i) const { return _begin[i]; }

    operator const char*() const { return _begin; }

    std::string str() const { return std::string(_begin, size()); }
    const char* begin() const { return _begin; }
    const char* end() const { return _end; }

    StringPiece subpiece(int64_t first, int64_t end) const {
        return StringPiece(_begin + first, _begin + end);
    }
private:
    const char* _begin;
    const char* _end;
};

namespace details
{
// Some template code to compare integral constant to 0
enum class enabler_t
{
    EMPTY
};
constexpr const enabler_t empty = enabler_t::EMPTY;

template <size_t T>
struct is_empty
{
    static const bool value = T == 0;
};

#define enable_if_eq_zero(val)        typename std::enable_if<std::integral_constant<bool, is_empty<val>::value>::value, enabler_t>::type = empty
#define enable_if_not_eq_zero(val)    typename std::enable_if<!std::integral_constant<bool, is_empty<val>::value>::value, enabler_t>::type = empty

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
void const_in_subset(F&& f, const std::index_sequence<I1, I...>& i)
{
    constexpr auto tempChars = make_array<char, sizeof...(Args), Args...>();
    MEYER_ALIGNED(16) constexpr auto filledArray = fill_array<std::get<I1>(tempChars)>();

    const auto&& val = _mm_load_si128(reinterpret_cast<const __m128i*>(filledArray.data()));
    f(std::forward<const __m128i>(val), std::bool_constant<I1 == 0>());
    (void)(i);
}

template <char... Args, typename F, std::size_t I1, std::size_t... I, enable_if_not_eq_zero(sizeof...(I))>
void const_in_subset(F&& f, const std::index_sequence<I1, I...>& i)
{
    constexpr auto tempChars = make_array<char, sizeof...(Args), Args...>();
    MEYER_ALIGNED(16) constexpr auto filledArray = fill_array<std::get<I1>(tempChars)>();
    const auto&& val = _mm_load_si128(reinterpret_cast<const __m128i*>(filledArray.data()));

    f(std::forward<const __m128i>(val), std::bool_constant<I1 == 0>());
    const_in_subset<Args...>(f, std::index_sequence<I...>());
    (void)(i);
}

template <char... Args, typename F>
void const_for_each(F&& f)
{
    const_in_subset<Args...>(f, std::make_index_sequence<sizeof...(Args)>());
}

}

template<char... Args>
std::vector<StringPiece> split(const std::string& str)
{
    StringPiece source(str.c_str(), str.c_str() + str.size());

    std::vector<StringPiece> vec;
    int64_t charspassed = 0;
    int64_t tokenStartPos = 0;

    do
    {
        const __m128i xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(source.begin() + charspassed));
 
        __m128i mask;
        details::const_for_each<Args...>([&](const __m128i cmpVal, const bool first)
        {
            if (first)
                mask = _mm_cmpeq_epi8(xmm1, cmpVal);
            else
                mask = _mm_or_si128(mask, _mm_cmpeq_epi8(xmm1, cmpVal));
        });

        int movemask = _mm_movemask_epi8(mask);
        while (movemask != 0)
        {
            // Get next offset from bitmask
            unsigned long offset;
            _BitScanForward(&offset, movemask);
            int64_t offsetdiff = static_cast<int64_t>(offset);

            // Insert stringpiece into vector
            vec.emplace_back(source.subpiece(tokenStartPos, charspassed + offsetdiff));
            tokenStartPos = charspassed + offsetdiff + 1;

            // Remove this offset from bitmask
            movemask &= ~(1 << offsetdiff);
        }

        charspassed += 16;
        //tokenStartPos = charspassed;
    } while (charspassed < str.size());

    // Add any rest of the string last
    vec.emplace_back(source.subpiece(tokenStartPos, source.size()));
    return vec;
}

}
