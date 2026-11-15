//---------------------------------------------------------------------------------------
// chiplet/sourcelanguage.hpp
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
// Source language detection for Chiplet and its users
//---------------------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace emu {

// The languages of the sources Chiplet and its users deal with. Only Octo and CDP1802 can be assembled by Chiplet so far,
// the others are recognized so a tool can say what a source is.
enum class SourceLanguage {
    UNKNOWN,
    OCTO,     // Octo with the Chiplet/Octopus extensions, also with :asm cdp1802 blocks
    CDP1802,  // RCA COSMAC Level I assembly (".." comments)
    M6800,    // Motorola 6800 assembly
    SATURN    // HP Saturn in the ASAP syntax of Alonzo Gariepy's mnemonics ("move.a @d0,a"), not HP's SASM notation
};
inline constexpr size_t NUM_SOURCE_LANGUAGES = 5;

struct SourceLanguageGuess
{
    SourceLanguage language{SourceLanguage::UNKNOWN};
    bool byExtension{false};                               // the extension decided it, the text wasn't looked at
    std::array<int, NUM_SOURCE_LANGUAGES> statements{};    // statements recognized per language (index: the enum value)
    int statementsFor(SourceLanguage lang) const { return statements[static_cast<size_t>(lang)]; }
};

// A short name for messages ("Octo", "CDP1802", "M6800", "Saturn"), and the name used on the command line ("octo",
// "cdp1802", "m6800", "saturn"), "unknown" for UNKNOWN
const char* sourceLanguageName(SourceLanguage language);
const char* sourceLanguageId(SourceLanguage language);
std::optional<SourceLanguage> sourceLanguageFromId(std::string_view id);

// The language a file name alone tells: ".8o" is Octo, ".asap" Saturn, everything else (".asm" too) is UNKNOWN
SourceLanguage sourceLanguageFromExtension(std::string_view filename);

// The language of a source: by its extension if that tells, else by its statements. Every line is read the way each
// language would read it (its comments, labels and statement separators) and counted for the languages that know its
// mnemonic or directive; mnemonics several CPUs have count for each of them. The language with at least twice the
// statements of any other wins, otherwise it is UNKNOWN and the caller decides (e.g. by the selected machine).
SourceLanguageGuess detectSourceLanguage(std::string_view filename, std::string_view text);

// The statement counts without looking at the extension, detectSourceLanguage() uses it for unknown extensions
SourceLanguageGuess detectSourceLanguageFromText(std::string_view text);

}  // namespace emu
