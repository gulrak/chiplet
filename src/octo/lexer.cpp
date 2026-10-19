//---------------------------------------------------------------------------------------
//
//  octo/lexer.cpp
//
//  A lexer for Octo CHIP-8 assembly language, suitable for embedding in other
//  tools and environments. Compared to its original form it depends heavily on C++20
//  standard library. Due to no more use of any static arrays it has none of the
//  original limitations.
//
//---------------------------------------------------------------------------------------
//
//  C++ Octo Assembler Version with Extensions:
//
//  The MIT License (MIT)
//
//  Copyright (c) 2023, Steffen Schümann
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
//---------------------------------------------------------------------------------------
//
//  Based on original code from 'octo_compiler.h' in C-Octo by John Earnest:
//
//  The MIT License (MIT)
//
//  Copyright (c) 2020, John Earnest
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
//---------------------------------------------------------------------------------------

#include "lexer.hpp"
#include <limits>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <fast_float/fast_float.h>
#include <fmt/format.h>

namespace octo
{

namespace {
struct TokenInfo {
    TokenId id;
    Token::Type type;
};
}

const std::unordered_map<std::string_view, TokenInfo> lexerTokenMap = {
#define TEXT_TOKEN_MAP(NAME, TEXT, TYPE) {TEXT, {TokenId::NAME, Token::Type::TYPE}},
    TOKEN_LIST(TEXT_TOKEN_MAP)
#undef TEXT_TOKEN_MAP
};

bool isKnownToken(std::string_view name)
{
    return lexerTokenMap.count(name);
}

TokenId tokenIdForText(std::string_view text)
{
    const auto iter = lexerTokenMap.find(text);
    return iter == lexerTokenMap.end() ? TokenId::TOK_UNKNOWN : iter->second.id;
}

namespace {

std::optional<double> parseRadixDigits(std::string_view digits, unsigned radix)
{
    if (digits.empty())
        return std::nullopt;
    double value = 0;
    uint64_t significand = 0;
    int significantBits = 0;
    int discardedBits = 0;
    bool roundBit = false;
    bool stickyBit = false;
    for (unsigned char character : digits) {
        unsigned digit = character >= '0' && character <= '9' ? character - '0'
            : character >= 'a' && character <= 'f' ? character - 'a' + 10
            : character >= 'A' && character <= 'F' ? character - 'A' + 10 : radix;
        if (digit >= radix)
            return std::nullopt;
        if (radix == 10) {
            value = value * radix + digit;
            continue;
        }
        // Retain 53 significant bits and round once, rather than rounding
        // after every digit. The latter disagrees with JS for large literals.
        const int digitBits = radix == 2 ? 1 : radix == 8 ? 3 : 4;
        for (int bitIndex = digitBits - 1; bitIndex >= 0; --bitIndex) {
            const auto bit = (digit >> bitIndex) & 1;
            if (significantBits == 0 && bit == 0)
                continue;
            if (significantBits < std::numeric_limits<double>::digits) {
                significand = (significand << 1) | bit;
                ++significantBits;
            }
            else {
                if (discardedBits == 0)
                    roundBit = bit;
                else
                    stickyBit = stickyBit || bit;
                if (discardedBits < 1024)
                    ++discardedBits;
            }
        }
    }
    if (radix == 10)
        return value;
    if (roundBit && (stickyBit || (significand & 1)))
        ++significand;
    return std::ldexp(static_cast<double>(significand), discardedBits);
}

std::optional<double> parseDecimalNumber(std::string_view text)
{
    if (text.starts_with('+')) {
        text.remove_prefix(1);
        if (text.starts_with('-') || text.starts_with('+'))
            return std::nullopt;
    }
    if (text.empty())
        return std::nullopt;
    double value = 0;
    const auto result = fast_float::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ptr != text.data() + text.size()
        || (result.ec != std::errc{} && result.ec != std::errc::result_out_of_range))
        return std::nullopt;
    return value;
}

} // namespace

std::optional<double> parseIntegerLiteral(std::string_view text)
{
    const auto original = text;
    const bool negative = text.starts_with('-');
    if (negative || text.starts_with('+'))
        text.remove_prefix(1);
    unsigned radix = 10;
    if (text.starts_with("0b"))
        radix = 2, text.remove_prefix(2);
    else if (text.starts_with("0x") || text.starts_with("0X"))
        radix = 16, text.remove_prefix(2);
    auto value = parseRadixDigits(text, radix);
    if (!value)
        return std::nullopt;
    if (radix == 10)
        return parseDecimalNumber(original);
    return negative ? -*value : *value;
}

std::optional<double> parseCalculatedNumber(std::string_view text)
{
    // Octo's expression terminals use JavaScript Number(), independently of
    // the integer-only token grammar used for ordinary operands.
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    if (text.empty())
        return 0;
    if (text == "Infinity" || text == "+Infinity")
        return std::numeric_limits<double>::infinity();
    if (text == "-Infinity")
        return -std::numeric_limits<double>::infinity();
    if (text.starts_with("0x") || text.starts_with("0X"))
        return parseRadixDigits(text.substr(2), 16);
    if (text.starts_with("0b") || text.starts_with("0B"))
        return parseRadixDigits(text.substr(2), 2);
    if (text.starts_with("0o") || text.starts_with("0O"))
        return parseRadixDigits(text.substr(2), 8);
    if (text.find_first_not_of("0123456789.eE+-") != std::string_view::npos)
        return std::nullopt;
    return parseDecimalNumber(text);
}

bool isPreprocessorDirective(std::string_view name)
{
    auto iter = lexerTokenMap.find(name);
    return iter != lexerTokenMap.end() && iter->second.type == Token::Type::PREPROCESSOR;
}

Token::Token(int line, int pos)
    : type{Type::END_OF_FILE}
    , line{line}
    , pos{pos}
{
}

Token::Token(int n)
    : type{Type::NUMBER}
    , line{0}
    , pos{0}
{
    numValue = n;
}

Token::Token(const Token& other)
    : type{other.type}
    , tid(other.tid)
    , line{other.line}
    , pos{other.pos}
    , rawValue{other.rawValue}
    , prefix{other.prefix}
    , prefixLine{other.prefixLine}
    , prefixPos{other.prefixPos}
    , nativeCpu{other.nativeCpu}
    , nativeLine{other.nativeLine}
    , nativePos{other.nativePos}
{
    if (type == Type::NUMBER) {
        numValue = other.numValue;
    }
    else {
        if (!other.strContainer.empty() && other.strValue.data() == other.strContainer.data()) {
            strContainer = other.strContainer;
            strValue = strContainer;
        }
        else {
            strValue = other.strValue;
        }
    }
}

Token& Token::operator=(const Token& other)
{
    type = other.type;
    tid = other.tid;
    line = other.line;
    pos = other.pos;
    rawValue = other.rawValue;
    prefix = other.prefix;
    prefixLine = other.prefixLine;
    prefixPos = other.prefixPos;
    nativeCpu = other.nativeCpu;
    nativeLine = other.nativeLine;
    nativePos = other.nativePos;
    if (type == Type::NUMBER) {
        numValue = other.numValue;
    }
    else {
        if (!other.strContainer.empty() && other.strValue.data() == other.strContainer.data()) {
            strContainer = other.strContainer;
            strValue = strContainer;
        }
        else {
            strValue = other.strValue;
        }
    }
    return *this;
}

std::string Token::formatValue() const
{
    switch (type) {
        case Type::END_OF_FILE:
            return "<end of file>";
        case Type::STRING:
        case Type::IDENTIFIER:
        case Type::DIRECTIVE:
        case Type::OPERATOR:
        case Type::KEYWORD:
        case Type::PREPROCESSOR:
        case Type::NATIVE_BLOCK:
        case Type::LCURLY:
        case Type::RCURLY:
        case Type::LSQUARE:
        case Type::RSQUARE:
            return fmt::format("'{}'", strValue);
        case Type::NUMBER:
            return fmt::format("{}", static_cast<int>(numValue));
        default:
            return "unknown type";
    }
}

Lexer::Lexer(std::string_view text, LexerOptions lexerOptions)
    : _options{lexerOptions}
{
    _token = &_internalToken;
    _source = text.data();
    _sourceRoot = text.data();
    _sourceEnd = _source + text.length();
    _sourceLine = 0;
    _sourcePos = 0;
    _isError = 0;
    _error.clear();
    _errorLine = 0;
    _errorPos = 0;
}
Lexer::Lexer(std::string_view text, Lexer* parent, LexerOptions lexerOptions)
    : Lexer(text, lexerOptions)
{
    _parent = parent;
}

Lexer::Lexer(std::string filename, std::string_view text, Lexer* parent, LexerOptions lexerOptions)
    : Lexer(text, parent, lexerOptions)
{
    _filename = std::move(filename);
}

char Lexer::nextChar()
{
    if(_source >= _sourceEnd)
        return 0;
    char c = _source[0];
    if (c == '\n')
        _sourceLine++, _sourcePos = 0;
    else
        _sourcePos++;
    _source++;
    return c;
}

char Lexer::peekChar() const
{
    return _source >= _sourceEnd ? '\0' : _source[0];
}

void Lexer::skipWhitespace(bool resetPrefixAfterNewline)
{
    while (true) {
        char c = peekChar();
        if (c == '#') {
            nextChar();
            while (true) {
                char cc = peekChar();
                if (cc == '\0' || cc == '\n')
                    break;
                nextChar();
            }
        }
        else if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\v')
            break;
        nextChar();
    }
}

void Lexer::scanNextToken(octo::Token& t, bool resetPrefixAfterNewline)
{
    std::string strBuffer;
    const auto* prefixStart = _source;
    int prefixLine = _sourceLine;
    int prefixPos = _sourcePos;
    skipWhitespace(resetPrefixAfterNewline);
    if (resetPrefixAfterNewline) {
        auto prefix = std::string_view{prefixStart, static_cast<size_t>(_source - prefixStart)};
        auto firstNewline = prefix.find('\n');
        if (firstNewline != std::string_view::npos) {
            prefixStart += firstNewline + 1;
            prefixLine += 1;
            prefixPos = 0;
        }
    }
    if (_options.preprocessorMode) {
        t.prefix = {prefixStart, static_cast<size_t>(_source - prefixStart)};
        t.prefixLine = prefixLine;
        t.prefixPos = prefixPos;
    }
    else {
        t.prefix = {};
        t.prefixLine = 0;
        t.prefixPos = 0;
    }
    const auto* start = _source;
    size_t index = 0;
    t.tid = TokenId::TOK_UNKNOWN;
    t.nativeCpu = {};
    t.nativeLine = t.nativePos = 0;
    t.strValue = {};
    t.strContainer.clear();
    t.line = _sourceLine;
    t.pos = _sourcePos;
    if (_source >= _sourceEnd || peekChar() == '\0') {
        _source = _sourceEnd;
        t.type = Token::Type::END_OF_FILE;
        t.tid = TokenId::TOK_UNKNOWN;
        t.rawValue = {};
        return;
    }
    if (_source[0] == '"') {
        nextChar();
        while (true) {
            char c = nextChar();
            if (c == '\0') {
                _isError = 1;
                _error = "Missing a closing \" in a string literal.";
                _errorLine = _sourceLine, _errorPos = _sourcePos;
                return;
            }
            if (c == '"') {
                break;
            }
            if (c == '\\') {
                start = nullptr;
                char ec = nextChar();
                if (ec == '\0') {
                    _isError = 1;
                    _error = "Missing a closing \" in a string literal.";
                    _errorLine = _sourceLine, _errorPos = _sourcePos;
                    return;
                }
                if (ec == 't')
                    strBuffer.push_back('\t');
                else if (ec == 'n')
                    strBuffer.push_back('\n');
                else if (ec == 'r')
                    strBuffer.push_back('\r');
                else if (ec == 'v')
                    strBuffer.push_back('\v');
                else if (ec == '0')
                    strBuffer.push_back('\0');
                else if (ec == '\\')
                    strBuffer.push_back('\\');
                else if (ec == '"')
                    strBuffer.push_back('"');
                else {
                    _isError = 1;
                    _error = fmt::format("Unrecognized escape character '{}' in a string literal.", ec);
                    _errorLine = _sourceLine, _errorPos = _sourcePos - 1;
                    return;
                }
            }
            else {
                strBuffer.push_back(c);
            }
        }
        t.type = Token::Type::STRING;
        t.tid = TokenId::STRING_LITERAL;
        t.rawValue = {t.line == _sourceLine ? _source - strBuffer.length() - 2 : nullptr, 0};
        t.rawValue = {prefixStart + t.prefix.size(), static_cast<size_t>(_source - (prefixStart + t.prefix.size()))};
        if(start) {
            t.strValue = std::string_view(start + 1, strBuffer.length());
        }
        else {
            t.strContainer = std::move(strBuffer);
            t.strValue = t.strContainer;
        }
        if (!_options.preprocessorMode)
            skipWhitespace();
    }
    else {
        while (true) {
            char c = peekChar();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '#' || c == '"' || c == '\0')
                break;
            nextChar();
            ++index;
        }
        if ((index == 1 || _options.preprocessorMode) && (start[0] == '{'|| start[0] == '['|| start[0] == '}'|| start[0] == ']' )) {
            if(*start == '{')
                t.type = Token::Type::LCURLY;
            if(*start == '}')
                t.type = Token::Type::RCURLY;
            if (*start == '[')
                t.type = Token::Type::LSQUARE;
            if (*start == ']')
                t.type = Token::Type::RSQUARE;
            t.tid = TokenId::TOK_UNKNOWN;
            t.strValue = {start, index};
            t.rawValue = {start, index};
            if (!_options.preprocessorMode)
                skipWhitespace();
            return;
        }
        if (!index) {
            t.type = Token::Type::END_OF_FILE;
            t.rawValue = {};
            return;
        }
        const auto text = std::string_view{start, index};
        auto number = parseIntegerLiteral(text);
        if (!number && _options.preprocessorMode
            && text.find_first_not_of("0123456789.eE+-") == std::string_view::npos)
            number = parseDecimalNumber(text);
        if (number) {
            t.type = Token::Type::NUMBER;
            t.numValue = *number;
        }
        else {
            t.type = Token::Type::IDENTIFIER;
            t.strValue = text;
            auto iter = lexerTokenMap.find(text);
            if (iter != lexerTokenMap.end()) {
                t.tid = iter->second.id;
                t.type = iter->second.type;
                if (t.type == Token::Type::PREPROCESSOR) {
                    while (!t.prefix.empty() && (t.prefix.back() == ' ' || t.prefix.back() == '\t'))
                        t.prefix.remove_suffix(1);
                }
            }
        }
        t.rawValue = {start, index};
        if (t.tid == TokenId::ASM) {
            scanNativeBlock(t, start);
            return;
        }
        if (!_options.preprocessorMode)
            skipWhitespace();
    }
}

void Lexer::scanNativeBlock(Token& token, const char* start)
{
    const auto fail = [&](std::string message) {
        _isError = 1;
        _error = std::move(message);
        _errorLine = token.line;
        _errorPos = token.pos;
    };
    skipWhitespace();
    const auto* cpuStart = _source;
    while (std::isalnum(static_cast<unsigned char>(peekChar())) || peekChar() == '_')
        nextChar();
    token.nativeCpu = {cpuStart, static_cast<size_t>(_source - cpuStart)};
    if (token.nativeCpu.empty()) {
        fail("Expected CPU name after :asm.");
        return;
    }
    skipWhitespace();
    if (peekChar() != '{') {
        fail("Expected '{' after :asm CPU name.");
        return;
    }
    nextChar();
    const auto* body = _source;
    token.nativeLine = _sourceLine;
    token.nativePos = _sourcePos;
    while (peekChar()) {
        if (peekChar() == '.' && _source + 1 < _sourceEnd && _source[1] == '.') {
            while (peekChar() && peekChar() != '\n')
                nextChar();
        }
        else if (peekChar() == '\'') {
            nextChar();
            while (peekChar() && peekChar() != '\'') {
                if (nextChar() == '\\' && peekChar())
                    nextChar();
            }
            if (peekChar())
                nextChar();
        }
        else if (peekChar() == '}') {
            token.strValue = {body, static_cast<size_t>(_source - body)};
            nextChar();
            token.rawValue = {start, static_cast<size_t>(_source - start)};
            if (!_options.preprocessorMode)
                skipWhitespace();
            return;
        }
        else if (peekChar() == '{') {
            fail("Nested :asm blocks are not supported.");
            return;
        }
        else {
            nextChar();
        }
    }
    fail("Unterminated :asm block.");
}

Token::Type Lexer::nextToken(bool resetPrefixAfterNewline)
{
    scanNextToken(_internalToken, resetPrefixAfterNewline);
    if (_isError)
        throw Exception(_error);
    return _internalToken.type;
}

void Lexer::errorLocation(emu::CompileResult& cr) const
{
    auto* parent = _parent;
    cr.locations.clear();
    cr.locations.push_back({_filename, static_cast<int>(_token->line + 1), static_cast<int>(_token->pos), emu::CompileResult::Location::eROOT});
    //std::string includes;
    while (parent) {
        cr.locations.push_back({parent->_filename, static_cast<int>(parent->_token->line + 1), static_cast<int>(parent->_token->pos), emu::CompileResult::Location::eINCLUDED});
        //includes = fmt::format("{}:{}:{}: info: Included from\n", parent->_filename, parent->_token.line, parent->_token.column) + includes;
        parent = parent->_parent;
    }
    //return fmt::format("{}{}:{}:{}: ", includes, _filename, _token.line, _token.column);
}

std::vector<std::pair<int,std::string>> Lexer::locationStack() const
{
    std::vector<std::pair<int,std::string>> result;
    result.reserve(10);
    auto* parent = this;
    while (parent) {
        result.insert(result.begin(), {parent->_token->line + 1, parent->_filename});
        parent = parent->_parent;
    }
    return result;
}
}
