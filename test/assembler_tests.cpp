//
// Created by Steffen Schümann on 20.02.24.
//
#include <doctest/doctest.h>
#include "../src/c-octo/octo_compiler.hpp"

TEST_CASE("Assembler: Tiny")
{
    std::string source = R"(
# a line comment

: main
	va := 0xBC
	loop again
)";
    std::vector<uint8_t> result = { 0x6a, 0xbc, 0x12, 0x02 };
    auto comp = std::make_unique<octo::Program>(source, 0x200);
    CHECK(comp->compile());
    CHECK(comp->romLength() - comp->romStartAddress() == result.size());
    CHECK(std::memcmp(comp->data() + comp->romStartAddress(), result.data(), result.size()) == 0);
}