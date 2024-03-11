//---------------------------------------------------------------------------------------
//
// ghc::bit - A minimal C++20/23-like <bit> implementation for C++17
//
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2024, Steffen Schümann <s.schuemann@pobox.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//---------------------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace ghc {

// 26.5.3, bit_cast
template <class To, class From>
typename std::enable_if_t<(sizeof(To) == sizeof(From)) && std::is_trivially_copyable_v<To> && std::is_trivially_copyable_v<From>, To> bit_cast(const From& from) noexcept;

// 26.5.4, integral powers of 2
template <class T>
constexpr bool has_single_bit(T x) noexcept;
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>,T> bit_width(T x) noexcept;
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>,T> bit_ceil(T x) noexcept;
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>,T> bit_floor(T x) noexcept;

// 26.5.5, rotating
template <class T>
[[nodiscard]] constexpr typename std::enable_if_t<std::is_unsigned_v<T>, T> rotl(T x, int s) noexcept;
template <class T>
[[nodiscard]] constexpr typename std::enable_if_t<std::is_unsigned_v<T>, T> rotr(T x, int s) noexcept;

// 26.5.6, counting
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>, int> countl_zero(T x) noexcept;
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>, int> countl_one(T x) noexcept;
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>, int> countr_zero(T x) noexcept;
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>, int> countr_one(T x) noexcept;
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>, int> popcount(T x) noexcept;

// 26.5.7, endian
enum class endian {
#if defined(_MSC_VER) && !defined(__clang__)
    little = 0,
    big = 1,
    native = little
#else
    little = __ORDER_LITTLE_ENDIAN__,
    big = __ORDER_BIG_ENDIAN__,
    native = __BYTE_ORDER__
#endif
};

template <class T>
constexpr typename std::enable_if_t<std::is_integral_v<T>, T> byteswap(T n) noexcept;

#if (defined(_MSC_VER) && !defined(__clang__)) || (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
template <class T>
constexpr typename std::enable_if_t<std::is_integral_v<T>, T> native_to_be(T n) noexcept
{
    return byteswap(n);
}
template <class T>
constexpr typename std::enable_if_t<std::is_integral_v<T>, T> be_to_native(T n) noexcept
{
    return byteswap(n);
}
template <class T>
constexpr typename std::enable_if_t<std::is_integral_v<T>, T> native_to_le(T n) noexcept
{
    return n;
}
template <class T>
constexpr typename std::enable_if_t<std::is_integral_v<T>, T> le_to_native(T n) noexcept
{
    return n;
}
#else
template <class T>
constexpr typename std::enable_if_t<std::is_integral_v<T>, T> native_to_be(T n) noexcept
{
    return n;
}
template <class T>
constexpr typename std::enable_if_t<std::is_integral_v<T>, T> be_to_native(T n) noexcept
{
    return n;
}
template <class T>
constexpr typename std::enable_if_t<std::is_integral_v<T>, T> native_to_le(T n) noexcept
{
    return byteswap(n);
}
constexpr typename std::enable_if_t<std::is_integral_v<T>, T> le_to_native(T n) noexcept
{
    return byteswap(n);
}
#endif

//---------------------------------------------------------------------------------------
// IMPLEMENTATION
//---------------------------------------------------------------------------------------

template <class To, class From>
typename std::enable_if_t<(sizeof(To) == sizeof(From)) && std::is_trivially_copyable_v<To> && std::is_trivially_copyable_v<From>, To> bit_cast(const From& from) noexcept
{
    To result;
    std::memcpy(&result, &from, sizeof(result));
    return result;
}

template <class T>
constexpr bool has_single_bit(T x) noexcept
{
    return x && !(x & (x - 1));
}

template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>,T> bit_width(T x) noexcept
{
    return std::numeric_limits<T>::digits - countl_zero(x);
}

template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>,T> bit_ceil(T x) noexcept
{
    if (x <= 1u)
        return 1u;
    return T(1u << bit_width(x));
}
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>,T> bit_floor(T x) noexcept
{
    if (x == 0)
        return x;
    return T(1u << (bit_width(x) - 1));
}

// 26.5.5, rotating
template <class T>
[[nodiscard]] constexpr typename std::enable_if_t<std::is_unsigned_v<T>, T> rotl(T x, int s) noexcept
{
    constexpr int N = std::numeric_limits<T>::digits;
    auto r = s % N;
    if (r == 0)
        return x;
    if (r > 0)
        return (x << r) | (x >> (N - r));
    return rotr(x, -r);
}
template <class T>
[[nodiscard]] constexpr typename std::enable_if_t<std::is_unsigned_v<T>, T> rotr(T x, int s) noexcept
{
    constexpr int N = std::numeric_limits<T>::digits;
    auto r = s % N;
    if (r == 0)
        return x;
    if (r > 0)
        return (x >> r) | (x << (N - r));
    return rotl(x, -r);
}

// 26.5.6, counting
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>, int> countl_zero(T x) noexcept
{
    if (!x)
        return std::numeric_limits<T>::digits;
#ifdef __GNUC__
    if constexpr (sizeof(T) == sizeof(unsigned int))
        return __builtin_clz(x);
    if constexpr (sizeof(T) == sizeof(unsigned long))
        return __builtin_clzl(x);
    if constexpr (sizeof(T) == sizeof(unsigned long long))
        return __builtin_clzll(x);
    return __builtin_clzll(static_cast<std::make_unsigned_t<T>>(x)) + std::numeric_limits<T>::digits - std::numeric_limits<unsigned long long>::digits;
#elif defined _MSC_VER
    unsigned long index;
    if constexpr (sizeof(T) <= 32) {
        _BitScanReverse(&index, static_cast<std::make_unsigned<T>>(x));
    }
    else {
        _BitScanReverse(&index, static_cast<std::make_unsigned<T>>(x));
    }
    return static_cast<int>(std::numeric_limits<T>::digits - index);
#else
    T mask = static_cast<T>(1) << (std::numeric_limits<T>::digits - 1);
    int result = 0;
    while (mask) {
        if (x & mask)
            break;
        ++result, mask >>= 1;
    }
    return result;
#endif
}
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>, int> countl_one(T x) noexcept
{
    if (!x)
        return 0;
    T mask = static_cast<T>(1) << (std::numeric_limits<T>::digits - 1);
    int result = 0;
    while (mask) {
        if (!(x & mask))
            break;
        ++result, mask >>= 1;
    }
    return result;
}
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>, int> countr_zero(T x) noexcept
{
    if (!x)
        return std::numeric_limits<T>::digits;
    T mask = 1;
    int result = 0;
    while (mask) {
        if (x & mask)
            break;
        ++result, mask <<= 1;
    }
    return result;
}
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>, int> countr_one(T x) noexcept
{
    if (!x)
        return 0;
    T mask = 1;
    int result = 0;
    while (mask) {
        if (!(x & mask))
            break;
        ++result, mask <<= 1;
    }
    return result;
}
template <class T>
constexpr typename std::enable_if_t<std::is_unsigned_v<T>, int> popcount(T x) noexcept
{
    int result = 0;
    while (x) {
        ++result, x &= x - 1;
    }
    return result;
}

template <class T>
constexpr typename std::enable_if_t<std::is_integral_v<T>, T> byteswap(T n) noexcept
{
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "only 8, 16, 32 or 64 bit integral types are supported");
    auto un = static_cast<std::make_unsigned_t<T>>(n);
    if constexpr (sizeof(T) == 2) {
        return static_cast<T>((un << 8) | (un >> 8));
    }
    if constexpr (sizeof(T) == 4) {
        un = ((un & 0x0000FFFF) << 16) | ((un & 0xFFFF0000) >> 16);
        un = ((un & 0x00FF00FF) << 8) | ((un & 0xFF00FF00) >> 8);
    }
    if constexpr (sizeof(T) == 8) {
        un = ((un & 0x00000000FFFFFFFF) << 32) | ((un & 0xFFFFFFFF00000000) >> 32);
        un = ((un & 0x0000FFFF0000FFFF) << 16) | ((un & 0xFFFF0000FFFF0000) >> 16);
        un = ((un & 0x00FF00FF00FF00FF) << 8) | ((un & 0xFF00FF00FF00FF00) >> 8);
    }
    return static_cast<T>(un);
}

}  // namespace ghc
