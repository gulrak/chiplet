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
#include <doctest/doctest.h>

#include <cmath>
#include "../bit.hpp"

TEST_SUITE("<bit>")
{
    TEST_CASE("bit_cast")
    {
        double f64v = 19880124.0;
        auto u64v = ghc::bit_cast<std::uint64_t>(f64v);
        CHECK(ghc::bit_cast<double>(u64v) == f64v);

        std::uint64_t u64v2 = 0x3fe9000000000000ull;
        auto f64v2 = ghc::bit_cast<double>(u64v2);
        CHECK(ghc::bit_cast<std::uint64_t>(f64v2) == u64v2);
    }

    TEST_CASE("has_single_bit")
    {
        for (unsigned i = 0; i < 1000; ++i) {
            if (ghc::has_single_bit(i))
                CHECK((1u << static_cast<unsigned>(std::log2(i))) == i);
            else
                CHECK((1u << static_cast<unsigned>(std::log2(i))) != i);
        }
        CHECK(ghc::has_single_bit(0x80000000));
        CHECK(ghc::has_single_bit(0x8000000000000000ull));
    }

    TEST_CASE_TEMPLATE("countl_zero", T, uint8_t, uint16_t, uint32_t, uint64_t)
    {
        constexpr auto d = std::numeric_limits<T>::digits;
#ifdef COMPILE_TIME_TESTS
        static_assert(ghc::countl_zero(T{}) == d);
        static_assert(ghc::countl_zero(T{1}) == d - 1);
        static_assert(ghc::countl_zero(T{2}) == d - 2);
#endif
        CHECK(ghc::countl_zero(T{}) == d);
        CHECK(ghc::countl_zero(T{1}) == d - 1);
        CHECK(ghc::countl_zero(T{2}) == d - 2);
    }

    TEST_CASE_TEMPLATE("countl_one", T, uint8_t, uint16_t, uint32_t, uint64_t)
    {
        constexpr auto d = std::numeric_limits<T>::digits;
#ifdef COMPILE_TIME_TESTS
        static_assert(ghc::countl_one(T{}) == 0);
        static_assert(ghc::countl_one(T{1}) == 0);
        static_assert(ghc::countl_one(T{std::numeric_limits<T>::max() - 1}) == (d - 1));
        static_assert(ghc::countl_one(std::numeric_limits<T>::max()) == d);
#endif
        CHECK(ghc::countl_one(T{}) == 0);
        CHECK(ghc::countl_one(T{1}) == 0);
        CHECK(ghc::countl_one(T{std::numeric_limits<T>::max() - 1}) == (d - 1));
        CHECK(ghc::countl_one(std::numeric_limits<T>::max()) == d);
    }

    TEST_CASE_TEMPLATE("countr_zero", T, uint8_t, uint16_t, uint32_t, uint64_t)
    {
#ifdef COMPILE_TIME_TESTS
        constexpr auto d = std::numeric_limits<T>::digits;
        static_assert(ghc::countr_zero(T{}) == d);
        static_assert(ghc::countr_zero(T{1}) == 0);
        static_assert(ghc::countr_zero(T{2}) == 1);
#endif
        CHECK(ghc::countr_zero(T{}) == d);
        CHECK(ghc::countr_zero(T{1}) == 0);
        CHECK(ghc::countr_zero(T{2}) == 1);
    }

    TEST_CASE_TEMPLATE("countr_one", T, uint8_t, uint16_t, uint32_t, uint64_t)
    {
        constexpr auto d = std::numeric_limits<T>::digits;
#ifdef COMPILE_TIME_TESTS
        static_assert(ghc::countr_one(T{}) == 0);
        static_assert(ghc::countr_one(T{1}) == 1);
        static_assert(ghc::countr_one(T{2}) == 0);
        static_assert(ghc::countr_one(T{0x07f}) == 7);
        static_assert(ghc::countr_one(T{std::numeric_limits<T>::max() - 1}) == 0);
        static_assert(ghc::countr_one(std::numeric_limits<T>::max()) == d);
#endif
        CHECK(ghc::countr_one(T{}) == 0);
        CHECK(ghc::countr_one(T{1}) == 1);
        CHECK(ghc::countr_one(T{2}) == 0);
        CHECK(ghc::countr_one(T{0x07f}) == 7);
        CHECK(ghc::countr_one(T{std::numeric_limits<T>::max() - 1}) == 0);
        CHECK(ghc::countr_one(std::numeric_limits<T>::max()) == d);
    }

    TEST_CASE_TEMPLATE("bit_width", T, uint8_t, uint16_t, uint32_t, uint64_t)
    {
#ifdef COMPILE_TIME_TESTS
        static_assert(ghc::bit_width(T{}) == 0);
        static_assert(ghc::bit_width(T{1}) == 1);
        static_assert(ghc::bit_width(T{2}) == 2);
        static_assert(ghc::bit_width(T{3}) == 2);
        static_assert(ghc::bit_width(T{0x42}) == 7);
        static_assert(ghc::bit_width(T{0xff}) == 8);
#endif
        CHECK(ghc::bit_width(T{}) == 0);
        CHECK(ghc::bit_width(T{1}) == 1);
        CHECK(ghc::bit_width(T{2}) == 2);
        CHECK(ghc::bit_width(T{3}) == 2);
        CHECK(ghc::bit_width(T{0x42}) == 7);
        CHECK(ghc::bit_width(T{0xff}) == 8);
    }

    TEST_CASE_TEMPLATE("bit_ceil", T, uint8_t, uint16_t, uint32_t, uint64_t)
    {
#ifdef COMPILE_TIME_TESTS
        static_assert(ghc::bit_ceil(T{}) == 1);
        static_assert(ghc::bit_ceil(T{1}) == 1);
        static_assert(ghc::bit_ceil(T{42}) == 64);
#endif
        CHECK(ghc::bit_ceil(T{}) == 1);
        CHECK(ghc::bit_ceil(T{1}) == 1);
        CHECK(ghc::bit_ceil(T{42}) == 64);
    }

    TEST_CASE_TEMPLATE("bit_floor", T, uint8_t, uint16_t, uint32_t, uint64_t)
    {
#ifdef COMPILE_TIME_TESTS
        static_assert(ghc::bit_floor(T{}) == 0);
        static_assert(ghc::bit_floor(T{1}) == 1);
        static_assert(ghc::bit_floor(T{42}) == 32);
#endif
        CHECK(ghc::bit_floor(T{}) == 0);
        CHECK(ghc::bit_floor(T{1}) == 1);
        CHECK(ghc::bit_floor(T{42}) == 32);
    }

    TEST_CASE_TEMPLATE("rotl", T, uint8_t, uint16_t, uint32_t, uint64_t)
    {
        const T i = 0b00011101;
        CHECK(ghc::rotl(i, 0) == 0b00011101);
        CHECK(ghc::rotl(i, 1) == 0b00111010);
        if(!std::is_same_v<T, uint8_t>) {
            CHECK(ghc::rotl(i, 4) == 0b111010000);
            CHECK(ghc::rotl(i, 9) == 0b11101000000000);
        }
        CHECK(ghc::rotl(i, -1) == (0b0001110 | (T{1} << (std::numeric_limits<T>::digits - 1))));
    }

    TEST_CASE_TEMPLATE("rotr", T, uint8_t, uint16_t, uint32_t, uint64_t)
    {
        const T i = 0b00011101;
        CHECK(ghc::rotr(i, 0) == 0b00011101);
        CHECK(ghc::rotr(i, 1) == (0b0001110 | (T{1} << (std::numeric_limits<T>::digits - 1))));
        CHECK(ghc::rotr(i, -1) == 0b00111010);
    }
}