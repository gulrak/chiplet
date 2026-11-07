//
// Created by Steffen Schümann on 28.11.25.
//
#include <doctest/doctest.h>

#include "../src/cdp1802/assembly_session.hpp"
#include "../src/cdp1802/lexer.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using Data = std::vector<uint8_t>;

namespace {

Data assemble(std::string_view source)
{
    cdp1802::AssemblySession session;
    const std::string sourceText{source};
    CAPTURE(sourceText);
    REQUIRE(session.compile(source));
    CHECK_FALSE(session.isError());
    return session.contiguousData();
}

} // namespace

TEST_SUITE("CDP1802")
{
    TEST_CASE("lexer splits compact Level-I tokens")
    {
        cdp1802::Lexer lexer{"ORG#8000\nLDI#00 ;PHI R0\n"};
        const auto& tokens = lexer.tokens();

        REQUIRE_FALSE(lexer.isError());
        CHECK_EQ(tokens[0].type, cdp1802::TokenType::IDENTIFIER);
        CHECK_EQ(std::string{tokens[0].text}, "ORG");
        CHECK_EQ(tokens[1].type, cdp1802::TokenType::NUMBER);
        CHECK_EQ(tokens[1].value, 0x8000);
        CHECK_EQ(std::string{tokens[3].text}, "LDI");
        CHECK_EQ(tokens[4].value, 0);
    }

    TEST_CASE("assembles immediate, register, branch, and address byte helpers")
    {
        CHECK_EQ(assemble(R"(
            ORG #8000
PC = R3
START:      LDI A.1(MAIN)
            PHI PC
            LDI A.0(MAIN)
            PLO PC
            SEP PC
MAIN:       BR MAIN
            END
)"),
                 Data{0xF8, 0x80, 0xB3, 0xF8, 0x07, 0xA3, 0xD3, 0x30, 0x07});
    }

    TEST_CASE("assembles inline data and DC text")
    {
        CHECK_EQ(assemble(R"(
            ORG #0200
            DIS,#00
            OUT 1,#01
MSG:        DC T'OK', A(MSG)
            END
)"),
                 Data{0x71, 0x00, 0x61, 0x01, 0x4F, 0x4B, 0x02, 0x04});
    }

    TEST_CASE("assembles semicolon-separated statements")
    {
        CHECK_EQ(assemble("ORG #8000\nLDI #D0 ; STR R1 ; BNZ *-#02\nEND\n"), Data{0xF8, 0xD0, 0x51, 0x3A, 0x01});
    }

    TEST_CASE("assembles data lists and zero-filled origin gaps")
    {
        CHECK_EQ(assemble("ORG #0200\nDB T'HI',#00\nORG #0205\n,#FF\nEND\n"), Data{0x48, 0x49, 0x00, 0x00, 0x00, 0xFF});
    }

    TEST_CASE("traditional listing is optional")
    {
        const std::string source = "ORG #0200\nSTART: LDI #01 ; PLO R3\nEND\n";
        cdp1802::AssemblySession session;

        REQUIRE(session.compile(source, {.listingMode = cdp1802::ListingMode::TRADITIONAL}));
        CHECK_GE(session.listing().capacity(), source.size() * 2);
        CHECK_EQ(session.listing(), "0000 ;              0001   ORG #0200\r\n"
                                    "0200 F801A3;        0002   START: LDI #01 ; PLO R3\r\n"
                                    "0203 ;              0003   END\r\n"
                                    "0000\r\n");
    }

    TEST_CASE("traditional listing wraps after the reference data field width")
    {
        const std::string source = "ORG #8279\n,T'INTRPT!',#00\nEND\n";
        cdp1802::AssemblySession session;

        REQUIRE(session.compile(source, {.listingMode = cdp1802::ListingMode::TRADITIONAL}));
        CHECK_EQ(session.listing(), "0000 ;              0001   ORG #8279\r\n"
                                    "8279 494E5452505421;0002   ,T'INTRPT!',#00\r\n"
                                    "8280 00;            \r\n"
                                    "8281 ;              0003   END\r\n"
                                    "0000\r\n");
    }

    TEST_CASE("modern listing is optional")
    {
        const std::string source = "ORG #0200\nSTART: LDI #01 ; PLO R3\nEND\n";
        cdp1802::AssemblySession session;

        REQUIRE(session.compile(source, {.listingMode = cdp1802::ListingMode::MODERN}));
        CHECK_GE(session.listing().capacity(), source.size() * 2);
        CHECK_EQ(session.listing(), "Line  Addr  Bytes                           Source\n"
                                    "0001  0000                                 ORG #0200\n"
                                    "0002  0200  F8 01 A3                       START: LDI #01 ; PLO R3\n"
                                    "0003  0203                                 END\n");
    }
}
