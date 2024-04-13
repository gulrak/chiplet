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
    decl(KEY, "key") \
    decl(NOT_KEY, "-key") \
    decl(HEX, "hex") \
    decl(BIGHEX, "bighex") \
    decl(RANDOM, "random") \
    decl(DELAY, "delay") \
    decl(COLON, ":") \
    decl(NEXT, ":next") \
    decl(UNPACK, ":unpack") \
    decl(BREAKPOINT, ":breakpoint") \
    decl(PROTO, ":proto") \
    decl(ALIAS, ":alias") \
    decl(CONST, ":const") \
    decl(ORG, ":org") \
    decl(SEMICOLON, ";") \
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
    decl(PITCH, "pitch")

enum class TokenId {
    TOK_UNKNOWN,
    STRING_LITERAL,
#define ENUM_ENTRY(NAME, TEXT) NAME,
    TOKEN_LIST(ENUM_ENTRY)
#undef ENUM_ENTRY
};

class Token
{
public:
    enum class Type { STRING, NUMBER, END_OF_FILE};
    Token() = delete;
    Token(int line, int pos);
    explicit Token(int n);
    Token(const Token& other);
    char* formatValue(char* d) const;

    Type type;
    TokenId tid{TokenId::TOK_UNKNOWN};
    int line;
    int pos;
    std::string_view str_value{};
    std::string str_container;
    double num_value{};
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
    explicit Lexer(std::string_view text);
    char next_char();
    char peek_char() const;
    void skip_whitespace();
    void scanNextToken(Token& t);

protected:
    const char* source;
    const char* source_root;
    const char* sourceEnd;
    int source_line;
    int source_pos;
    // error reporting
    char is_error{};
    std::string error{};
    int error_line{};
    int error_pos{};
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
    bool isError() const { return is_error; }
    int errorLine() const { return is_error ? error_line + 1 : 0; }
    int errorPos() const { return is_error ? error_pos + 1 : 0; }
    [[nodiscard]] std::string errorMessage() const { return error; }
    int lastAddressUsed() const { return length - 1; }
    size_t codeSize() const { return length - startAddress; }
    int romStartAddress() const { return startAddress; }
    const uint8_t* data() const { return rom.data() + startAddress; }
    int numSourceLines() const { return source_line; }
    const char* breakpointInfo(uint32_t addr) const
    {
        if (is_error || addr > rom.size())
            return nullptr;
        auto iter = breakpoints.find(addr);
        return iter == breakpoints.end() ? nullptr : iter->second;
    }
    uint32_t lineForAddress(uint32_t addr) const { return !is_error && addr < romLineMap.size() ? romLineMap[addr] : 0xFFFFFFFF; }

private:
    static double sign(double x) { return (0.0 < x) - (x < 0.0); }
    static double max(double x, double y) { return x < y ? y : x; }
    static double min(double x, double y) { return x < y ? x : y; }
    std::string_view safeStringStringView(std::string&& name);
    std::string_view safeStringStringView(char* name);
    int is_end() const;
    void fetchToken();
    Token next();
    Token peek();
    bool peek_match(const std::string_view& name, int index);
    bool match(const std::string_view& name);
    void eat();
    bool check_name(std::string_view name, const char* kind);
    std::string_view string();
    std::string_view identifier(const char* kind);
    void expect(std::string_view name);
    bool is_register(std::string_view name);
    bool peek_is_register();
    int register_or_alias();
    int value_range(int n, int mask);
    void value_fail(const std::string_view& w, const std::string_view& n, bool undef);
    int value_4bit();
    int value_8bit();
    int value_12bit();
    int value_16bit(int can_forward_ref, int offset);
    int value_24bit(int can_forward_ref, int offset);
    void addProtoRef(std::string_view name, int line, int pos, int where, int8_t size);
    Constant value_constant();
    void macro_body(const std::string_view& desc, const std::string_view& name, Macro& m);
    double calc_expr(std::string_view name);
    double calc_terminal(std::string_view name);
    double calculated(std::string_view name);
    void append(uint8_t byte);
    void instruction(uint8_t a, uint8_t b);
    void immediate(uint8_t op, int nnn);
    void jump(int addr, int dest);
    void pseudo_conditional(int reg, int sub, int comp);
    void conditional(int negated);
    void resolve_label(int offset);
    void compile_statement();

    static bool is_reserved(std::string_view name);

    // string interning table
    std::unordered_set<std::string> stringTable;

    // tokenizer
    std::deque<Token> tokens;

    // compiler
    char has_main{};  // do we need a trampoline for 'main'?
    int startAddress{};
    int here{};
    int length{};
    std::vector<uint8_t> rom{};
    std::vector<char> used{};
    std::vector<uint32_t> romLineMap{};
    std::unordered_map<std::string_view, Constant> constants;
    std::unordered_map<std::string_view, int> aliases{};
    std::unordered_map<std::string_view, Prototype> protos{};
    std::unordered_map<std::string_view, Macro> macros{};
    std::unordered_map<std::string_view, StringMode> stringModes{};
    std::stack<FlowControl> loops{};
    std::stack<FlowControl> branches{};
    std::stack<FlowControl> whiles{}; // value=-1 indicates a marker

    // debugging
    std::unordered_map<uint32_t, const char*> breakpoints{};
    std::unordered_map<std::string_view, Monitor> monitors{};

};


}
