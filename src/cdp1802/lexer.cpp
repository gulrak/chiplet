#include "lexer.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fmt/format.h>

namespace cdp1802 {

namespace {

bool isIdentifierStart(char ch)
{
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool isIdentifierContinue(char ch)
{
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

std::string upper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return text;
}

int digitValue(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}

} // namespace

Lexer::Lexer(std::string_view text)
    : _source{text}
{
    scan();
}

char Lexer::peek(size_t offset) const
{
    return _offset + offset < _source.size() ? _source[_offset + offset] : '\0';
}

char Lexer::advance()
{
    const auto ch = peek();
    if (ch == '\0')
        return ch;
    ++_offset;
    if (ch == '\n') {
        ++_line;
        _column = 1;
    }
    else {
        ++_column;
    }
    return ch;
}

void Lexer::addToken(TokenType type, std::string text, int value)
{
    _tokens.push_back(Token{type, std::move(text), value, _tokenLine, _tokenColumn});
}

void Lexer::fail(std::string message)
{
    if (_errorMessage.empty())
        _errorMessage = fmt::format("{} at {}:{}", message, _tokenLine, _tokenColumn);
}

void Lexer::scan()
{
    while (peek() != '\0' && _errorMessage.empty()) {
        _tokenLine = _line;
        _tokenColumn = _column;
        const auto ch = peek();
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            advance();
        }
        else if (ch == '\n') {
            advance();
            addToken(TokenType::END_OF_LINE);
        }
        else if (ch == '.' && peek(1) == '.') {
            while (peek() != '\0' && peek() != '\n')
                advance();
        }
        else if (isIdentifierStart(ch)) {
            scanIdentifier();
        }
        else if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '#') {
            scanNumber();
        }
        else if (ch == '\'') {
            scanStringOrBasedNumber();
        }
        else {
            advance();
            switch (ch) {
                case ':': addToken(TokenType::COLON, ":"); break;
                case ',': addToken(TokenType::COMMA, ","); break;
                case ';': addToken(TokenType::SEMICOLON, ";"); break;
                case '=': addToken(TokenType::EQUAL, "="); break;
                case '+': addToken(TokenType::PLUS, "+"); break;
                case '-': addToken(TokenType::MINUS, "-"); break;
                case '.': addToken(TokenType::DOT, "."); break;
                case '(': addToken(TokenType::LPAREN, "("); break;
                case ')': addToken(TokenType::RPAREN, ")"); break;
                case '*':
                case '$': addToken(TokenType::STAR, std::string(1, ch)); break;
                default: fail(fmt::format("Unexpected character '{}'", ch)); break;
            }
        }
    }
    _tokenLine = _line;
    _tokenColumn = _column;
    addToken(TokenType::END_OF_FILE);
}

void Lexer::scanIdentifier()
{
    const auto start = _offset;
    while (isIdentifierContinue(peek()))
        advance();
    auto text = std::string{_source.substr(start, _offset - start)};
    if ((text == "X" || text == "x" || text == "B" || text == "b" || text == "D" || text == "d" || text == "T" || text == "t") && peek() == '\'') {
        scanStringOrBasedNumber();
        return;
    }
    addToken(TokenType::IDENTIFIER, upper(std::move(text)));
}

void Lexer::scanNumber()
{
    int base = 10;
    if (peek() == '#') {
        base = 16;
        advance();
    }

    const auto start = _offset;
    while (std::isxdigit(static_cast<unsigned char>(peek())))
        advance();
    if (start == _offset) {
        fail("Expected digits");
        return;
    }

    int value = 0;
    for (size_t index = start; index < _offset; ++index) {
        const auto digit = digitValue(_source[index]);
        if (digit < 0 || digit >= base) {
            fail("Invalid digit in number");
            return;
        }
        value = value * base + digit;
    }
    addToken(TokenType::NUMBER, std::string{_source.substr(start, _offset - start)}, value);
}

void Lexer::scanStringOrBasedNumber()
{
    char prefix = '\0';
    if (_offset > 0)
        prefix = static_cast<char>(std::toupper(static_cast<unsigned char>(_source[_offset - 1])));

    if (peek() != '\'') {
        fail("Expected string delimiter");
        return;
    }
    advance();

    std::string text;
    while (peek() != '\0' && peek() != '\n' && peek() != '\'') {
        if (peek() == '\\') {
            advance();
            if (peek() == '\0' || peek() == '\n')
                break;
        }
        text.push_back(advance());
    }
    if (peek() != '\'') {
        fail("Unterminated string literal");
        return;
    }
    advance();

    if (prefix == 'X' || prefix == 'B' || prefix == 'D') {
        const int base = prefix == 'X' ? 16 : prefix == 'B' ? 2 : 10;
        int value = 0;
        for (char ch : text) {
            const auto digit = digitValue(ch);
            if (digit < 0 || digit >= base) {
                fail("Invalid digit in based number");
                return;
            }
            value = value * base + digit;
        }
        addToken(TokenType::NUMBER, text, value);
    }
    else if (prefix == 'T') {
        addToken(TokenType::STRING, text);
    }
    else if (text.size() == 1) {
        addToken(TokenType::NUMBER, text, static_cast<unsigned char>(text.front()));
    }
    else {
        fail("Character literal must contain exactly one character");
    }
}

} // namespace cdp1802
