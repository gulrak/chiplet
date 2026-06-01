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
