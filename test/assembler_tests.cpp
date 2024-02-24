//
// Created by Steffen Schümann on 20.02.24.
//
#include <doctest/doctest.h>

#include <numeric>
#include <sstream>

#include "../src/octo_compiler.hpp"

using Data = std::vector<uint8_t>;

namespace doctest {
template<> struct
StringMaker<Data>
{
    static String convert(const Data& vec)
    {
        std::ostringstream oss;
        oss << "[";
        if(!vec.empty())
            oss << std::accumulate(std::next(vec.begin()), vec.end(), fmt::format("0x{:02x}", vec[0]), [](std::string& a, uint8_t b) { return a + "," + fmt::format("0x{:02x}", b); });
        oss << "]";
        return oss.str().c_str();
    }
};
} // namespace doctest

void compileTest(const std::string& source, const std::vector<uint8_t>& result)
{
    auto comp = std::make_unique<octo::Program>(source, 0x200);
    CHECK(comp->compile());
    CHECK(!comp->isError());
    CHECK_EQ(comp->codeSize(), result.size());
    CHECK_EQ(result, std::vector<uint8_t>(comp->data(), comp->data() + comp->codeSize()));
}

TEST_SUITE("Assembler")
{
    TEST_CASE("minimal")
    {
        compileTest(R"(: main)",{});
    }

    TEST_CASE("tiny")
    {
        compileTest(R"(
# a line comment

: main
	va := 0xBC
	loop again
)",
                    {0x6a, 0xbc, 0x12, 0x02});
    }

    TEST_CASE("negative literals")
    {
        compileTest(R"(
: main
	-23
	-0x3A
	-0b1
)",
                    {0xe9 ,0xc6, 0xff});
    }
}