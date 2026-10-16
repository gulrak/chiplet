#include <doctest/doctest.h>

#include "../src/octo/assembler.hpp"
#include <chiplet/octocompiler.hpp>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
using Data = std::vector<uint8_t>;

Data assembleHybrid(const std::string& source, int origin = 0x200)
{
    octo::Assembler assembler{source, origin};
    INFO(source);
    INFO(assembler.errorMessage());
    REQUIRE(assembler.compile());
    return {assembler.data(), assembler.data() + assembler.codeSize()};
}

void rejectHybrid(const std::string& source, std::string_view message, int origin = 0x200)
{
    octo::Assembler assembler{source, origin};
    INFO(source);
    REQUIRE_FALSE(assembler.compile());
    INFO(assembler.errorMessage());
    CHECK(assembler.errorMessage().find(message) != std::string::npos);
}
}

TEST_SUITE("Hybrid")
{
    TEST_CASE("native blocks emit only their bytes without automatic calls or padding")
    {
        CHECK(assembleHybrid(": main native inc_bgcol loop again : inc_bgcol :asm cdp1802 { SEX R9; OUT 1; SEP R4 }")
            == Data{0x02,0x04,0x12,0x02,0xE9,0x61,0xD4});
        CHECK(assembleHybrid(": main :asm cdp1802 {} 0x12 0x34") == Data{0x12,0x34});
        CHECK(assembleHybrid(": main :asm cdp1802 {NOP} : pointer :pointer pointer") == Data{0xC4,0x02,0x01});
    }

    TEST_CASE("native instructions import Octo constants with case sensitive symbols")
    {
        CHECK(assembleHybrid(":const Port 1 :const port 2 : main :asm cdp1802 { out Port; out port; sex r9; sep r4 }")
            == Data{0x61,0x62,0xE9,0xD4});
        rejectHybrid(":const Port 1 : main :asm cdp1802 {OUT PORT}", "Undefined symbol 'PORT'");
    }

    TEST_CASE("native expressions resolve forward Octo labels at the actual origin")
    {
        CHECK(assembleHybrid(R"(
: main
    native routine
    jump done
: routine
    :asm cdp1802 {
        LDI A.1(sprite_data)
        PHI RF
        LDI A.0(sprite_data)
        PLO RF
        LBR sprite_data
    }
: sprite_data
    0x11
: done
    loop again
)") == Data{0x02,0x04,0x12,0x0E,0xF8,0x02,0xBF,0xF8,0x0D,0xAF,0xC0,0x02,0x0D,0x11,0x12,0x0E});
        CHECK(assembleHybrid(": main :asm cdp1802 {LDI A.1(*); LDI A.0(*)}", 0x7B8)
            == Data{0xF8,0x07,0xF8,0xBA});
    }

    TEST_CASE("local labels and equates belong to each block")
    {
        CHECK(assembleHybrid(R"(
: main
    :asm cdp1802 { again: NOP; BR again }
    :asm cdp1802 { again: NOP; BR again }
    :asm cdp1802 { ptr = R9; port EQU 1; SEX ptr; OUT port }
)") == Data{0xC4,0x30,0x00,0xC4,0x30,0x03,0xE9,0x61});
        rejectHybrid(": main :asm cdp1802 {hidden: NOP} jump hidden", "Undefined forward reference: hidden");
        rejectHybrid(": main :asm cdp1802 {same: NOP; same: NOP}", "Duplicate local symbol");
        rejectHybrid(": main :asm cdp1802 {R9 = 1; NOP}", "Cannot redefine a CPU register");
        rejectHybrid(": main :asm cdp1802 {123}", "Expected native instruction");
    }

    TEST_CASE("forward local labels work and need no global exports")
    {
        CHECK(assembleHybrid(": main :asm cdp1802 { BR done; NOP; done: SEP R4 }")
            == Data{0x30,0x03,0xC4,0xD4});
    }

    TEST_CASE("calc values are captured at the block rather than final emission")
    {
        CHECK(assembleHybrid(":calc value { 1 } : main :asm cdp1802 {LDI value} :calc value { 2 } :asm cdp1802 {LDI value}")
            == Data{0xF8,0x01,0xF8,0x02});
        rejectHybrid(": main :asm cdp1802 {LDI late} :const late 1", "Undefined symbol 'late'");
    }

    TEST_CASE("imports must be finite integers and fit their operands")
    {
        rejectHybrid(":calc value { 1.5 } : main :asm cdp1802 {LDI value}", "finite integer");
        rejectHybrid(":const value 256 : main :asm cdp1802 {LDI value}", "byte range");
        rejectHybrid(": main :asm cdp1802 {OUT 8}", "port out of range");
        rejectHybrid(": main :asm cdp1802 {SEX 16}", "Register out of range");
    }

    TEST_CASE("embedded data has an explicit stable byte or address width")
    {
        CHECK(assembleHybrid(": main :asm cdp1802 {DB A(later), A.1(later), A.0(later), T'OK'} : later 0xAA")
            == Data{0x02,0x06,0x02,0x06,0x4F,0x4B,0xAA});
        rejectHybrid(": main :asm cdp1802 {DB later} : later 0", "data value out of byte range");
        rejectHybrid(": main :asm cdp1802 {DB #1234}", "data value out of byte range");
    }

    TEST_CASE("origin and early termination directives are rejected inside a block")
    {
        for (const auto directive : {"ORG #300", "PAGE", "END"}) {
            rejectHybrid(": main :asm cdp1802 {" + std::string{directive} + "}", "not allowed inside :asm blocks");
        }
        CHECK(assembleHybrid(": main :org 0x204 :asm cdp1802 {LDI A.0(*)}") == Data{0,0,0,0,0xF8,0x04});
    }

    TEST_CASE("unsupported CPUs and missing braces have positioned errors")
    {
        rejectHybrid(": main :asm m6800 {RTS}", "Unsupported :asm CPU 'm6800'");
        rejectHybrid(": main :asm cdp1802 NOP", "Expected '{'");
        rejectHybrid(": main :asm cdp1802 {NOP", "Unterminated :asm block");
        rejectHybrid(": main :asm cdp1802 {{NOP}}", "Nested :asm blocks");
    }

    TEST_CASE("native bodies retain hash literals semicolons comments and quoted braces")
    {
        CHECK(assembleHybrid(": main :asm cdp1802 {LDI #39; DB T'{}#' .. } is inside a comment\n NOP}")
            == Data{0xF8,0x39,0x7B,0x7D,0x23,0xC4});
        CHECK(assembleHybrid(": main :asm cdp1802 {DB T'it\\'s}'; SEP R4}")
            == Data{0x69,0x74,0x27,0x73,0x7D,0xD4});
    }

    TEST_CASE("Octo macros can repeat opaque native blocks with independent locals")
    {
        CHECK(assembleHybrid(":macro emit { :asm cdp1802 {local: LBR local} } : main emit emit")
            == Data{0xC0,0x02,0x00,0xC0,0x02,0x03});
    }

    TEST_CASE("native diagnostics and source maps identify the native instruction line")
    {
        const std::string source=": main\n  :asm cdp1802 {\n    NOP\n    LDI missing\n  }\n";
        octo::Assembler bad{source};
        REQUIRE_FALSE(bad.compile());
        CHECK(bad.errorLine() == 4);
        CHECK(bad.errorPos() == 9);
        const std::string good=": main\n  :asm cdp1802 {\n    NOP\n    LDI #39\n  }\n";
        octo::Assembler assembler{good};
        REQUIRE(assembler.compile());
        CHECK(assembler.lineForAddress(0x200) == 2);
        CHECK(assembler.lineForAddress(0x201) == 3);
        CHECK(assembler.lineForAddress(0x202) == 3);
    }

    TEST_CASE("short branches validate the operand page including boundary instructions")
    {
        CHECK(assembleHybrid(": main :asm cdp1802 {BR target} : target :asm cdp1802 {NOP}", 0x2FF)
            == Data{0x30,0x01,0xC4});
        rejectHybrid(": main :asm cdp1802 {BR target} : target :asm cdp1802 {NOP}", "outside the operand's page", 0x2FE);
        CHECK(assembleHybrid(": main :asm cdp1802 {LBR target} : target :asm cdp1802 {NOP}", 0x2FE)
            == Data{0xC0,0x03,0x01,0xC4});
    }

    TEST_CASE("native blocks cannot exceed the CPU address space or overlap Octo output")
    {
        CHECK(assembleHybrid(": main :asm cdp1802 {NOP}", 0xFFFF) == Data{0xC4});
        rejectHybrid(": main :asm cdp1802 {NOP; NOP}", "16-bit address space", 0xFFFF);
        rejectHybrid(": main :asm cdp1802 {NOP}", "16-bit origin", 0x10000);
        rejectHybrid(": main :asm cdp1802 {NOP; NOP} :org 0x201 0", "Data overlap");
    }

    TEST_CASE("preprocessing preserves the native body and deferred include diagnostics")
    {
        const std::string source = ":const PORT 2\n: main\n:asm cdp1802 {LDI #2C; OUT PORT .. }\n SEP R4}\n";
        emu::OctoCompiler compiler;
        const auto& result = compiler.compile("hybrid.8o", std::span<const char>{source.data(), source.size()});
        INFO(result.errorMessage);
        REQUIRE_FALSE(compiler.isError());
        CHECK(Data{compiler.code(), compiler.code() + compiler.codeSize()} == Data{0xF8,0x2C,0x62,0xD4});
    }

    TEST_CASE("Wipeout ETI-660 mixed source reproduces all 606 original bytes")
    {
        const std::string root = CHIPLET_TEST_SOURCE_DIR;
        emu::OctoCompiler compiler;
        compiler.setStartAddress(0x600);
        compiler.compile(root + "/test/hybrid/wipeout-eti660.8o");
        INFO(compiler.compileResult().errorMessage);
        REQUIRE_FALSE(compiler.isError());
        std::ifstream expected{root + "/test/hybrid/wipeout-eti660.ch8", std::ios::binary};
        REQUIRE(expected.good());
        const Data data{std::istreambuf_iterator<char>{expected}, {}};
        REQUIRE(data.size() == 606);
        CHECK(Data{compiler.code(), compiler.code() + compiler.codeSize()} == data);
    }
}
