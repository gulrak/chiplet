//---------------------------------------------------------------------------------------
//
//  octo/lexer.hpp
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
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <chiplet/compileresult.hpp>

namespace octo {

#define TOKEN_LIST(decl) \
    decl(ASSIGN, ":=", OPERATOR) \
    decl(ASSIGN_OR, "|=", OPERATOR) \
    decl(ASSIGN_END, "&=", OPERATOR) \
    decl(ASSIGN_XOR, "^=", OPERATOR) \
    decl(ASSIGN_SUB, "-=", OPERATOR) \
    decl(ASSIGN_RSUB, "=-", OPERATOR) \
    decl(ASSIGN_ADD, "+=", OPERATOR) \
    decl(ASSIGN_SHR, ">>=", OPERATOR) \
    decl(ASSIGN_SHL, "<<=", OPERATOR) \
    decl(EQUAL, "==", OPERATOR) \
    decl(UNEQUAL, "!=", OPERATOR) \
    decl(LESS, "<", OPERATOR) \
    decl(GREATER, ">", OPERATOR) \
    decl(LESS_EQUAL, "<=", OPERATOR) \
    decl(GREATER_EQUAL, ">=", OPERATOR) \
    decl(KEY, "key", KEYWORD) \
    decl(NOT_KEY, "-key", KEYWORD) \
    decl(HEX, "hex", KEYWORD) \
    decl(BIGHEX, "bighex", KEYWORD) \
    decl(RANDOM, "random", KEYWORD) \
    decl(DELAY, "delay", KEYWORD) \
    decl(COLON, ":", DIRECTIVE) \
    decl(NEXT, ":next", DIRECTIVE) \
    decl(UNPACK, ":unpack", DIRECTIVE) \
    decl(BREAKPOINT, ":breakpoint", DIRECTIVE) \
    decl(PROTO, ":proto", DIRECTIVE) \
    decl(ALIAS, ":alias", DIRECTIVE) \
    decl(CONST, ":const", DIRECTIVE) \
    decl(ORG, ":org", DIRECTIVE) \
    decl(SEMICOLON, ";", OPERATOR) \
    decl(AGAIN, "again", KEYWORD) \
    decl(AUDIO, "audio", KEYWORD) \
    decl(BCD, "bcd", KEYWORD) \
    decl(BEGIN, "begin", KEYWORD) \
    decl(BUZZER, "buzzer", KEYWORD) \
    decl(CLEAR, "clear", KEYWORD) \
    decl(ELSE, "else", KEYWORD) \
    decl(END, "end", KEYWORD) \
    decl(EXIT, "exit", KEYWORD) \
    decl(HIRES, "hires", KEYWORD) \
    decl(IF, "if", KEYWORD) \
    decl(I_REG, "i", KEYWORD) \
    decl(JUMP, "jump", KEYWORD) \
    decl(JUMP0, "jump0", KEYWORD) \
    decl(LOAD, "load", KEYWORD) \
    decl(LOADFLAGS, "loadflags", KEYWORD) \
    decl(LOOP, "loop", KEYWORD) \
    decl(LORES, "lores", KEYWORD) \
    decl(NATIVE, "native", KEYWORD) \
    decl(PITCH, "pitch", KEYWORD) \
    decl(PLANE, "plane", KEYWORD) \
    decl(RETURN, "return", KEYWORD) \
    decl(SAVE, "save", KEYWORD) \
    decl(SAVEFLAGS, "saveflags", KEYWORD) \
    decl(SCROLL_DOWN, "scroll-down", KEYWORD) \
    decl(SCROLL_LEFT, "scroll-left", KEYWORD) \
    decl(SCROLL_RIGHT, "scroll-right", KEYWORD) \
    decl(SCROLL_UP, "scroll-up", KEYWORD) \
    decl(SPRITE, "sprite", KEYWORD) \
    decl(THEN, "then", KEYWORD) \
    decl(WHILE, "while", KEYWORD) \
    decl(ASSERT, ":assert", DIRECTIVE) \
    decl(BYTE, ":byte", DIRECTIVE) \
    decl(CALC, ":calc", DIRECTIVE) \
    decl(CALL, ":call", DIRECTIVE) \
    decl(MACRO, ":macro", DIRECTIVE) \
    decl(MONITOR, ":monitor", DIRECTIVE) \
    decl(POINTER, ":pointer", DIRECTIVE) \
    decl(POINTER16, ":pointer16", DIRECTIVE) \
    decl(POINTER24, ":pointer24", DIRECTIVE) \
    decl(STRINGMODE, ":stringmode", DIRECTIVE) \
    decl(INCLUDE, ":include", PREPROCESSOR) \
    decl(SEGMENT, ":segment", PREPROCESSOR) \
    decl(PREPROCESSOR_IF, ":if", PREPROCESSOR) \
    decl(PREPROCESSOR_ELSE, ":else", PREPROCESSOR) \
    decl(PREPROCESSOR_END, ":end", PREPROCESSOR) \
    decl(UNLESS, ":unless", PREPROCESSOR) \
    decl(DUMP_OPTIONS, ":dump-options", PREPROCESSOR) \
    decl(CONFIG, ":config", PREPROCESSOR) \
    decl(ASM, ":asm", PREPROCESSOR) \

enum class TokenId {
    TOK_UNKNOWN,
    STRING_LITERAL,
#define ENUM_ENTRY(NAME, TEXT, TYPE) NAME,
    TOKEN_LIST(ENUM_ENTRY)
#undef ENUM_ENTRY
};

class Token
{
public:
    enum class Type {
        STRING,
        NUMBER,
        IDENTIFIER,
        DIRECTIVE,
        OPERATOR,
        KEYWORD,
        PREPROCESSOR,
        LCURLY,
        RCURLY,
        LSQUARE,
        RSQUARE,
        END_OF_FILE
    };
    Token() = delete;
    Token(int line, int pos);
    explicit Token(int n);
    Token(const Token& other);
    Token& operator=(const Token& other);
    std::string formatValue() const;
    bool isText() const { return type != Type::NUMBER && type != Type::END_OF_FILE; }
    Type type;
    TokenId tid{TokenId::TOK_UNKNOWN};
    int line;
    int pos;
    std::string_view strValue{};
    std::string strContainer;
    std::string_view rawValue{};
    std::string_view prefix{};
    int prefixLine{};
    int prefixPos{};
    double numValue{};
};

struct LexerOptions
{
    bool preprocessorMode{false};
};

bool isKnownToken(std::string_view name);
bool isPreprocessorDirective(std::string_view name);

class Lexer
{
public:
    struct Exception : public std::exception {
        explicit Exception(std::string  message) : errorMessage(std::move(message)) {}
        ~Exception() noexcept override = default;
        const char* what() const noexcept override { return errorMessage.c_str(); }
        std::string errorMessage;
    };
    Lexer() = delete;
    explicit Lexer(std::string_view text, LexerOptions lexerOptions = {});
    explicit Lexer(std::string_view text, Lexer* parent, LexerOptions lexerOptions = {});
    explicit Lexer(std::string filename, std::string_view text, Lexer* parent = nullptr, LexerOptions lexerOptions = {});
    char nextChar();
    char peekChar() const;
    void skipWhitespace(bool resetPrefixAfterNewline = false);
    void scanNextToken(Token& t, bool resetPrefixAfterNewline = false); //< external token scanning

    Token::Type nextToken(bool resetPrefixAfterNewline = false); //< internal token lexing
    const Token& token() const { return *_token; }
    bool expect(std::string_view literal) const { return _token->rawValue == literal; }
    void errorLocation(emu::CompileResult& result) const;
    std::vector<std::pair<int,std::string>> locationStack() const;
    const std::string& filename() const { return _filename; }

protected:
    Lexer* _parent{nullptr};
    std::string _filename;
    const char* _source;
    const char* _sourceRoot;
    const char* _sourceEnd;
    Token* _token{nullptr};
    int _sourceLine;
    int _sourcePos;
    LexerOptions _options;
    char _isError{};
    std::string _error{};
    int _errorLine{};
    int _errorPos{};
private:
    Token _internalToken{0, 0};
};

}
