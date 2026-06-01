//---------------------------------------------------------------------------------------
// cdp1802/lexer.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2025, Steffen Schümann <s.schuemann@pobox.com>
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
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cdp1802 {

enum class TokenType {
    IDENTIFIER,
    NUMBER,
    STRING,
    COLON,
    COMMA,
    SEMICOLON,
    EQUAL,
    PLUS,
    MINUS,
    DOT,
    LPAREN,
    RPAREN,
    STAR,
    END_OF_LINE,
    END_OF_FILE
};

struct Token
{
    TokenType type{};
    std::string text;
    int value{};
    int line{};
    int column{};
};

class Lexer
{
public:
    explicit Lexer(std::string_view text);

    const std::vector<Token>& tokens() const { return _tokens; }
    bool isError() const { return !_errorMessage.empty(); }
    const std::string& errorMessage() const { return _errorMessage; }

private:
    void scan();
    void scanIdentifier();
    void scanNumber();
    void scanStringOrBasedNumber();
    void addToken(TokenType type, std::string text = {}, int value = 0);
    char peek(size_t offset = 0) const;
    char advance();
    void fail(std::string message);

    std::string_view _source;
    size_t _offset{};
    int _line{1};
    int _column{1};
    int _tokenLine{1};
    int _tokenColumn{1};
    std::vector<Token> _tokens;
    std::string _errorMessage;
};

} // namespace cdp1802
