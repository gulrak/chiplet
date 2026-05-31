//---------------------------------------------------------------------------------------
//
//  octo/assembler.hpp
//
//  An assembler for Octo CHIP-8 assembly language, suitable for embedding in other
//  tools and environments. Compared to its original form it depends heavily on C++20
//  standard library. It's for sure not as lightweight as the original assembler, but it is
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

#include <cmath>
#include <array>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <string_view>
#include <vector>
#include <fmt/format.h>

#include "lexer.hpp"


namespace octo {

struct Constant
{
    double value;
    bool isMutable;
};

struct ProtoRef
{
    int value;
    int8_t size;
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

struct MonitorField {
    enum Type { STRING, UINT8, UINT16, UINT24, UINT32 } type;
    enum Base { CHAR, BINARY, DECIMAL, HEXADECIMAL } base;
    uint8_t offset;
    std::string_view text;
};

struct Monitor
{
    int type, base, len;
    std::vector<MonitorField> format;
};

class Assembler : protected Lexer
{
public:
    static constexpr int RAM_MAX = 16 * 1024 * 1024;
    static constexpr int RAM_MASK = 16 * 1024 * 1024 - 1;

    Assembler() = delete;
    explicit Assembler(std::string_view text, int startAddress = 0x200);
    ~Assembler();
    bool compile();
    bool isError() const { return _isError; }
    int errorLine() const { return _isError ? _errorLine + 1 : 0; }
    int errorPos() const { return _isError ? _errorPos + 1 : 0; }
    [[nodiscard]] std::string errorMessage() const { return _error; }
    int lastAddressUsed() const { return length - 1; }
    size_t codeSize() const { return length - startAddress; }
    int romStartAddress() const { return startAddress; }
    const uint8_t* data() const { return rom.data() + startAddress; }
    int numSourceLines() const { return _sourceLine; }
    std::string_view breakpointInfo(uint32_t addr) const
    {
        if (_isError || addr > rom.size())
            return "";
        auto iter = breakpoints.find(addr);
        return iter == breakpoints.end() ? "" : iter->second;
    }
    uint32_t lineForAddress(uint32_t addr) const { return !_isError && addr < romLineMap.size() ? romLineMap[addr] : 0xFFFFFFFF; }

private:
    static double sign(double x) { return (0.0 < x) - (x < 0.0); }
    static double max(double x, double y) { return x < y ? y : x; }
    static double min(double x, double y) { return x < y ? x : y; }
    std::string_view safeStringStringView(std::string&& name);
    std::string_view safeStringStringView(std::string_view name);
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
    bool isRegister(std::string_view name);
    bool peekIsRegister();
    int registerOrAlias();
    int valueRange(int n, int mask);
    void valueFail(const std::string_view& w, const std::string_view& n, bool undef);
    int value4bit();
    int value8bit();
    int value12bit();
    int value16bit(int can_forward_ref, int offset);
    int value24bit(int can_forward_ref, int offset);
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
    Token stringToken{0,0};

    // debugging
    std::unordered_map<uint32_t, std::string_view> breakpoints{};
    std::unordered_map<std::string_view, Monitor> monitors{};

};


}
