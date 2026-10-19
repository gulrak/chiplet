//
// Created by Steffen Schümann on 20.02.24.
//
#include <doctest/doctest.h>

#include <numeric>
#include <sstream>

#include "../src/octo/assembler.hpp"
#include "../src/octo/preprocessor.hpp"

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
            oss << std::accumulate(std::next(vec.begin()), vec.end(), fmt::format("0x{:02x}", vec[0]), [](const std::string& a, uint8_t b) { return a + "," + fmt::format("0x{:02x}", b); });
        oss << "]";
        return oss.str().c_str();
    }
};
} // namespace doctest

void compileTest(const std::string& source, const std::vector<uint8_t>& result)
{
    auto comp = std::make_unique<octo::Assembler>(source, 0x200);
    CHECK(comp->compile());
    CHECK(!comp->isError());
    CHECK_EQ(comp->codeSize(), result.size());
    CHECK_EQ(result, std::vector<uint8_t>(comp->data(), comp->data() + comp->codeSize()));
}

TEST_SUITE("Assembler")
{
    TEST_CASE("octo lexer preserves flexible identifiers")
    {
        octo::Lexer lex{"foo-bar foo.bar %scratch [v0] 8x5"};
        octo::Token token{0, 0};

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "foo-bar");
        CHECK_EQ(token.tid, octo::TokenId::TOK_UNKNOWN);

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "foo.bar");

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "%scratch");

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "[v0]");

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "8x5");
    }

    TEST_CASE("octo lexer classifies numbers and keywords")
    {
        octo::Lexer lex{": main va := -0x3A 0b1010 loop again"};
        octo::Token token{0, 0};

        lex.scanNextToken(token);
        CHECK_EQ(token.tid, octo::TokenId::COLON);
        CHECK_EQ(token.type, octo::Token::Type::DIRECTIVE);

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "main");

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(token.strValue, "va");

        lex.scanNextToken(token);
        CHECK_EQ(token.tid, octo::TokenId::ASSIGN);
        CHECK_EQ(token.type, octo::Token::Type::OPERATOR);

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::NUMBER);
        CHECK_EQ(static_cast<int>(token.numValue), -0x3A);

        lex.scanNextToken(token);
        CHECK_EQ(token.type, octo::Token::Type::NUMBER);
        CHECK_EQ(static_cast<int>(token.numValue), 0b1010);

        lex.scanNextToken(token);
        CHECK_EQ(token.tid, octo::TokenId::LOOP);
        CHECK_EQ(token.type, octo::Token::Type::KEYWORD);

        lex.scanNextToken(token);
        CHECK_EQ(token.tid, octo::TokenId::AGAIN);
        CHECK_EQ(token.type, octo::Token::Type::KEYWORD);
    }

    TEST_CASE("octo lexer optionally preserves trivia")
    {
        octo::Lexer lex{"# comment\n  : main", {.preprocessorMode = true}};
        octo::Token token{0, 0};

        lex.scanNextToken(token);
        CHECK_EQ(token.tid, octo::TokenId::COLON);
        CHECK_EQ(token.prefix, "# comment\n  ");
        CHECK_EQ(token.rawValue, ":");
        CHECK_EQ(token.line, 1);
        CHECK_EQ(token.pos, 2);
    }

    TEST_CASE("octo preprocessor scanner classifies preprocessor directives")
    {
        octo::Preprocessor preprocessor;
        auto result = preprocessor.scan(":include \"font.8o\"\n: main");

        REQUIRE_GE(result.tokens.size(), 5);
        CHECK_EQ(result.tokens[0].type, octo::Token::Type::PREPROCESSOR);
        CHECK_EQ(result.tokens[0].strValue, ":include");
        CHECK_EQ(result.tokens[1].type, octo::Token::Type::STRING);
        CHECK_EQ(result.tokens[1].strValue, "font.8o");
        CHECK_EQ(result.tokens[2].tid, octo::TokenId::COLON);
        CHECK_EQ(result.tokens[2].type, octo::Token::Type::DIRECTIVE);
        CHECK_EQ(result.tokens[2].prefix, "\n");
        CHECK_EQ(result.tokens[3].type, octo::Token::Type::IDENTIFIER);
        CHECK_EQ(result.tokens[3].strValue, "main");
    }

    TEST_CASE("minimal")
    {
        compileTest(R"(: main)",{});
    }

    TEST_CASE("monitor metadata preserves wide fields and escaped text")
    {
        octo::Assembler assembler{R"(
:const COUNT 2
: main
:monitor v0 "prefix\n%2c:%5x:%256x:%i"
:monitor v1 COUNT
:breakpoint "break\npoint"
:alias "longer\nescaped-register-name" v2
return
)"};
        REQUIRE(assembler.compile());
        const auto* monitor = assembler.monitorInfo("v0");
        REQUIRE(monitor != nullptr);
        CHECK(monitor->base == 0);
        CHECK(monitor->len == -1);
        REQUIRE(monitor->format.size() == 8);
        CHECK(monitor->format[0].text == "prefix\n");
        CHECK(monitor->format[1].base == octo::MonitorField::CHAR);
        CHECK(monitor->format[1].length == 2);
        CHECK(monitor->format[3].type == octo::MonitorField::BYTES);
        CHECK(monitor->format[3].length == 5);
        CHECK(monitor->format[3].offset == 2);
        CHECK(monitor->format[5].length == 256);
        CHECK(monitor->format[7].offset == 263);
        CHECK(monitor->format[7].base == octo::MonitorField::DECIMAL);
        const auto* counted = assembler.monitorInfo("v1");
        REQUIRE(counted != nullptr);
        CHECK(counted->len == 2);
        CHECK(counted->format.empty());
        CHECK(assembler.breakpointInfo(0x200) == "break\npoint");
    }

    TEST_CASE("malformed monitors return positioned assembly errors")
    {
        for (const auto format : {"%", "%q", "%0x"}) {
            const auto source = fmt::format(": main\n:monitor v0 \"{}\"\n", format);
            octo::Assembler assembler{source};
            CHECK_NOTHROW(assembler.compile());
            REQUIRE(assembler.isError());
            CHECK(assembler.errorLine() == 2);
            CHECK(assembler.errorPos() == 13);
        }
    }

    TEST_CASE("Octo stringmode expands supplementary characters as UTF-16 units")
    {
        compileTest(R"(
:stringmode text "😀" { :byte { CHAR & 255 } :byte INDEX :byte VALUE }
: main
text "😀"
)", {0x3d, 0, 0, 0, 1, 1});
    }

    TEST_CASE("Octo UTF-8 recovery preserves bytes following invalid sequences")
    {
        for (const auto& [text, length] : std::vector<std::pair<std::string, uint8_t>>{
                 {"\xed\xa0\x80", 3},
                 {"\xe2" "A", 2},
                 {"\xf0\x9f", 1},
                 {"\xc0\xaf", 2},
                 {"é\xe2" "A😀", 5},
             }) {
            const auto source = ": main\n:byte { strlen \"" + text + "\" }\nreturn\n";
            compileTest(source, {length, 0, 0xee});
        }
    }

    TEST_CASE("Octo bitwise arithmetic safely converts both operands")
    {
        compileTest(R"(
: main
:byte { 4294967297 & 255 }
:byte { ( 1 / 0 ) | 5 }
:byte { ( sqrt -1 ) ^ 7 }
:byte { -1 >> 33 }
:byte { 1 << -1 }
:byte { ! ( sqrt -1 ) }
)", {1, 5, 7, 255, 0, 1});
    }

    TEST_CASE("Octo large radix literals round once to double precision")
    {
        compileTest(R"(
: main
:byte { 0x29738c1f8f6395dc == 2986885045054313948 }
:byte { 0x23091c55ee9bd1ef0e7d == 165450889397460060278397 }
:byte { 0b10100101110011100011000001111110001111011000111001010111011100 == 2986885045054313948 }
)", {1, 1, 1});
    }

    TEST_CASE("Octo origin selects the label when a constant shares its name")
    {
        compileTest(R"(
:const target 1
: main
: target
:org target
return
)", {0, 0xee});
    }

    TEST_CASE("24-bit addresses reject invalid values before conversion")
    {
        for (const auto directive : {":org", ":pointer24"}) {
            for (const auto expression : {"16777216", "4294967808", "-4294967296", "sqrt -1", "1 / 0"}) {
                const auto source = fmt::format(": main\n{} {{ {} }}\n", directive, expression);
                octo::Assembler assembler{source};
                INFO(source);
                REQUIRE_FALSE(assembler.compile());
                CHECK(assembler.errorLine() == 2);
                CHECK((assembler.errorMessage().find("24 bits") != std::string::npos
                       || assembler.errorMessage().find("negative") != std::string::npos));
            }
        }
        octo::Assembler nonfiniteConstant{":calc VALUE { sqrt -1 }\n: main\n:pointer24 VALUE\n"};
        REQUIRE_FALSE(nonfiniteConstant.compile());
        CHECK(nonfiniteConstant.errorLine() == 3);
        compileTest(": main\n:pointer24 { 16777215 }\n", {0xff, 0xff, 0xff});
    }

    TEST_CASE("24-bit origins reach the final byte and can move before allocation")
    {
        for (const auto origin : {"16777215", "{ 16777215 }"}) {
            const auto source = fmt::format(
                ": main\n:org {}\n:org 16777214\n:byte 0x5a\n:byte 0xa5\n", origin);
            octo::Assembler assembler{source};
            REQUIRE(assembler.compile());
            REQUIRE(assembler.codeSize() == octo::Assembler::RAM_MAX - 0x200);
            CHECK(assembler.lastAddressUsed() == octo::Assembler::RAM_MASK);
            CHECK(assembler.data()[0] == 0);
            CHECK(assembler.data()[assembler.codeSize() - 2] == 0x5a);
            CHECK(assembler.data()[assembler.codeSize() - 1] == 0xa5);
        }
    }

    TEST_CASE("generated jumps accept the last 12-bit destination and check else patching")
    {
        for (const auto& source : {
                 ": main\n:org 4095\nloop\nagain\n",
                 ": main\n:org 4091\nif v0 == 1 begin\nend\n",
                 ": main\n:org 4089\nloop\nwhile v0 == 1\nagain\n",
             }) {
            octo::Assembler assembler{source};
            INFO(source);
            REQUIRE(assembler.compile());
        }
        octo::Assembler assembler{": main\n:org 4091\nif v0 == 1 begin\nelse\nend\n"};
        REQUIRE_FALSE(assembler.compile());
        CHECK(assembler.errorLine() == 4);
        CHECK(assembler.errorMessage() == "Argument 4097 does not fit in 12 bits.");
    }

    TEST_CASE("an else branch may contain a nested if with its own else")
    {
        compileTest(R"(
: main
if v0 == 1 begin
clear
else
if v1 == 2 begin
clear
else
clear
end
end
)", {0x30, 1, 0x12, 8, 0, 0xe0, 0x12, 0x12,
     0x31, 2, 0x12, 0x10, 0, 0xe0, 0x12, 0x12, 0, 0xe0});
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
