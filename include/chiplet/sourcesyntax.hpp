//---------------------------------------------------------------------------------------
// chiplet/sourcesyntax.hpp
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
#pragma once

#include <chiplet/sourcelanguage.hpp>

#include <string>
#include <string_view>
#include <unordered_set>

namespace emu {

// The way the assembly sources of a language are written, for tools that read them without assembling (the language
// detection, editors highlighting them). The words come from the assemblers' own tables where Chiplet has them: the
// CDP1802 instructions of its assembler, the Saturn mnemonics of its decoder table (the one asacpp assembles with) and
// asacpp's directives; the M6800 list is written down, Chiplet has no assembler for it yet. Words are compared as the
// language does (CDP1802 and M6800 without case, Saturn statements are lower case for asacpp, also compared without
// case here).
struct SourceSyntax
{
    SourceLanguage language{SourceLanguage::UNKNOWN};
    std::string_view lineComment;           // starts a comment anywhere: ".." (CDP1802), ";"
    char firstColumnComment{0};             // a line starting with it is a comment: '*' (M6800)
    char statementSeparator{0};             // more statements on a line: ';' (CDP1802)
    char directivePrefix{0};                // a word starting with it is a directive: '!' (Saturn)
    bool sizeSuffix{false};                 // mnemonics carry a field suffix: "move.a" (Saturn)
    bool firstColumnLabels{false};          // a first-column word that is no mnemonic or directive is a label
    std::string_view numberPrefixes;        // a word starting with one of them (or a digit) is a number
    std::string_view quotes;                // the quote characters of strings
    std::string_view basedNumberPrefixes;   // a quoted text after one of them is a number: X'1F' (CDP1802)

    // The keywords, compared without case
    bool isMnemonic(std::string_view word) const;
    bool isRegisterOperandMnemonic(std::string_view word) const;  // M6800 "LDA" of "LDA A": a mnemonic with the register word
    bool isDirective(std::string_view word) const;
    bool isDataDirective(std::string_view word) const;            // the directives placing data (DC, FCB, data), also directives
    bool isRegister(std::string_view word) const;

    // The name a statement word starts with, the mnemonic or directive it can be: the letters and digits from the start,
    // "LDI#00" is LDI; with a size suffix the part before the '.' ("move.a" is move); a name continued by '.' or '_' in a
    // language without suffixes is none ("add.a" is no CDP1802 ADD, "inc_bgcol" no INC)
    std::string_view nameOf(std::string_view word) const;

    // The syntax of an assembly language, nullptr for Octo and UNKNOWN
    static const SourceSyntax* forLanguage(SourceLanguage language);

    std::unordered_set<std::string> mnemonics;                  // all in upper case
    std::unordered_set<std::string> registerOperandMnemonics;
    std::unordered_set<std::string> directives;
    std::unordered_set<std::string> dataDirectives;
    std::unordered_set<std::string> registers;
};

}  // namespace emu
