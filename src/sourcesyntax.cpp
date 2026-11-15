//---------------------------------------------------------------------------------------
// src/sourcesyntax.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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
// How the assembly sources of the languages Chiplet knows are written
//---------------------------------------------------------------------------------------
#include <chiplet/sourcesyntax.hpp>
#include <chiplet/decoder/saturndecoder.hpp>

#include "cdp1802/assembler.hpp"

#include <algorithm>
#include <cctype>
#include <initializer_list>

namespace emu {

namespace {

std::string upper(std::string_view text)
{
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

void insertAll(std::unordered_set<std::string>& set, std::initializer_list<const char*> words)
{
    for (const auto* word : words)
        set.insert(word);
}

// RCA COSMAC Level I syntax of Chiplet's CDP1802 assembler
SourceSyntax makeCdp1802()
{
    SourceSyntax syntax;
    syntax.language = SourceLanguage::CDP1802;
    syntax.lineComment = "..";
    syntax.statementSeparator = ';';
    syntax.firstColumnLabels = true;
    syntax.numberPrefixes = "#$%";
    syntax.quotes = "'";
    syntax.basedNumberPrefixes = "XDB";
    for (const auto& [name, instruction] : cdp1802::Assembler::instructions())
        syntax.mnemonics.insert(upper(name));
    insertAll(syntax.directives, {"ORG", "DC", "DB", "PAGE", "END", "EQU"});  // cdp1802::Assembler::isDirective() and EQU
    insertAll(syntax.dataDirectives, {"DC", "DB"});
    for (int reg = 0; reg < 16; ++reg)
        syntax.registers.insert(std::string("R") + "0123456789ABCDEF"[reg]);
    return syntax;
}

// The common Motorola 6800 assemblers, "LDAA" and "LDA A"
SourceSyntax makeM6800()
{
    SourceSyntax syntax;
    syntax.language = SourceLanguage::M6800;
    syntax.lineComment = ";";
    syntax.firstColumnComment = '*';
    syntax.firstColumnLabels = true;
    syntax.numberPrefixes = "#$%@";
    syntax.quotes = "'\"";
    insertAll(syntax.mnemonics, {"ABA",  "ADCA", "ADCB", "ADDA", "ADDB", "ANDA", "ANDB", "ASL",  "ASLA", "ASLB", "ASR",  "ASRA", "ASRB", "BCC",
                                 "BCS",  "BEQ",  "BGE",  "BGT",  "BHI",  "BITA", "BITB", "BLE",  "BLS",  "BLT",  "BMI",  "BNE",  "BPL",  "BRA",
                                 "BSR",  "BVC",  "BVS",  "CBA",  "CLC",  "CLI",  "CLR",  "CLRA", "CLRB", "CLV",  "CMPA", "CMPB", "COM",  "COMA",
                                 "COMB", "CPX",  "DAA",  "DEC",  "DECA", "DECB", "DES",  "DEX",  "EORA", "EORB", "INC",  "INCA", "INCB", "INS",
                                 "INX",  "JMP",  "JSR",  "LDAA", "LDAB", "LDS",  "LDX",  "LSR",  "LSRA", "LSRB", "NBA",  "NEG",  "NEGA", "NEGB",
                                 "NOP",  "ORAA", "ORAB", "PSHA", "PSHB", "PULA", "PULB", "ROL",  "ROLA", "ROLB", "ROR",  "RORA", "RORB", "RTI",
                                 "RTS",  "SBA",  "SBCA", "SBCB", "SEC",  "SEI",  "SEV",  "STAA", "STAB", "STS",  "STX",  "SUBA", "SUBB", "SWI",
                                 "TAB",  "TAP",  "TBA",  "TPA",  "TST",  "TSTA", "TSTB", "TSX",  "TXS",  "WAI"});
    insertAll(syntax.registerOperandMnemonics, {"ADC", "ADD", "AND", "ASL", "ASR", "BIT", "CLR", "CMP", "COM", "DEC", "EOR", "INC",
                                                "LDA", "LSR", "NEG", "ORA", "PSH", "PUL", "ROL", "ROR", "SBC", "STA", "SUB", "TST"});
    insertAll(syntax.directives, {"ORG", "EQU", "SET", "FCB", "FDB", "FCC", "RMB", "ZMB", "BSZ", "FILL", "END", "NAM", "OPT", "PAGE", "SPC", "TTL"});
    insertAll(syntax.dataDirectives, {"FCB", "FDB", "FCC", "RMB"});
    insertAll(syntax.registers, {"A", "B", "X"});
    return syntax;
}

// The ASAP syntax (Alonzo Gariepy's mnemonics) of asacpp: the mnemonics of the Saturn decoder table, asacpp's aliases
// and directives
SourceSyntax makeSaturn()
{
    SourceSyntax syntax;
    syntax.language = SourceLanguage::SATURN;
    syntax.lineComment = ";";
    syntax.directivePrefix = '!';
    syntax.sizeSuffix = true;
    syntax.numberPrefixes = "#";
    syntax.quotes = "\"'";
    for (size_t index = 0; index < SaturnDecoder::numInstructions(); ++index) {
        const std::string_view format = SaturnDecoder::getInstructionInfo(index)->fmt;
        syntax.mnemonics.insert(upper(format.substr(0, format.find_first_of(". "))));
    }
    insertAll(syntax.mnemonics, {"RETSXM"});  // an alias of retsetxm
    insertAll(syntax.directives, {"DATA", "DAT", "INCLUDE", "MACRO", "ENDMACRO", "IF", "ELSE", "ENDIF", "EVEN"});
    insertAll(syntax.dataDirectives, {"DATA", "DAT"});
    insertAll(syntax.registers, {"A", "B", "C", "D", "D0", "D1", "R0", "R1", "R2", "R3", "R4", "P", "PC", "ST", "ID"});
    return syntax;
}

}  // namespace

bool SourceSyntax::isMnemonic(std::string_view word) const
{
    return mnemonics.contains(upper(word));
}

bool SourceSyntax::isRegisterOperandMnemonic(std::string_view word) const
{
    return registerOperandMnemonics.contains(upper(word));
}

bool SourceSyntax::isDirective(std::string_view word) const
{
    return directives.contains(upper(word));
}

bool SourceSyntax::isDataDirective(std::string_view word) const
{
    return dataDirectives.contains(upper(word));
}

bool SourceSyntax::isRegister(std::string_view word) const
{
    return registers.contains(upper(word));
}

std::string_view SourceSyntax::nameOf(std::string_view word) const
{
    size_t end = 0;
    while (end < word.size() && std::isalnum(static_cast<unsigned char>(word[end])))
        ++end;
    if (!end || !std::isalpha(static_cast<unsigned char>(word[0])))
        return {};
    if (end < word.size() && word[end] == '.')
        return sizeSuffix ? word.substr(0, end) : std::string_view{};
    if (end < word.size() && word[end] == '_')
        return {};
    return word.substr(0, end);
}

const SourceSyntax* SourceSyntax::forLanguage(SourceLanguage language)
{
    static const SourceSyntax cdp1802 = makeCdp1802();
    static const SourceSyntax m6800 = makeM6800();
    static const SourceSyntax saturn = makeSaturn();
    switch (language) {
        case SourceLanguage::CDP1802: return &cdp1802;
        case SourceLanguage::M6800: return &m6800;
        case SourceLanguage::SATURN: return &saturn;
        default: return nullptr;
    }
}

}  // namespace emu
