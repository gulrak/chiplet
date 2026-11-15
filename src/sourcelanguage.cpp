//---------------------------------------------------------------------------------------
// src/sourcelanguage.cpp
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
#include <chiplet/sourcelanguage.hpp>
#include <chiplet/sourcesyntax.hpp>

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string>
#include <unordered_set>
#include <vector>

namespace emu {

namespace {

// The lines looked at, enough to decide, also for big sources
constexpr int MAX_LINES = 4000;

using WordSet = std::unordered_set<std::string_view>;

const WordSet OCTO_DIRECTIVES{":const", ":alias", ":macro", ":calc", ":byte", ":org", ":next", ":unpack", ":breakpoint", ":monitor",
                              ":assert", ":call", ":include", ":if", ":else", ":end", ":unless", ":segment", ":dump-options",
                              ":stringmode", ":pointer", ":asm", ":config"};
const WordSet OCTO_STATEMENTS{"loop", "again", "sprite", "clear", "return", "jump", "jump0", "bcd", "save", "load", "hires", "lores",
                              "scroll-down", "scroll-up", "scroll-left", "scroll-right", "plane", "audio", "exit", "pitch",
                              "saveflags", "loadflags", "delay", "buzzer", "native"};
const WordSet OCTO_ASSIGNMENTS{":=", "+=", "-=", "=-", "|=", "&=", "^=", ">>=", "<<="};

// Without a size suffix only words no other CPU here uses count, "add" or "ret" alone could be 1802 or 6800 code
const WordSet SATURN_SHARED_WORDS{"add", "and", "call", "clr", "dec", "in", "inc", "jump", "neg", "not", "or", "out", "pop", "push", "ret", "sub"};

std::string toUpper(std::string_view text)
{
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

std::string toLower(std::string_view text)
{
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::vector<std::string_view> splitWords(std::string_view text)
{
    std::vector<std::string_view> words;
    size_t pos = 0;
    while (pos < text.size()) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;
        const auto start = pos;
        while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;
        if (pos > start)
            words.push_back(text.substr(start, pos - start));
    }
    return words;
}

bool isOctoRegister(std::string_view word)
{
    return word == "i" || (word.size() == 2 && (word[0] == 'v' || word[0] == 'V') && std::isxdigit(static_cast<unsigned char>(word[1])));
}

bool isOctoStatement(std::string_view line)
{
    line = line.substr(0, line.find('#'));
    const auto words = splitWords(line);
    for (size_t i = 0; i < words.size(); ++i) {
        const auto word = words[i];
        if ((word == ":" && i + 1 < words.size()) || OCTO_DIRECTIVES.contains(word))
            return true;
        if (i > 0 && OCTO_ASSIGNMENTS.contains(word) && isOctoRegister(words[i - 1]))
            return true;
    }
    if (words.empty())
        return false;
    if (OCTO_STATEMENTS.contains(words.front()))
        return true;
    return words.front() == "if" && std::ranges::any_of(words, [](std::string_view word) { return word == "then" || word == "begin"; });
}

// The mnemonic of a statement of the classic assemblers: a label ("NAME:", or any name in the first column that isn't a
// mnemonic) is skipped
template <typename IsMnemonic>
bool hasMnemonic(std::string_view statement, bool startsInFirstColumn, IsMnemonic isMnemonic)
{
    const auto words = splitWords(statement);
    size_t index = 0;
    if (index < words.size() && words[index].ends_with(':'))
        ++index;
    else if (startsInFirstColumn && index < words.size() && !isMnemonic(words, index))
        ++index;
    return index < words.size() && isMnemonic(words, index);
}

bool isCdp1802Statement(std::string_view line)
{
    if (const auto comment = line.find(".."); comment != std::string_view::npos)
        line = line.substr(0, comment);
    // ";" separates statements in the RCA syntax, one known statement is enough for the line
    bool firstStatement = true;
    while (!line.empty()) {
        const auto end = line.find(';');
        const auto statement = line.substr(0, end);
        const bool firstColumn = firstStatement && !statement.empty() && !std::isspace(static_cast<unsigned char>(statement.front()));
        if (hasMnemonic(statement, firstColumn, [](const auto& words, size_t index) {
                const auto& syntax = *SourceSyntax::forLanguage(SourceLanguage::CDP1802);
                const auto name = syntax.nameOf(words[index]);
                return !name.empty() && (syntax.isMnemonic(name) || syntax.isDataDirective(name));
            }))
            return true;
        if (end == std::string_view::npos)
            break;
        line = line.substr(end + 1);
        firstStatement = false;
    }
    return false;
}

bool isM6800Statement(std::string_view line)
{
    if (line.starts_with('*'))
        return false;
    line = line.substr(0, line.find(';'));
    const bool firstColumn = !line.empty() && !std::isspace(static_cast<unsigned char>(line.front()));
    return hasMnemonic(line, firstColumn, [](const auto& words, size_t index) {
        const auto& syntax = *SourceSyntax::forLanguage(SourceLanguage::M6800);
        const auto name = syntax.nameOf(words[index]);
        if (name.empty())
            return false;
        if (syntax.isMnemonic(name) || syntax.isDataDirective(name))
            return true;
        // "LDA A", the accumulator as a word of its own
        if (syntax.isRegisterOperandMnemonic(name) && index + 1 < words.size()) {
            const auto reg = toUpper(syntax.nameOf(words[index + 1]));
            return reg == "A" || reg == "B";
        }
        return false;
    });
}

bool isSaturnMnemonic(std::string_view word)
{
    const auto& syntax = *SourceSyntax::forLanguage(SourceLanguage::SATURN);
    const auto lower = toLower(word);
    const auto dot = lower.find('.');
    if (dot != std::string::npos) {
        const auto base = std::string_view(lower).substr(0, dot);
        return dot > 0 && dot + 1 < lower.size() && (syntax.isMnemonic(base) || syntax.isDataDirective(base));
    }
    return syntax.isMnemonic(lower) && !SATURN_SHARED_WORDS.contains(lower);
}

bool isSaturnStatement(std::string_view line)
{
    line = line.substr(0, line.find(';'));
    if (line.find(":=") != std::string_view::npos)
        return false;  // an assignment
    const bool firstColumn = !line.empty() && !std::isspace(static_cast<unsigned char>(line.front()));
    return hasMnemonic(line, firstColumn, [](const auto& words, size_t index) { return isSaturnMnemonic(words[index]); });
}

size_t languageIndex(SourceLanguage language)
{
    return static_cast<size_t>(language);
}

}  // namespace

const char* sourceLanguageName(SourceLanguage language)
{
    switch (language) {
        case SourceLanguage::OCTO: return "Octo";
        case SourceLanguage::CDP1802: return "CDP1802";
        case SourceLanguage::M6800: return "M6800";
        case SourceLanguage::SATURN: return "Saturn";
        default: return "unknown";
    }
}

const char* sourceLanguageId(SourceLanguage language)
{
    switch (language) {
        case SourceLanguage::OCTO: return "octo";
        case SourceLanguage::CDP1802: return "cdp1802";
        case SourceLanguage::M6800: return "m6800";
        case SourceLanguage::SATURN: return "saturn";
        default: return "unknown";
    }
}

std::optional<SourceLanguage> sourceLanguageFromId(std::string_view id)
{
    for (auto language : {SourceLanguage::OCTO, SourceLanguage::CDP1802, SourceLanguage::M6800, SourceLanguage::SATURN}) {
        if (id == sourceLanguageId(language))
            return language;
    }
    return std::nullopt;
}

SourceLanguage sourceLanguageFromExtension(std::string_view filename)
{
    const auto slash = filename.find_last_of("/\\");
    const auto name = slash == std::string_view::npos ? filename : filename.substr(slash + 1);
    const auto dot = name.rfind('.');
    if (dot == std::string_view::npos || dot == 0)
        return SourceLanguage::UNKNOWN;
    const auto extension = toLower(name.substr(dot));
    if (extension == ".8o")
        return SourceLanguage::OCTO;
    if (extension == ".asap")
        return SourceLanguage::SATURN;
    return SourceLanguage::UNKNOWN;
}

SourceLanguageGuess detectSourceLanguageFromText(std::string_view text)
{
    SourceLanguageGuess guess;
    int lines = 0;
    while (!text.empty() && lines++ < MAX_LINES) {
        const auto end = text.find('\n');
        auto line = text.substr(0, end);
        if (line.ends_with('\r'))
            line.remove_suffix(1);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        if (isOctoStatement(line))
            ++guess.statements[languageIndex(SourceLanguage::OCTO)];
        if (isCdp1802Statement(line))
            ++guess.statements[languageIndex(SourceLanguage::CDP1802)];
        if (isM6800Statement(line))
            ++guess.statements[languageIndex(SourceLanguage::M6800)];
        if (isSaturnStatement(line))
            ++guess.statements[languageIndex(SourceLanguage::SATURN)];
    }
    // the clear winner: at least twice the statements of every other language
    size_t best = 0;
    for (size_t i = 1; i < NUM_SOURCE_LANGUAGES; ++i) {
        if (guess.statements[i] > guess.statements[best])
            best = i;
    }
    const auto bestCount = guess.statements[best];
    const bool clear = bestCount > 0 && std::ranges::all_of(std::views::iota(size_t{1}, NUM_SOURCE_LANGUAGES), [&](size_t i) {
        return i == best || bestCount >= 2 * guess.statements[i];
    });
    guess.language = clear ? static_cast<SourceLanguage>(best) : SourceLanguage::UNKNOWN;
    return guess;
}

SourceLanguageGuess detectSourceLanguage(std::string_view filename, std::string_view text)
{
    if (const auto language = sourceLanguageFromExtension(filename); language != SourceLanguage::UNKNOWN) {
        SourceLanguageGuess guess;
        guess.language = language;
        guess.byExtension = true;
        return guess;
    }
    return detectSourceLanguageFromText(text);
}

}  // namespace emu
