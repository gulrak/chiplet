//
// Created by Steffen Schümann on 20.02.24.
//
#include <doctest/doctest.h>

#include <numeric>
#include <sstream>

#include "../src/octo/assembler.hpp"
#include "../src/octo/preprocessor.hpp"
#include <chiplet/octocompiler.hpp>

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

    TEST_CASE("OctoCompiler lists monitors in definition order and renders them like Octo")
    {
        // monitors can't reference labels that are defined later, the jump to main puts data at 0x202
        const std::string source = R"(
: data
: text
0 0
: main
:monitor v0 "x:%i y:%2i"
:monitor data 4
:monitor v2 3
:monitor text "%3c|%x|%b"
:monitor v0 "again %x"
:monitor data "%4i"
return
)";
        emu::OctoCompiler compiler;
        compiler.compile("monitors.8o", std::span<const char>{source.data(), source.size()});
        INFO(compiler.compileResult().errorMessage);
        REQUIRE_FALSE(compiler.isError());
        const auto monitors = compiler.monitors();
        REQUIRE(monitors.size() == 4);
        // a redefinition replaces the monitor but keeps its place
        CHECK(monitors[0].name == "v0");
        CHECK(monitors[0].source == emu::OctoCompiler::MonitorInfo::eREGISTER);
        CHECK(monitors[1].name == "data");
        CHECK(monitors[1].source == emu::OctoCompiler::MonitorInfo::eMEMORY);
        CHECK(monitors[1].base == 0x202);
        CHECK(monitors[2].name == "v2");
        CHECK(monitors[2].length == 3);
        CHECK(monitors[3].name == "text");
        REQUIRE(monitors[3].format.size() == 5);
        CHECK(monitors[3].format[0].kind == emu::OctoCompiler::MonitorInfo::Field::eCHAR);
        CHECK(monitors[3].format[0].length == 3);
        CHECK(monitors[3].format[1].kind == emu::OctoCompiler::MonitorInfo::Field::eTEXT);
        CHECK(monitors[3].format[1].text == "|");

        const uint8_t v[16] = {0x12, 0xAB, 0x01, 0x02, 0x03};
        auto reg = [&](uint32_t index) -> uint8_t { return index < 16 ? v[index] : 0; };
        auto mem = [](uint32_t index) -> uint8_t { return index == 0x202 ? 0xFF : static_cast<uint8_t>('A' + (index - 0x202)); };
        CHECK(monitors[0].render(reg) == "again 12");
        CHECK(monitors[1].render(mem) == "-12434620");  // 0xFF424344 as int32
        CHECK(monitors[2].render(reg) == "0x010203");
        CHECK(monitors[3].render(mem) == "\x7F" "BC|44|01000101");

        emu::OctoCompiler::MonitorInfo memory{.name = "m", .source = emu::OctoCompiler::MonitorInfo::eMEMORY, .base = 0x202, .length = 3};
        CHECK(memory.render(mem) == "0xFF 0x42 0x43");
        emu::OctoCompiler::MonitorInfo tail{.name = "v14", .source = emu::OctoCompiler::MonitorInfo::eREGISTER, .base = 14, .length = 4};
        CHECK(tail.render(reg) == "0x0000");  // only v14 and v15, like Octo's slice
        emu::OctoCompiler::MonitorInfo decimal{.name = "v0", .source = emu::OctoCompiler::MonitorInfo::eREGISTER, .base = 0,
                                               .format = {{.kind = emu::OctoCompiler::MonitorInfo::Field::eDECIMAL, .text = "%2i", .length = 2}}};
        CHECK(decimal.render(reg) == "4779");  // 0x12AB
    }

    TEST_CASE("monitor formats keep {} placeholders as text, also without a %")
    {
        const std::string source = R"(
: data
	1 2 3
: main
:monitor data "a{v1:%2i}b%i{{c}}"
:monitor v3 "only {v3:02x}"
return
)";
        emu::OctoCompiler compiler;
        compiler.compile("placeholders.8o", std::span<const char>{source.data(), source.size()});
        INFO(compiler.compileResult().errorMessage);
        REQUIRE_FALSE(compiler.isError());
        const auto monitors = compiler.monitors();
        REQUIRE(monitors.size() == 2);
        using Field = emu::OctoCompiler::MonitorInfo::Field;
        REQUIRE(monitors[0].format.size() == 3);
        CHECK(monitors[0].format[0].kind == Field::eTEXT);
        CHECK(monitors[0].format[0].text == "a{v1:%2i}b");
        CHECK(monitors[0].format[1].kind == Field::eDECIMAL);
        CHECK(monitors[0].format[1].length == 1);
        CHECK(monitors[0].format[2].text == "{{c}}");
        REQUIRE(monitors[1].format.size() == 1);
        CHECK(monitors[1].format[0].text == "only {v3:02x}");
        CHECK(monitors[1].length == -1);

        auto mem = [](uint32_t index) -> uint8_t { return static_cast<uint8_t>(index & 0xFF); };
        CHECK(monitors[0].render(mem) == "a{v1:%2i}b2{{c}}");  // data is at 0x202 after the jump to main
        auto upper = [](std::string_view text) { return "<" + std::string(text) + ">"; };
        CHECK(monitors[0].render(mem, upper) == "<a{v1:%2i}b>2<{{c}}>");
    }

    TEST_CASE("OctoCompiler exports labels and integer constants")
    {
        const std::string source = R"(
:const SPEED 3
:calc HALF { 1 / 2 }
:calc STEPS { SPEED * 4 }
: main
	jump main
: ship
	0xFF
)";
        emu::OctoCompiler compiler;
        compiler.compile("symbols.8o", std::span<const char>{source.data(), source.size()});
        INFO(compiler.compileResult().errorMessage);
        REQUIRE_FALSE(compiler.isError());
        const auto symbols = compiler.symbolValues();
        auto find = [&](std::string_view name) -> std::optional<int64_t> {
            for (const auto& [symbol, value] : symbols)
                if (symbol == name)
                    return value;
            return std::nullopt;
        };
        CHECK(find("SPEED") == 3);
        CHECK(find("STEPS") == 12);
        CHECK(find("main") == 0x200);
        CHECK(find("ship") == 0x202);
        CHECK_FALSE(find("HALF").has_value());
        CHECK(std::ranges::is_sorted(symbols));
    }

    TEST_CASE("OctoCompiler has no monitors after an error")
    {
        const std::string source = ":monitor v0 \"%q\"\n: main\nreturn\n";
        emu::OctoCompiler compiler;
        compiler.compile("broken.8o", std::span<const char>{source.data(), source.size()});
        CHECK(compiler.isError());
        CHECK(compiler.monitors().empty());
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
