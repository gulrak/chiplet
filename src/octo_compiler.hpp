//---------------------------------------------------------------------------------------
//
//  octo_compiler.hpp
//
//  A compiler for Octo CHIP-8 assembly language, suitable for embedding in other
//  tools and environments. Compared to its original form it depends heavily on C++17
//  standard library. It's for sure not as lightweight as the original, but it is
//  quite a bit faster and hopefully easier to extend and due to no more use of any
//  static arrays it has none of the original limitations.
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

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <string_view>
#include <vector>
#include <fmt/format.h>
#include <fast_float/fast_float.h>



namespace octo {

// clang-format: off

#define TOKEN_LIST(decl) \
    decl(ASSIGN, ":=") \
    decl(ASSIGN_OR, "|=") \
    decl(ASSIGN_END, "&=") \
    decl(ASSIGN_XOR, "^=") \
    decl(ASSIGN_SUB, "-=") \
    decl(ASSIGN_RSUB, "=-") \
    decl(ASSIGN_ADD, "+=") \
    decl(ASSIGN_SHR, ">>=") \
    decl(ASSIGN_SHL, "<<=") \
    decl(EQUAL, "==") \
    decl(UNEQUAL, "!=") \
    decl(LESS, "<") \
    decl(GREATER, ">") \
    decl(LESS_EQUAL, "<=") \
    decl(GREATER_EQUAL, ">=") \
    decl(PRE_INCLUDE, ":include") \
    decl(PRE_SEGMENT, ":segment") \
    decl(PRE_IF, ":if") \
    decl(PRE_ELSE, ":else") \
    decl(PRE_END, ":end") \
    decl(PRE_UNLESS, ":unless") \
    decl(PRE_DUMP_OPTIONS, ":dump-options") \
    decl(PRE_CONFIG, ":config") \
    decl(PRE_ASM, ":asm") \
    decl(COLON, ":") \
    decl(NEXT, ":next") \
    decl(UNPACK, ":unpack") \
    decl(BREAKPOINT, ":breakpoint") \
    decl(PROTO, ":proto") \
    decl(ALIAS, ":alias") \
    decl(CONST, ":const") \
    decl(ORG, ":org") \
    decl(MACRO, ":macro") \
    decl(CALC, ":calc") \
    decl(BYTE, ":byte") \
    decl(CALL, ":call") \
    decl(STRINGMODE, ":stringmode") \
    decl(ASSERT, ":assert") \
    decl(MONITOR, ":monitor") \
    decl(POINTER, ":pointer") \
    decl(POINTER16, ":pointer16") \
    decl(POINTER24, ":pointer24") \
    decl(SEMICOLON, ";") \
    decl(KEY, "key") \
    decl(NOT_KEY, "-key") \
    decl(HEX, "hex") \
    decl(BIGHEX, "bighex") \
    decl(RANDOM, "random") \
    decl(DELAY, "delay") \
    decl(RETURN, "return") \
    decl(CLEAR, "clear") \
    decl(BCD, "bcd") \
    decl(SAVE, "save") \
    decl(LOAD, "load") \
    decl(BUZZER, "buzzer") \
    decl(IF, "if") \
    decl(THEN, "then") \
    decl(BEGIN, "begin") \
    decl(ELSE, "else") \
    decl(END, "end") \
    decl(EXIT, "exit") \
    decl(JUMP, "jump") \
    decl(JUMP0, "jump0") \
    decl(NATIVE, "native") \
    decl(SPRITE, "sprite") \
    decl(LOOP, "loop") \
    decl(WHILE, "while") \
    decl(AGAIN, "again") \
    decl(SCROLL_DOWN, "scroll-down") \
    decl(SCROLL_UP, "scroll-up") \
    decl(SCROLL_RIGHT, "scroll-right") \
    decl(SCROLL_LEFT, "scroll-left") \
    decl(LORES, "lores") \
    decl(HIRES, "hires") \
    decl(LOADFLAGS, "loadflags") \
    decl(SAVEFLAGS, "saveflags") \
    decl(I_REG, "i") \
    decl(AUDIO, "audio") \
    decl(PLANE, "plane") \
    decl(PITCH, "pitch")

enum class TokenId {
    TOK_UNKNOWN,
    STRING_LITERAL,
#define ENUM_ENTRY(NAME, TEXT) NAME,
    TOKEN_LIST(ENUM_ENTRY)
#undef ENUM_ENTRY
};

// clang-format: on

struct FilePos {
    std::string file;
    int depth{0};
    int line{0};
};

class Token
{
public:
    enum class Type { STRING, NUMBER, COMMENT, END_OF_FILE};
    enum class Group { UNKNOWN, NUMBER, STRING, OPERATOR, PREPROCESSOR, DIRECTIVE, REGISTER, STATEMENT, IDENTIFIER, COMMENT };
    Token() = delete;
    Token(int line, int pos);
    explicit Token(int n);
    Token(const Token& other);
    char* formatValue(char* d) const;
    Group groupId() const;

    Type type;
    TokenId tid{TokenId::TOK_UNKNOWN};
    int line;
    int pos;
    std::string_view str_value{};
    std::string str_container;
    double num_value{};
    bool inMacro{};
};

struct Constant
{
    double value;
    bool isMutable;
};

struct ProtoRef
{
    int value;
    char size;
};

struct Prototype
{
    int line, pos;
    std::vector<ProtoRef> addrs;
};

struct Macro
{
    int calls{};
    std::vector<std::string_view> args;
    std::vector<Token> body;
};

struct StringMode
{
    int calls{};
    char values[256]{};
    std::array<std::unique_ptr<Macro>, 256> modes{};
};

struct FlowControl
{
    int addr, line, pos;
    const char* type;
};

struct Monitor
{
    int type, base, len;
    std::string format;
};

class Lexer
{
public:
    Lexer() = delete;
    explicit Lexer(std::string_view text, bool emitComments = false);
    char nextChar();
    char peekChar() const;
    void skipWhitespace();
    void scanNextToken(Token& t);

protected:
    const char* _source;
    const char* _sourceRoot;
    const char* _sourceEnd;
    int _sourceLine;
    int _sourcePos;
    // error reporting
    char _isError{};
    std::string _error{};
    int _errorLine{};
    int _errorPos{};
    bool _emitComment{};
};

class Program : protected Lexer
{
public:
    static constexpr int RAM_MAX = 16 * 1024 * 1024;
    static constexpr int RAM_MASK = 16 * 1024 * 1024 - 1;

    Program() = delete;
    explicit Program(std::string_view text, int startAddress = 0x200);
    ~Program();
    bool compile();
    bool isError() const { return _isError; }
    int errorLine() const { return _isError ? _errorLine + 1 : 0; }
    int errorPos() const { return _isError ? _errorPos + 1 : 0; }
    [[nodiscard]] std::string errorMessage() const { return _error; }
    int lastAddressUsed() const { return _length - 1; }
    size_t codeSize() const { return _length - _startAddress; }
    int romStartAddress() const { return _startAddress; }
    const uint8_t* data() const { return _rom.data() + _startAddress; }
    int numSourceLines() const { return _sourceLine; }
    const char* breakpointInfo(uint32_t addr) const
    {
        if (_isError || addr > _rom.size())
            return nullptr;
        auto iter = _breakpoints.find(addr);
        return iter == _breakpoints.end() ? nullptr : iter->second;
    }
    uint32_t lineForAddress(uint32_t addr) const { return !_isError && addr < _romLineMap.size() ? _romLineMap[addr] : 0xFFFFFFFF; }
    bool isRegisterAlias(std::string_view name) const;

private:
    static double sign(double x) { return (0.0 < x) - (x < 0.0); }
    static double max(double x, double y) { return x < y ? y : x; }
    static double min(double x, double y) { return x < y ? x : y; }
    std::string_view safeStringStringView(std::string&& name);
    std::string_view safeStringStringView(char* name);
    int isEnd() const;
    void fetchToken();
    Token next();
    Token peek();
    bool peekMatch(const std::string_view& name, int index);
    bool match(const std::string_view& name);
    void eat();
    bool checkName(std::string_view name, const char* kind);
    std::string_view string();
    std::string_view identifier(const char* kind);
    void expect(std::string_view name);
    bool isRegister(std::string_view name);
    bool peekIsRegister();
    int registerOrAlias();
    int valueRange(int n, int mask);
    void valueFail(const std::string_view& w, const std::string_view& n, bool undef);
    int value4Bit();
    int value8Bit();
    int value12Bit();
    int value16Bit(int can_forward_ref, int offset);
    int value24Bit(int can_forward_ref, int offset);
    void addProtoRef(std::string_view name, int line, int pos, int where, int8_t size);
    Constant valueConstant();
    void macroBody(const std::string_view& desc, const std::string_view& name, Macro& m);
    double calcExpr(std::string_view name);
    double calcTerminal(std::string_view name);
    double calculated(std::string_view name);
    void append(uint8_t byte);
    void instruction(uint8_t a, uint8_t b);
    void immediate(uint8_t op, int nnn);
    void jump(int addr, int dest);
    void pseudoConditional(int reg, int sub, int comp);
    void conditional(int negated);
    void resolveLabel(int offset);
    void compileStatement();

    static bool isReserved(std::string_view name);

    // string interning table
    std::unordered_set<std::string> stringTable;

    // tokenizer
    std::deque<Token> tokens;

    // compiler
    char _hasMain{}; // do we need a trampoline for 'main'?
    bool _isNested{};
    int _startAddress{};
    int _here{};
    int _tokenStart{};
    int _length{};
    std::vector<uint8_t> _rom{};
    std::vector<char> _used{};
    std::vector<uint32_t> _romLineMap{};
    std::unordered_map<std::string_view, Constant> _constants;
    std::unordered_map<std::string_view, int> _aliases{};
    std::unordered_map<std::string_view, Prototype> _protos{};
    std::unordered_map<std::string_view, Macro> _macros{};
    std::unordered_map<std::string_view, StringMode> _stringModes{};
    std::stack<FlowControl> _loops{};
    std::stack<FlowControl> _branches{};
    std::stack<FlowControl> _whiles{}; // value=-1 indicates a marker

    // debugging
    std::unordered_map<uint32_t, const char*> _breakpoints{};
    std::unordered_map<std::string_view, Monitor> _monitors{};

};


}
