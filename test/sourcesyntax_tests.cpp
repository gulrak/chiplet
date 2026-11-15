#include <doctest/doctest.h>

#include <chiplet/decoder/saturndecoder.hpp>
#include <chiplet/sourcesyntax.hpp>

#include "../src/cdp1802/assembler.hpp"

#include <string>

using emu::SourceLanguage;
using emu::SourceSyntax;

TEST_SUITE("SourceSyntax")
{
    TEST_CASE("Only assembly languages have a syntax")
    {
        CHECK(SourceSyntax::forLanguage(SourceLanguage::OCTO) == nullptr);
        CHECK(SourceSyntax::forLanguage(SourceLanguage::UNKNOWN) == nullptr);
        for (auto language : {SourceLanguage::CDP1802, SourceLanguage::M6800, SourceLanguage::SATURN}) {
            REQUIRE(SourceSyntax::forLanguage(language) != nullptr);
            CHECK(SourceSyntax::forLanguage(language)->language == language);
        }
    }

    TEST_CASE("CDP1802: the instructions of Chiplet's assembler")
    {
        const auto& syntax = *SourceSyntax::forLanguage(SourceLanguage::CDP1802);
        for (const auto& [name, instruction] : cdp1802::Assembler::instructions()) {
            CAPTURE(std::string(name));
            CHECK(syntax.isMnemonic(name));
        }
        CHECK(syntax.isMnemonic("ldi"));
        CHECK_FALSE(syntax.isMnemonic("LDAA"));
        CHECK(syntax.isDirective("ORG"));
        CHECK(syntax.isDirective("equ"));
        CHECK(syntax.isDataDirective("DC"));
        CHECK_FALSE(syntax.isDataDirective("ORG"));
        CHECK(syntax.isRegister("R0"));
        CHECK(syntax.isRegister("rf"));
        CHECK_FALSE(syntax.isRegister("RG"));
        // the name a statement word starts with
        CHECK(syntax.nameOf("LDI#00") == "LDI");
        CHECK(syntax.nameOf("DIS,#00") == "DIS");
        CHECK(syntax.nameOf("add.a").empty());
        CHECK(syntax.nameOf("inc_bgcol").empty());
        CHECK(syntax.nameOf("#12").empty());
        CHECK(syntax.lineComment == "..");
        CHECK(syntax.statementSeparator == ';');
    }

    TEST_CASE("Saturn: the mnemonics of the decoder table and asacpp's directives")
    {
        const auto& syntax = *SourceSyntax::forLanguage(SourceLanguage::SATURN);
        for (size_t index = 0; index < emu::SaturnDecoder::numInstructions(); ++index) {
            const std::string format = emu::SaturnDecoder::getInstructionInfo(index)->fmt;
            CAPTURE(format);
            CHECK(syntax.isMnemonic(syntax.nameOf(format.substr(0, format.find(' ')))));
        }
        CHECK(syntax.nameOf("move.a") == "move");
        CHECK(syntax.nameOf("move.p5") == "move");
        CHECK(syntax.isMnemonic("retsxm"));
        CHECK(syntax.isMnemonic("RETCLRC"));
        CHECK_FALSE(syntax.isMnemonic("frobnicate"));
        CHECK_FALSE(syntax.isMnemonic("ldi"));
        CHECK(syntax.isDirective("include"));
        CHECK(syntax.isDataDirective("data"));
        CHECK(syntax.isRegister("d0"));
        CHECK_FALSE(syntax.isRegister("d2"));
        CHECK(syntax.directivePrefix == '!');
        CHECK(syntax.sizeSuffix);
    }

    TEST_CASE("M6800: the mnemonics with and without the accumulator in the name")
    {
        const auto& syntax = *SourceSyntax::forLanguage(SourceLanguage::M6800);
        CHECK(syntax.isMnemonic("LDAA"));
        CHECK(syntax.isMnemonic("staB"));
        CHECK(syntax.isMnemonic("INX"));
        CHECK(syntax.isMnemonic("CLR"));  // also on memory
        CHECK_FALSE(syntax.isMnemonic("LDA"));
        CHECK(syntax.isRegisterOperandMnemonic("LDA"));
        CHECK(syntax.isDataDirective("FCB"));
        CHECK(syntax.isDirective("EQU"));
        CHECK(syntax.isRegister("X"));
        CHECK(syntax.firstColumnComment == '*');
    }
}
