//---------------------------------------------------------------------------------------
//
//  octo/assembler.cpp
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
//  Copyright (c) 2024, Steffen Schümann
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

#include "assembler.hpp"

#include <ghc/filesystem.hpp>

#include <iostream>
#include <ostream>

namespace fs = ghc::filesystem;

namespace octo
{

namespace detail {

inline MonitorField::Type typeFromLen(std::size_t len) {
    switch (len) {
        case 1: return MonitorField::UINT8;
        case 2: return MonitorField::UINT16;
        case 3: return MonitorField::UINT24;
        case 4: return MonitorField::UINT32;
        default: return MonitorField::BYTES;
    }
}

inline MonitorField::Base baseFromSpec(char spec) {
    switch (spec) {
        case 'b': return MonitorField::BINARY;
        case 'i': return MonitorField::DECIMAL;
        case 'x': return MonitorField::HEXADECIMAL;
        case 'c': return MonitorField::CHAR;
        default:  throw std::invalid_argument("unknown format directive specifier (expected b/i/x/c)");
    }
}

inline void pushLiteral(std::vector<MonitorField>& out,
                         std::string_view fmt,
                         std::size_t begin,
                         std::size_t end,
                         std::size_t offset) {
    if (end > begin) {
        out.push_back(MonitorField{
          .type   = MonitorField::STRING,
          .base   = MonitorField::DECIMAL,   // unused for STRING
          .offset = offset,
          .text   = fmt.substr(begin, end - begin),
        });
    }
}

static uint8_t toUint8JS(double x) {
    if (!std::isfinite(x) || x == 0.0)
        return 0;
    double narrowed = std::fmod(std::trunc(x), 256.0);
    if (narrowed < 0.0)
        narrowed += 256.0;
    return static_cast<uint8_t>(narrowed);
}

static int32_t toInt32JS(double x) noexcept
{
    if (!std::isfinite(x) || x == 0.0)
        return 0;

    constexpr double MODULUS = 4294967296.0;  // 2^32
    constexpr double SIGN_BIT = 2147483648.0; // 2^31

    double narrowed = std::fmod(std::trunc(x), MODULUS);
    if (narrowed < 0.0)
        narrowed += MODULUS;
    if (narrowed >= SIGN_BIT)
        narrowed -= MODULUS;

    return static_cast<int32_t>(narrowed);
}

[[nodiscard]] bool isTruthy(double val) noexcept
{
    // NaN is never equal to itself in IEEE 754; +0.0 == -0.0 in C++
    return !std::isnan(val) && (val != 0.0);
}

std::u16string toUtf16(std::string_view text)
{
    std::u16string units;
    std::size_t chunkBegin = 0;
    std::size_t validEnd = 0;
    unsigned state = fs::detail::S_STRT;
    uint32_t codePoint = 0;
    for (std::size_t index = 0; index < text.size();) {
        const auto previousState = state;
        state = fs::detail::consumeUtf8Fragment(state, static_cast<uint8_t>(text[index]), codePoint);
        if (state == fs::detail::S_RJCT) {
            units += fs::detail::fromUtf8<std::u16string>(text.substr(chunkBegin, validEnd - chunkBegin));
            units.push_back(0xFFFD);
            state = fs::detail::S_STRT;
            // Node retries the rejecting byte after an incomplete sequence;
            // ghc's default recovery would consume it along with the prefix.
            if (previousState == fs::detail::S_STRT)
                ++index;
            chunkBegin = validEnd = index;
        }
        else {
            ++index;
            if (state == fs::detail::S_STRT)
                validEnd = index;
        }
    }
    units += fs::detail::fromUtf8<std::u16string>(text.substr(chunkBegin, validEnd - chunkBegin));
    if (state != fs::detail::S_STRT)
        units.push_back(0xFFFD);
    return units;
}

std::string utf8Unit(char16_t unit)
{
    std::string text;
    fs::detail::appendUTF8(text, fs::detail::is_surrogate(unit) ? 0xFFFD : unit);
    return text;
}

} // namespace detail

Assembler::~Assembler()
{
}

std::string_view Assembler::safeStringStringView(std::string&& name)
{
    auto iter = stringTable.find(name);
    if (iter != stringTable.end())
        return *iter;
    return *stringTable.emplace(std::move(name)).first;
}

std::string_view Assembler::safeStringStringView(std::string_view name)
{
    return safeStringStringView(std::string(name));
}

std::string_view Assembler::safeStringStringView(char* name)
{
    return safeStringStringView(std::string{name, std::strlen(name)});
}

int Assembler::is_end() const
{
    return tokens.empty() && _source >= _sourceEnd;
}

void Assembler::fetchToken()
{
    if (is_end()) {
        _isError = 1;
        _error = "Unexpected EOF.";
        return;
    }
    if (_isError)
        return;
    tokens.emplace_back(_sourceLine, _sourcePos);
    auto& t = tokens.back();
    scanNextToken(t);
}

Token Assembler::next()
{
    if (tokens.empty())
        fetchToken();
    if (_isError)
        return {_sourceLine, _sourcePos};
    auto t = tokens.front();
    tokens.pop_front();
    _errorLine = t.line, _errorPos = t.pos;
    return t;
}

Token Assembler::peek()
{
    if (tokens.empty())
        fetchToken();
    if (_isError)
        return {_sourceLine, _sourcePos};
    return tokens.front();
}

bool Assembler::peek_match(const std::string_view& name, int index)
{
    while (!_isError && !is_end() && tokens.size() < index + 1)
        fetchToken();
    if (is_end() || _isError)
        return false;
    return tokens[index].isText() && tokens[index].strValue == name;
}

inline bool Assembler::match(const std::string_view& name)
{
    if (peek_match(name, 0)) {
        tokens.pop_front();
        return true;
    }
    return false;
}

void Assembler::eat()
{
    tokens.pop_front();
}

/**
 *
 *  Parsing
 *
 **/


bool Assembler::isReserved(std::string_view name)
{
    return name != "exit" && isKnownToken(name);
}

bool Assembler::check_name(std::string_view name, const char* kind)
{
    if (_isError)
        return false;
    if (name.starts_with("OCTO_") || isReserved(name)) {
        _isError = 1, _error = fmt::format("The name '{}' is reserved and cannot be used for a {}.", name, kind);
        return false;
    }
    return true;
}

std::string_view Assembler::string()
{
    if (_isError)
        return "";
    stringToken = next();
    if (!stringToken.isText()) {
        _isError = 1, _error = fmt::format("Expected a string, got {}.", (int)stringToken.numValue);
        return "";
    }
    return safeStringStringView(stringToken.strValue);
}

std::string_view Assembler::identifier(const char* kind)
{
    if (_isError)
        return "";
    stringToken = next();
    if (!stringToken.isText()) {
        _isError = 1, _error = fmt::format("Expected a name for a {}, got {}.", kind, (int)stringToken.numValue);
        return "";
    }
    if (!check_name(stringToken.strValue, kind))
        return "";
    return safeStringStringView(stringToken.strValue);
}

void Assembler::expect(std::string_view name)
{
    if (_isError)
        return;
    auto t = next();
    if (!t.isText() || t.strValue != name) {
        _isError = 1, _error = fmt::format("Expected {}, got {}.", name, t.formatValue());
    }
}

bool Assembler::isRegister(std::string_view name)
{
    if (aliases.count(name))
        return true;
    if (name.length() != 2)
        return false;
    if (name[0] != 'v' && name[0] != 'V')
        return false;
    return isxdigit(name[1]);
}

bool Assembler::peekIsRegister()
{
    auto t = peek();
    return t.isText() && isRegister(t.strValue);
}

int Assembler::registerOrAlias()
{
    if (_isError)
        return 0;
    auto t = next();
    if (!t.isText() || !isRegister(t.strValue)) {
        _isError = 1, _error = fmt::format("Expected register, got {}.", t.formatValue());
        return 0;
    }
    auto iter = aliases.find(t.strValue);
    if (iter != aliases.end())
        return iter->second;
    char c = static_cast<char>(std::tolower(t.strValue[1]));
    return isdigit(c) ? c - '0' : 10 + (c - 'a');
}

int Assembler::valueRange(double n, int mask)
{
    if (mask == 0xF && (n < 0 || n > mask))
        _isError = 1, _error = fmt::format("Argument {} does not fit in 4 bits- must be in range [0,15].", n);
    if (mask == 0xFF && (n < -128 || n > mask))
        _isError = 1, _error = fmt::format("Argument {} does not fit in a byte- must be in range [-128,255].", n);
    if (mask == 0xFFF && (n < 0 || n > mask))
        _isError = 1, _error = fmt::format("Argument {} does not fit in 12 bits.", n);
    if (mask == 0xFFFF && (n < 0 || n > mask))
        _isError = 1, _error = fmt::format("Argument {} does not fit in 16 bits.", n);
    if (mask == 0xFFFFFF && (n < 0 || n > mask))
        _isError = 1, _error = fmt::format("Argument {} does not fit in 24 bits.", n);
    return detail::toInt32JS(n) & mask;
}

int Assembler::checkedAddress(double value, int bits)
{
    if (_isError)
        return 0;
    const int maximum = (1 << bits) - 1;
    if (!std::isfinite(value) || value < 0 || value > maximum) {
        _isError = 1;
        _error = fmt::format("Argument {} does not fit in {} bits.", value, bits);
        return 0;
    }
    return static_cast<int>(value);
}

void Assembler::valueFail(const std::string_view& w, const std::string_view& n, bool undef)
{
    if (_isError)
        return;
    if (isRegister(n))
        _isError = 1, _error = fmt::format("Expected {} value, but found the register {}.", w, n);
    else if (isReserved(n))
        _isError = 1, _error = fmt::format("Expected {} value, but found the keyword '{}'. Missing a token?", w, n);
    else if (undef)
        _isError = 1, _error = fmt::format("Expected {} value, but found the undefined name '{}'.", w, n);
}

int Assembler::value4bit()
{
    if (_isError)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return valueRange(t.numValue, 0xF);
    }
    auto& n = t.strValue;
    valueFail("a 4-bit", n, false);
    if (_isError)
        return 0;
    auto iter = constants.find(n);
    if (iter != constants.end())
        return valueRange(iter->second.value, 0xF);
    if (const auto label = _programLabels.find(n); label != _programLabels.end())
        return valueRange(label->second, 0xF);
    return valueFail("a 4-bit", n, true), 0;
}

int Assembler::value8bit()
{
    if (_isError)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return valueRange(t.numValue, 0xFF);
    }
    auto& n = t.strValue;
    valueFail("an 8-bit", n, false);
    if (_isError)
        return 0;
    auto iter = constants.find(n);
    if (iter != constants.end())
        return valueRange(iter->second.value, 0xFF);
    if (const auto label = _programLabels.find(n); label != _programLabels.end())
        return valueRange(label->second, 0xFF);
    return valueFail("an 8-bit", t.strValue, true), 0;
}

int Assembler::value12bit()
{
    if (_isError)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return valueRange(t.numValue, 0xFFF);
    }
    auto& n = t.strValue;
    valueFail("a 12-bit", n, false);
    if (_isError)
        return 0;
    int proto_line = t.line, proto_pos = t.pos;
    auto iter = constants.find(n);
    if (iter != constants.end())
        return valueRange(iter->second.value, 0xFFF);
    if (const auto label = _programLabels.find(n); label != _programLabels.end())
        return valueRange(label->second, 0xFFF);
    if (!check_name(n, "label"))
        return 0;
    addProtoRef(n, proto_line, proto_pos, here, 12);
    return 0;
}

int Assembler::value16bit(int can_forward_ref, int offset)
{
    if (_isError)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return valueRange(t.numValue, 0xFFFF);
    }
    auto& n = t.strValue;
    valueFail("a 16-bit", n, false);
    if (_isError)
        return 0;
    int proto_line = t.line, proto_pos = t.pos;
    auto iter = constants.find(n);
    if (iter != constants.end())
        return valueRange(iter->second.value, 0xFFFF);
    if (const auto label = _programLabels.find(n); label != _programLabels.end())
        return valueRange(label->second, 0xFFFF);
    if (!check_name(n, "label"))
        return 0;
    if (!can_forward_ref) {
        _isError = 1, _error = fmt::format("The reference to '{}' may not be forward-declared.", n);
        return 0;
    }
    addProtoRef(n, proto_line, proto_pos, here + offset, 16);
    return 0;
}

int Assembler::value24bit(int can_forward_ref, int offset)
{
    if (_isError)
        return 0;
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        return checkedAddress(t.numValue, 24);
    }
    auto& n = t.strValue;
    valueFail("a 24-bit", n, false);
    if (_isError)
        return 0;
    int proto_line = t.line, proto_pos = t.pos;
    auto iter = constants.find(n);
    if (iter != constants.end())
        return checkedAddress(iter->second.value, 24);
    if (const auto label = _programLabels.find(n); label != _programLabels.end())
        return checkedAddress(label->second, 24);
    if (!check_name(n, "label"))
        return 0;
    if (!can_forward_ref) {
        _isError = 1, _error = fmt::format("The reference to '{}' may not be forward-declared.", n);
        return 0;
    }
    addProtoRef(n, proto_line, proto_pos, here + offset, 24);
    return 0;
}

void Assembler::addProtoRef(std::string_view name, int line, int pos, int where, int8_t size)
{
    name = safeStringStringView(name);
    auto iter = protos.find(name);
    if (iter == protos.end()) {
        iter = protos.emplace(name, Prototype{line, pos}).first;
    }
    iter->second.addrs.push_back({where, size});
}

Constant Assembler::valueConstant()
{
    const auto token = next();
    if (_isError)
        return {0, false};
    if (token.type == Token::Type::NUMBER)
        return {token.numValue, false};
    const auto name = token.strValue;
    if (protos.contains(name)) {
        _isError = 1;
        _error = fmt::format("A constant reference to '{}' may not be forward-declared.", name);
    }
    else if (isReserved(name)) {
        _isError = 1;
        _error = fmt::format("Expected a constant name, but found the keyword '{}'. Missing a token?", name);
    }
    else if (const auto label = _programLabels.find(name); label != _programLabels.end())
        return {static_cast<double>(label->second), false};
    else if (const auto constant = constants.find(name); constant != constants.end())
        return {constant->second.value, false};
    else {
        _isError = 1;
        _error = fmt::format("Undefined constant name '{}'.", name);
    }
    return {0, false};
}

void Assembler::macroBody(const std::string_view& desc, const std::string_view& name, Macro& m)
{
    if (_isError)
        return;
    expect("{");
    if (_isError) {
        _error = fmt::format("Expected '{{' for definition of {} '{}'.", desc, name);
        return;
    }
    int depth = 1;
    while (!is_end()) {
        auto t = peek();
        if (t.type == Token::Type::END_OF_FILE)
            break;
        if (t.isText() && t.strValue == "{")
            depth++;
        if (t.isText() && t.strValue == "}")
            depth--;
        if (depth == 0)
            break;
        m.body.push_back(next());
    }
    expect("}");
    if (_isError)
        _error = fmt::format("Expected '}}' for definition of {} '{}'.", desc, name);
}

//-----------------------------------------------------------
// Compile-time Calculation
//-----------------------------------------------------------

double Assembler::calcTerminal(std::string_view name)
{
    // NUMBER | CONSTANT | LABEL | VREGISTER | '(' expression ')'
    if (match("PI"))
        return 3.141592653589793;
    if (match("E"))
        return 2.718281828459045;
    if (match("HERE"))
        return here;
    if (peekIsRegister())
        return registerOrAlias();
    auto t = next();
    if (t.type == Token::Type::NUMBER) {
        double r = t.numValue;
        return r;
    }
    auto& n = t.strValue;
    if (auto number = parseCalculatedNumber(n); number && !std::isnan(*number))
        return *number;
    if (const auto iter = constants.find(n); iter != constants.end())
        return iter->second.value;
    if (const auto label = _programLabels.find(n); label != _programLabels.end())
        return label->second;
    if (protos.count(n)) {
        _isError = 1, _error = fmt::format("Cannot use forward declaration '{}' when calculating constant '{}'.", n, name);
        return 0;
    }
    if (n != "(") {
        _isError = 1, _error = fmt::format("Found undefined name '{}' when calculating constant '{}'.", n, name);
        return 0;
    }
    double r = calcExpr(name);
    expect(")");
    return r;
}

double Assembler::calcExpr(std::string_view name)
{
    // UNARY expression
    if (match("strlen"))
        return static_cast<double>(detail::toUtf16(string()).size());
    if (match("-"))
        return -calcExpr(name);
    if (match("~"))
        return ~detail::toInt32JS(calcExpr(name));
    if (match("!"))
        return !detail::isTruthy(calcExpr(name));
    if (match("sin"))
        return sin(calcExpr(name));
    if (match("cos"))
        return cos(calcExpr(name));
    if (match("tan"))
        return tan(calcExpr(name));
    if (match("exp"))
        return exp(calcExpr(name));
    if (match("log"))
        return log(calcExpr(name));
    if (match("abs"))
        return fabs(calcExpr(name));
    if (match("sqrt"))
        return sqrt(calcExpr(name));
    if (match("sign"))
        return sign(calcExpr(name));
    if (match("ceil"))
        return ceil(calcExpr(name));
    if (match("floor"))
        return floor(calcExpr(name));
    if (match("@")) {
        auto addr = detail::toInt32JS(calcExpr(name));
        return addr >= 0 && addr < rom.size() ? 0xFF & rom[addr] : 0;
    }

    // expression BINARY expression
    double r = calcTerminal(name);
    if (match("-"))
        return r - calcExpr(name);
    if (match("+"))
        return r + calcExpr(name);
    if (match("*"))
        return r * calcExpr(name);
    if (match("/"))
        return r / calcExpr(name);
    if (match("%"))
        return std::fmod(r, calcExpr(name));
    if (match("&"))
        return detail::toInt32JS(r) & detail::toInt32JS(calcExpr(name));
    if (match("|"))
        return detail::toInt32JS(r) | detail::toInt32JS(calcExpr(name));
    if (match("^"))
        return detail::toInt32JS(r) ^ detail::toInt32JS(calcExpr(name));
    if (match("<<")) {
        const auto count = static_cast<uint32_t>(detail::toInt32JS(calcExpr(name))) & 31;
        const auto bits = static_cast<uint32_t>(detail::toInt32JS(r)) << count;
        return detail::toInt32JS(static_cast<double>(bits));
    }
    if (match(">>")) {
        const auto count = static_cast<uint32_t>(detail::toInt32JS(calcExpr(name))) & 31;
        return detail::toInt32JS(r) >> count;
    }
    if (match("pow"))
        return pow(r, calcExpr(name));
    if (match("min"))
        return min(r, calcExpr(name));
    if (match("max"))
        return max(r, calcExpr(name));
    if (match("<"))
        return r < calcExpr(name);
    if (match(">"))
        return r > calcExpr(name);
    if (match("<="))
        return r <= calcExpr(name);
    if (match(">="))
        return r >= calcExpr(name);
    if (match("=="))
        return r == calcExpr(name);
    if (match("!="))
        return r != calcExpr(name);
    // terminal
    return r;
}

double Assembler::calculated(std::string_view name)
{
    expect("{");
    double r = calcExpr(name);
    expect("}");
    return r;
}

//-----------------------------------------------------------
//  ROM construction
//-----------------------------------------------------------

void Assembler::append(uint8_t byte)
{
    if (_isError)
        return;
    if (here < 0 || here >= RAM_MAX) {
        _isError = 1;
        _error = "Supported ROM space is full (16MB).";
        return;
    }
    while (here >= rom.size()) {
        if (rom.size() < 1024 * 1024) {
            rom.resize(1024 * 1024, 0);
            used.resize(1024 * 1024, 0);
            romLineMap.resize(1024 * 1024, 0xFFFFFFFF);
        }
        else if (rom.size() < RAM_MAX / 2) {
            rom.resize(RAM_MAX / 2, 0);
            used.resize(RAM_MAX / 2, 0);
            romLineMap.resize(RAM_MAX / 2, 0xFFFFFFFF);
        }
        else if (rom.size() < RAM_MAX) {
            rom.resize(RAM_MAX, 0);
            used.resize(RAM_MAX, 0);
            romLineMap.resize(RAM_MAX, 0xFFFFFFFF);
        }
    }
    if (here > startAddress && used[here]) {
        _isError = 1;
        _error = fmt::format("Data overlap. Address 0x{:0X} has already been defined.", here);
        return;
    }
    romLineMap[here] = _sourceLine;
    rom[here] = byte, used[here] = 1, here++;
    if (here > length)
        length = here;
}

void Assembler::instruction(uint8_t a, uint8_t b)
{
    append(a), append(b);
}

void Assembler::immediate(uint8_t op, int nnn)
{
    nnn = valueRange(nnn, 0xFFF);
    if (_isError)
        return;
    instruction(op | ((nnn >> 8) & 0xF), (nnn & 0xFF));
}

void Assembler::jump(int addr, int dest)
{
    if (_isError)
        return;
    dest = valueRange(dest, 0xFFF);
    if (_isError)
        return;
    rom[addr] = (0x10 | ((dest >> 8) & 0xF)), used[addr] = 1;
    rom[addr + 1] = (dest & 0xFF), used[addr + 1] = 1;
}

//-----------------------------------------------------------
//  The Compiler proper
//-----------------------------------------------------------

void Assembler::pseudoConditional(int reg, int sub, int comp)
{
    if (peekIsRegister())
        instruction(0x8F, registerOrAlias() << 4);
    else
        instruction(0x6F, value8bit());
    instruction(0x8F, (reg << 4) | sub);
    instruction(comp, 0);
}

void Assembler::conditional(int negated)
{
    int reg = registerOrAlias();
    auto t = peek();
    auto formattedToken = t.formatValue();
    if (_isError)
        return;
    auto n = string();

#define octo_ca(pos, neg) (n == (negated ? (neg) : (pos)))

    if (octo_ca("==", "!=")) {
        if (peekIsRegister())
            instruction(0x90 | reg, registerOrAlias() << 4);
        else
            instruction(0x40 | reg, value8bit());
    }
    else if (octo_ca("!=", "==")) {
        if (peekIsRegister())
            instruction(0x50 | reg, registerOrAlias() << 4);
        else
            instruction(0x30 | reg, value8bit());
    }
    else if (octo_ca("key", "-key"))
        instruction(0xE0 | reg, 0xA1);
    else if (octo_ca("-key", "key"))
        instruction(0xE0 | reg, 0x9E);
    else if (octo_ca(">", "<="))
        pseudoConditional(reg, 0x5, 0x4F);
    else if (octo_ca("<", ">="))
        pseudoConditional(reg, 0x7, 0x4F);
    else if (octo_ca(">=", "<"))
        pseudoConditional(reg, 0x7, 0x3F);
    else if (octo_ca("<=", ">"))
        pseudoConditional(reg, 0x5, 0x3F);
    else {
        _isError = 1, _error = fmt::format("Expected conditional operator, got {}.", formattedToken);
    }
}

void Assembler::resolveLabel(int offset)
{
    int target = (here) + offset;
    auto n = identifier("label");
    if (_isError)
        return;
    if (_programLabels.count(n)) {
        _isError = 1, _error = fmt::format("The name '{}' has already been defined.", n);
        return;
    }
    if (aliases.count(n)) {
        _isError = 1, _error = fmt::format("The name '{}' is already used by an alias.", n);
        return;
    }
    if ((target == startAddress + 2 || target == startAddress) && (n == "main")) {
        has_main = 0, here = target = startAddress;
        rom[startAddress] = 0, used[startAddress] = 0;
        rom[startAddress + 1] = 0, used[startAddress + 1] = 0;
    }
    _programLabels.insert_or_assign(n, target);
    auto iter = protos.find(n);
    if (iter == protos.end())
        return;

    auto& pr = iter->second;
    for (auto& pa : pr.addrs) {
        if (pa.size <= 12 && (target & 0xFFF) != target) {
            _isError = 1, _error = fmt::format("Value 0x{:0X} for label '{}' does not fit in 12 bits.", target, n);
            break;
        }
        if (pa.size <= 16 && (target & 0xFFFF) != target) {
            _isError = 1, _error = fmt::format("Value 0x{:0X} for label '{}' does not fit in 16 bits.", target, n);
            break;
        }
        if (pa.size <= 24 && (target & 0xFFFFFF) != target) {
            _isError = 1, _error = fmt::format("Value 0x{:0X} for label '{}' does not fit in 24 bits.", target, n);
            break;
        }
        if (pa.size == 16 && (rom[pa.value] & 0xF0) == 0x60) {  // :unpack long target
            rom[pa.value + 1] = target >> 8;
            rom[pa.value + 3] = target;
        }
        else if (pa.size == 16) {  // i := long target
            rom[pa.value] = target >> 8;
            rom[pa.value + 1] = target;
        }
        else if (pa.size == 24) {
            rom[pa.value] = target >> 16;
            rom[pa.value + 1] = target >> 8;
            rom[pa.value + 2] = target;
        }
        else if ((rom[pa.value] & 0xF0) == 0x60) {  // :unpack target
            rom[pa.value + 1] = ((rom[pa.value + 1]) & 0xF0) | ((target >> 8) & 0xF);
            rom[pa.value + 3] = target;
        }
        else {
            rom[pa.value] = ((rom[pa.value]) & 0xF0) | ((target >> 8) & 0xF);
            rom[pa.value + 1] = target;
        }
    }
    protos.erase(n);
}

#if 0
void Assembler::compile_statement()
{
    if (_isError)
        return;
    int peek_line = peek().line, peek_pos = peek().pos;
    if (peek_is_register()) {
        int r = register_or_alias();
        if (match(":=")) {
            if (peek_is_register())
                instruction(0x80 | r, (register_or_alias() << 4) | 0x0);
            else if (match("random"))
                instruction(0xC0 | r, value_8bit());
            else if (match("key"))
                instruction(0xF0 | r, 0x0A);
            else if (match("delay"))
                instruction(0xF0 | r, 0x07);
            else
                instruction(0x60 | r, value_8bit());
        }
        else if (match("+=")) {
            if (peek_is_register())
                instruction(0x80 | r, (register_or_alias() << 4) | 0x4);
            else
                instruction(0x70 | r, value_8bit());
        }
        else if (match("-=")) {
            if (peek_is_register())
                instruction(0x80 | r, (register_or_alias() << 4) | 0x5);
            else
                instruction(0x70 | r, 1 + ~value_8bit());
        }
        else if (match("|="))
            instruction(0x80 | r, (register_or_alias() << 4) | 0x1);
        else if (match("&="))
            instruction(0x80 | r, (register_or_alias() << 4) | 0x2);
        else if (match("^="))
            instruction(0x80 | r, (register_or_alias() << 4) | 0x3);
        else if (match("=-"))
            instruction(0x80 | r, (register_or_alias() << 4) | 0x7);
        else if (match(">>="))
            instruction(0x80 | r, (register_or_alias() << 4) | 0x6);
        else if (match("<<="))
            instruction(0x80 | r, (register_or_alias() << 4) | 0xE);
        else {
            auto t = next();
            if (!_isError)
                _isError = 1, _error = fmt::format("Unrecognized operator {}.", t.formatValue());
        }
    }
    else if (match(":"))
        resolve_label(0);
    else if (match(":next"))
        resolve_label(1);
    else if (match(":unpack")) {
        int a = 0;
        if (match("long")) {
            a = value_16bit(1, 0);
        }
        else {
            int v = value_4bit();
            a = (v << 12) | value_12bit();
        }
        auto rh = aliases["unpack-hi"];
        auto rl = aliases["unpack-lo"];
        instruction(0x60 | rh, a >> 8);
        instruction(0x60 | rl, a);
    }
    else if (match(":breakpoint"))
        breakpoints[here] = string().data();
    else if (match(":monitor")) {
        auto n = peek().formatValue();
        int type, base, len;
        std::string format;
        if (peek_is_register()) {
            type = 0;  // register monitor
            base = register_or_alias();
            if (peek().type == Token::Type::NUMBER)
                len = value_4bit();
            else
                len = -1, format = string();
        }
        else {
            type = 1;  // memory monitor
            base = value_16bit(0, 0);
            if (peek().type == Token::Type::NUMBER)
                len = value_16bit(0, 0);
            else
                len = -1, format = string();
        }
        if (!n.empty() && n.back() == '\'')
            n.pop_back();
        auto nn = safeStringStringView(!n.empty() && n.front() == '\'' ? std::string_view(n).substr(1) : std::string_view(n));
        monitors.insert_or_assign(nn, Monitor{type, base, len, format});
    }
    else if (match(":assert")) {
        auto message = peek_match("{", 0) ? std::string_view() : string();
        if (!(int)calculated("assertion")) {
            _isError = 1;
            if (!message.empty())
                _error = fmt::format("Assertion failed: {}", message);
            else
                _error = "Assertion failed.";
        }
    }
    else if (match(":proto"))
        next();  // deprecated
    else if (match(":alias")) {
        auto n = identifier("alias");
        if (constants.count(n)) {
            _isError = 1, _error = fmt::format("The name '{}' is already used by a constant.", n);
            return;
        }
        int v = peek_match("{", 0) ? (int)calculated("ANONYMOUS") : (int)register_or_alias();
        if (v < 0 || v > 15) {
            _isError = 1;
            _error = "Register index must be in the range [0,F].";
            return;
        }
        aliases[n] = v;
    }
    else if (match(":byte")) {
        append(peek_match("{", 0) ? (int)calculated("ANONYMOUS") : value_8bit());
    }
    else if (match(":pointer") || match(":pointer16")) {
        int a = peek_match("{", 0) ? (int)calculated("ANONYMOUS") : (int)value_16bit(1, 0);
        instruction(a >> 8, a);
    }
    else if (match(":pointer24")) {
        int a = peek_match("{", 0) ? (int)calculated("ANONYMOUS") : (int)value_24bit(1, 0);
        append(a >> 16);
        instruction(a >> 8, a);
    }
    else if (match(":org")) {
        here = (peek_match("{", 0) ? RAM_MASK & (int)calculated("ANONYMOUS") : value_16bit(0, 0));
    }
    else if (match(":call")) {
        immediate(0x20, peek_match("{", 0) ? 0xFFF & (int)calculated("ANONYMOUS") : value_12bit());
    }
    else if (match(":const")) {
        auto n = identifier("constant");
        if (constants.count(n)) {
            _isError = 1;
            _error = fmt::format("The name '{}' has already been defined.", n);
            return;
        }
        constants.insert_or_assign(n, value_constant());
    }
    else if (match(":calc")) {
        auto n = identifier("calculated constant");
        auto iter = constants.find(n);
        if (iter != constants.end() && !iter->second.isMutable) {
            _isError = 1, _error = fmt::format("Cannot redefine the name '{}' with :calc.", n);
            return;
        }
        constants.insert_or_assign(n, Constant{calculated(n), true});
    }
    else if (match(";") || match("return"))
        instruction(0x00, 0xEE);
    else if (match("clear"))
        instruction(0x00, 0xE0);
    else if (match("bcd"))
        instruction(0xF0 | register_or_alias(), 0x33);
    else if (match("delay"))
        expect(":="), instruction(0xF0 | register_or_alias(), 0x15);
    else if (match("buzzer"))
        expect(":="), instruction(0xF0 | register_or_alias(), 0x18);
    else if (match("pitch"))
        expect(":="), instruction(0xF0 | register_or_alias(), 0x3A);
    else if (match("jump0"))
        immediate(0xB0, value_12bit());
    else if (match("jump"))
        immediate(0x10, value_12bit());
    else if (match("native"))
        immediate(0x00, value_12bit());
    else if (match("audio"))
        instruction(0xF0, 0x02);
    else if (match("scroll-down"))
        instruction(0x00, 0xC0 | value_4bit());
    else if (match("scroll-up"))
        instruction(0x00, 0xD0 | value_4bit());
    else if (match("scroll-right"))
        instruction(0x00, 0xFB);
    else if (match("scroll-left"))
        instruction(0x00, 0xFC);
    else if (match("exit"))
        instruction(0x00, 0xFD);
    else if (match("lores"))
        instruction(0x00, 0xFE);
    else if (match("hires"))
        instruction(0x00, 0xFF);
    else if (match("sprite")) {
        int x = register_or_alias(), y = register_or_alias();
        instruction(0xD0 | x, (y << 4) | value_4bit());
    }
    else if (match("plane")) {
        int n = value_4bit();
        if (n > 15)
            _isError = 1, _error = fmt::format("The plane bitmask must be [0,15], was {}.", n);
        instruction(0xF0 | n, 0x01);
    }
    else if (match("saveflags"))
        instruction(0xF0 | register_or_alias(), 0x75);
    else if (match("loadflags"))
        instruction(0xF0 | register_or_alias(), 0x85);
    else if (match("save")) {
        int r = register_or_alias();
        if (match("-"))
            instruction(0x50 | r, (register_or_alias() << 4) | 0x02);
        else
            instruction(0xF0 | r, 0x55);
    }
    else if (match("load")) {
        int r = register_or_alias();
        if (match("-"))
            instruction(0x50 | r, (register_or_alias() << 4) | 0x03);
        else
            instruction(0xF0 | r, 0x65);
    }
    else if (match("i")) {
        if (match(":=")) {
            if (match("long")) {
                int a = value_16bit(1, 2);
                instruction(0xF0, 0x00);
                instruction((a >> 8), a);
            }
            else if (match("hex"))
                instruction(0xF0 | register_or_alias(), 0x29);
            else if (match("bighex"))
                instruction(0xF0 | register_or_alias(), 0x30);
            else
                immediate(0xA0, value_12bit());
        }
        else if (match("+="))
            instruction(0xF0 | register_or_alias(), 0x1E);
        else {
            auto t = next();
            _isError = 1, _error = fmt::format("{} is not an operator that can target the i register.", t.formatValue());
        }
    }
    else if (match("if")) {
        int index = (peek_match("key", 1) || peek_match("-key", 1)) ? 2 : 3;
        if (peek_match("then", index)) {
            conditional(0), expect("then");
        }
        else if (peek_match("begin", index)) {
            conditional(1), expect("begin");
            branches.push({here, _sourceLine, _sourcePos, "begin"});
            instruction(0x00, 0x00);
        }
        else {
            for (int z = 0; z <= index; z++)
                if (!is_end())
                    next();
            _isError = 1;
            _error = "Expected 'then' or 'begin'.";
        }
    }
    else if (match("else")) {
        if (branches.empty()) {
            _isError = 1;
            _error = "This 'else' does not have a matching 'begin'.";
            return;
        }
        jump(branches.top().addr, here + 2);
        branches.pop();
        branches.push({here, peek_line, peek_pos, "else"});
        instruction(0x00, 0x00);
    }
    else if (match("end")) {
        if (branches.empty()) {
            _isError = 1;
            _error = "This 'end' does not have a matching 'begin'.";
            return;
        }
        jump(branches.top().addr, here);
        branches.pop();
    }
    else if (match("loop")) {
        loops.push({here, peek_line, peek_pos, "loop"});
        whiles.push({-1, peek_line, peek_pos, "loop"});
    }
    else if (match("while")) {
        if (loops.empty()) {
            _isError = 1;
            _error = "This 'while' is not within a loop.";
            return;
        }
        conditional(1);
        whiles.push({here, peek_line, peek_pos, "while"});
        immediate(0x10, 0);  // forward jump
    }
    else if (match("again")) {
        if (loops.empty()) {
            _isError = 1;
            _error = "This 'again' does not have a matching 'loop'.";
            return;
        }
        immediate(0x10, loops.top().addr);
        loops.pop();
        while (true) {
            // works as loop always pushes a -1 while, but is it needed?
            int a = whiles.top().addr;
            whiles.pop();
            if (a == -1)
                break;
            jump(a, here);
        }
    }
    else if (match(":macro")) {
        auto n = identifier("macro");
        if (_isError)
            return;
        if (macros.count(n)) {
            _isError = 1, _error = fmt::format("The name '{}' has already been defined.", n);
            return;
        }
        auto& m = macros.emplace(n, Macro()).first->second;
        while (!_isError && !is_end() && !peek_match("{", 0))
            m.args.push_back(identifier("macro argument"));
        macro_body("macro", n, m);
    }
    else if (match(":stringmode")) {
        auto n = identifier("stringmode");
        if (_isError)
            return;
        auto& s = stringModes.try_emplace(n, StringMode()).first->second;
        int alpha_base = _sourcePos, alpha_quote = peekChar() == '"';
        auto alphabet = string();
        Macro m;  // every stringmode needs its own copy of this
        macro_body("string mode", n, m);
        for (int z = 0; z < alphabet.length(); z++) {
            int c = 0xFF & alphabet[z];
            if (s.modes[c]) {
                _errorPos = alpha_base + z + (alpha_quote ? 1 : 0);
                _isError = 1, _error = fmt::format("String mode '{}' is already defined for the character '{:c}'.", n, c);
                break;
            }
            s.values[c] = (char)z;
            auto mm = std::make_unique<Macro>();
            mm->body = m.body;
            s.modes[c] = std::move(mm);
        }
    }
    else {
        auto t = peek();
        if (_isError)
            return;
        if (t.type == Token::Type::NUMBER) {
            int n = (int)t.numValue;
            next();
            if (n < -128 || n > 255) {
                _isError = 1, _error = fmt::format("Literal value '{}' does not fit in a byte- must be in range [-128,255].", n);
            }
            append(n);
            return;
        }
        auto n = t.isText() ? t.strValue : std::string_view();
        if (auto mi = macros.find(n); mi != macros.end()) {
            next();
            auto& m = mi->second;
            std::unordered_map<std::string_view, Token> bindings;  // name -> tok
            bindings.emplace("CALLS", Token(m.calls++));
            for (auto& arg : m.args) {
                if (is_end()) {
                    _errorLine = _sourceLine, _errorPos = _sourcePos;
                    _isError = 1, _error = fmt::format("Not enough arguments for expansion of macro '{}'.", n);
                    break;
                }
                bindings.emplace(arg, next());
            }
            int splice_index = 0;
            for (int z = 0; z < m.body.size(); z++) {
                auto& bt = m.body[z];
                auto argIter = (bt.isText() ? bindings.find(bt.strValue) : bindings.end());
                tokens.insert(tokens.begin() + z, argIter != bindings.end() ? argIter->second : bt);
            }
        }
        else if (auto iter = stringModes.find(n); iter != stringModes.end()) {
            next();
            auto& s = iter->second;
            int text_base = _sourcePos, text_quote = peekChar() == '"';
            auto text = string();
            int splice_index = 0;
            for (int tz = 0; tz < text.length(); tz++) {
                int c = 0xFF & text[tz];
                if (!s.modes[c]) {
                    _errorPos = text_base + tz + (text_quote ? 1 : 0);
                    _isError = 1, _error = fmt::format("String mode '{}' is not defined for the character '{:c}'.", n, c);
                    break;
                }
                std::unordered_map<std::string_view, Token> bindings;  // name -> tok
                bindings.emplace("CALLS", Token(s.calls++));           // expansion count
                bindings.emplace("CHAR", Token(c));                    // ascii value of current char
                bindings.emplace("INDEX", Token((int)tz));             // index of char in input string
                bindings.emplace("VALUE", Token(s.values[c]));         // index of char in class alphabet
                auto& sm = *s.modes[c];
                for (auto& bt : sm.body) {
                    auto argIter = (bt.isText() ? bindings.find(bt.strValue) : bindings.end());
                    tokens.insert(tokens.begin() + splice_index++, argIter != bindings.end() ? argIter->second : bt);
                }
            }
        }
        else
            immediate(0x20, value_12bit());
    }
}
#else

// Parses directives of the form: %<len?>[bixc]
// - len omitted => 1
// - positive lengths can span multiple characters or more than four bytes
// Field text references the assembler's interned format string.
inline std::vector<MonitorField> parseMonitorFormat(std::string_view format)
{
    std::vector<MonitorField> fields;
    std::size_t literalBegin = 0;
    std::size_t offset = 0;
    for (std::size_t index = 0; index < format.size(); ++index) {
        if (format[index] != '%')
            continue;
        detail::pushLiteral(fields, format, literalBegin, index, offset);
        const auto countBegin = index + 1;
        auto specIndex = countBegin;
        std::size_t length = 0;
        while (specIndex < format.size() && format[specIndex] >= '0' && format[specIndex] <= '9') {
            const auto digit = static_cast<unsigned>(format[specIndex++] - '0');
            if (length > (std::numeric_limits<std::size_t>::max() - digit) / 10)
                throw std::invalid_argument("format byte count is too large.");
            length = length * 10 + digit;
        }
        const auto spec = specIndex < format.size() ? format[specIndex] : '\0';
        if (spec != 'c' && spec != 'i' && spec != 'b' && spec != 'x') {
            const auto character = countBegin < format.size()
                ? std::string(1, format[countBegin]) : std::string("undefined");
            throw std::invalid_argument(fmt::format(
                "Unknown format character %{}- should be %Nc,%Ni,%Nb, or %Nx.", character));
        }
        if (specIndex == countBegin)
            length = 1;
        else if (format.substr(countBegin, specIndex - countBegin) == "0")
            throw std::invalid_argument("format byte count cannot be 0.");
        fields.push_back(MonitorField{
            .type = detail::typeFromLen(length),
            .base = detail::baseFromSpec(spec),
            .offset = offset,
            .text = format.substr(index, specIndex + 1 - index),
            .length = length,
        });
        if (offset > std::numeric_limits<std::size_t>::max() - length)
            throw std::invalid_argument("format byte offset is too large.");
        offset += length;
        index = specIndex;
        literalBegin = index + 1;
    }
    detail::pushLiteral(fields, format, literalBegin, format.size(), offset);
    return fields;
}

bool Assembler::expandMacroOrStringMode()
{
    const auto token = peek();
    if (_isError || !token.isText())
        return false;
    const auto name = token.strValue;
    if (auto iter = macros.find(name); iter != macros.end()) {
        next();
        auto& macro = iter->second;
        std::unordered_map<std::string_view, Token> bindings;
        bindings.emplace("CALLS", Token(macro.calls++));
        for (const auto argument : macro.args) {
            if (is_end()) {
                _isError = 1;
                _errorLine = _sourceLine;
                _errorPos = _sourcePos;
                _error = fmt::format("Not enough arguments for expansion of macro '{}'.", name);
                return true;
            }
            bindings.insert_or_assign(argument, next());
        }
        std::size_t insertion = 0;
        for (const auto& bodyToken : macro.body) {
            const auto binding = bodyToken.isText() ? bindings.find(bodyToken.strValue) : bindings.end();
            tokens.insert(tokens.begin() + insertion++, binding != bindings.end() ? binding->second : bodyToken);
        }
        return true;
    }
    if (auto iter = stringModes.find(name); iter != stringModes.end()) {
        next();
        auto& mode = iter->second;
        const auto textToken = peek();
        const auto text = detail::toUtf16(string());
        if (_isError)
            return true;
        std::size_t insertion = 0;
        for (std::size_t index = 0; index < text.size(); ++index) {
            const auto character = text[index];
            const auto macro = mode.modes.find(character);
            if (macro == mode.modes.end()) {
                _isError = 1;
                _errorLine = textToken.line;
                _errorPos = textToken.pos + static_cast<int>(index) + 1;
                _error = fmt::format("String mode '{}' is not defined for the character '{}'.", name, detail::utf8Unit(character));
                return true;
            }
            std::unordered_map<std::string_view, Token> bindings;
            bindings.emplace("CALLS", Token(mode.calls++));
            bindings.emplace("CHAR", Token(static_cast<int>(character)));
            bindings.emplace("INDEX", Token(static_cast<int>(index)));
            bindings.emplace("VALUE", Token(mode.values.at(character)));
            for (const auto& bodyToken : macro->second.body) {
                const auto binding = bodyToken.isText() ? bindings.find(bodyToken.strValue) : bindings.end();
                tokens.insert(tokens.begin() + insertion++, binding != bindings.end() ? binding->second : bodyToken);
            }
        }
        return true;
    }
    return false;
}

void Assembler::compileStatement()
{
    if (_isError)
        return;
    if (peek().tid == TokenId::ASM) {
        compileNativeBlock(next());
        return;
    }
    if (expandMacroOrStringMode())
        return;
    const auto statement = peek();
    const auto statementId = statement.isText() ? tokenIdForText(statement.strValue) : statement.tid;
    int peek_line = statement.line, peek_pos = statement.pos;
    _errorLine = statement.line;
    _errorPos = statement.pos;
    if (statementId == TokenId::TOK_UNKNOWN && peekIsRegister()) {
        int r = registerOrAlias();
        if (match(":=")) {
            if (peekIsRegister())
                instruction(0x80 | r, (registerOrAlias() << 4) | 0x0);
            else if (match("random"))
                instruction(0xC0 | r, value8bit());
            else if (match("key"))
                instruction(0xF0 | r, 0x0A);
            else if (match("delay"))
                instruction(0xF0 | r, 0x07);
            else
                instruction(0x60 | r, value8bit());
        }
        else if (match("+=")) {
            if (peekIsRegister())
                instruction(0x80 | r, (registerOrAlias() << 4) | 0x4);
            else
                instruction(0x70 | r, value8bit());
        }
        else if (match("-=")) {
            if (peekIsRegister())
                instruction(0x80 | r, (registerOrAlias() << 4) | 0x5);
            else
                instruction(0x70 | r, 1 + ~value8bit());
        }
        else if (match("|="))
            instruction(0x80 | r, (registerOrAlias() << 4) | 0x1);
        else if (match("&="))
            instruction(0x80 | r, (registerOrAlias() << 4) | 0x2);
        else if (match("^="))
            instruction(0x80 | r, (registerOrAlias() << 4) | 0x3);
        else if (match("=-"))
            instruction(0x80 | r, (registerOrAlias() << 4) | 0x7);
        else if (match(">>="))
            instruction(0x80 | r, (registerOrAlias() << 4) | 0x6);
        else if (match("<<="))
            instruction(0x80 | r, (registerOrAlias() << 4) | 0xE);
        else {
            auto t = next();
            if (!_isError)
                _isError = 1, _error = fmt::format("Unrecognized operator {}.", t.formatValue());
        }
    }
    else {
        if (!_isError && !is_end() && tokens.empty())
            fetchToken();
        if (is_end() || _isError)
            return;
        switch (statementId) {
            case TokenId::COLON:
                eat();
                resolveLabel(0);
                break;
            case TokenId::NEXT:
                eat();
                resolveLabel(1);
                break;
            case TokenId::UNPACK: {
                eat();
                int a = 0;
                if (match("long")) {
                    a = value16bit(1, 0);
                }
                else {
                    int v = value4bit();
                    a = (v << 12) | value12bit();
                }
                auto rh = aliases["unpack-hi"];
                auto rl = aliases["unpack-lo"];
                instruction(0x60 | rh, a >> 8);
                instruction(0x60 | rl, a);
                break;
            }
            case TokenId::BREAKPOINT:
                eat();
                breakpoints[here] = string();
                break;
            case TokenId::MONITOR: {
                eat();
                const auto nameToken = peek();
                const auto name = safeStringStringView(nameToken.isText() ? nameToken.strValue : nameToken.formatValue());
                const bool registerMonitor = peekIsRegister();
                const auto base = registerMonitor ? registerOrAlias() : value16bit(0, 0);
                const auto formatToken = peek();
                const bool hasFormat = formatToken.isText() && formatToken.strValue.find('%') != std::string_view::npos;
                const auto length = hasFormat ? -1 : registerMonitor ? value4bit() : value16bit(0, 0);
                std::vector<MonitorField> fields;
                if (hasFormat && !_isError) {
                    const auto format = string();
                    try {
                        fields = parseMonitorFormat(format);
                    }
                    catch (const std::invalid_argument& error) {
                        _isError = 1;
                        _errorLine = formatToken.line;
                        _errorPos = formatToken.pos;
                        _error = error.what();
                    }
                }
                if (!_isError)
                    monitors.insert_or_assign(name, Monitor{registerMonitor ? 0 : 1, base, length, std::move(fields)});
                break;
            }
            case TokenId::ASSERT: {
                eat();
                auto message = peek_match("{", 0) ? std::string_view() : string();
                if (!detail::isTruthy(calculated("assertion"))) {
                    _isError = 1;
                    if (!message.empty())
                        _error = fmt::format("Assertion failed: {}", message);
                    else
                        _error = "Assertion failed.";
                }
                break;
            }
            case TokenId::PROTO:
                eat();
                next();  // deprecated
                break;
            case TokenId::ALIAS: {
                eat();
                auto n = identifier("alias");
                if (_programLabels.count(n)) {
                    _isError = 1, _error = fmt::format("The name '{}' is already used by a constant.", n);
                    return;
                }
                int v = peek_match("{", 0) ? (int)calculated("ANONYMOUS") : (int)registerOrAlias();
                if (v < 0 || v > 15) {
                    _isError = 1;
                    _error = "Register index must be in the range [0,F].";
                    return;
                }
                aliases[n] = v;
                break;
            }
            case TokenId::BYTE: {
                eat();
                append(peek_match("{", 0) ? detail::toUint8JS(calculated("ANONYMOUS")) : value8bit());
                break;
            }
            case TokenId::POINTER:
            case TokenId::POINTER16: {
                eat();
                int a = peek_match("{", 0) ? detail::toInt32JS(calculated("ANONYMOUS")) : value16bit(1, 0);
                instruction(a >> 8, a);
                break;
            }
            case TokenId::POINTER24: {
                eat();
                int a;
                if (peek_match("{", 0)) {
                    const auto valuePos = _sourcePos;
                    const auto value = calculated("ANONYMOUS");
                    if (_isError)
                        return;
                    _errorPos = valuePos;
                    a = checkedAddress(value, 24);
                }
                else
                    a = value24bit(1, 0);
                append(a >> 16);
                instruction(a >> 8, a);
                break;
            }
            case TokenId::ORG: {
                eat();
                int newAddress;
                if (peek_match("{", 0)) {
                    const auto valuePos = _sourcePos;
                    const auto value = calculated("ANONYMOUS");
                    if (_isError)
                        return;
                    _errorPos = valuePos;
                    if (value < 0) {
                        _isError = 1;
                        _error = "Argument to :org is negative.";
                        return;
                    }
                    newAddress = checkedAddress(value, 24);
                }
                else {
                    newAddress = checkedAddress(valueConstant().value, 24);
                }
                if (_isError)
                    return;
                if (newAddress < here && newAddress < used.size() && used[newAddress] && newAddress != 0x200) {
                    _isError = 1;
                    _error = fmt::format("Data overlap by {} bytes. Address 0x{:0X} has already been defined.", here - newAddress, here);
                }
                here = newAddress;
                break;
            }
            case TokenId::CALL: {
                eat();
                if (peek_match("{", 0)) {
                    const auto label = detail::toInt32JS(calculated("ANONYMOUS"));
                    immediate(0x20, label & 0xFFF);
                }
                else {
                    immediate(0x20, value12bit());
                }
                break;
            }
            case TokenId::CONST_DIRECTIVE: {
                eat();
                auto n = identifier("constant");
                if (constants.contains(n)) {
                    _isError = 1;
                    _error = fmt::format("The name '{}' has already been defined.", n);
                    return;
                }
                constants.insert_or_assign(n, valueConstant());
                break;
            }
            case TokenId::CALC: {
                eat();
                auto n = identifier("calculated constant");
                auto iter = constants.find(n);
                if (iter != constants.end() && !iter->second.isMutable) {
                    _isError = 1, _error = fmt::format("Cannot redefine the name '{}' with :calc.", n);
                    return;
                }
                constants.insert_or_assign(n, Constant{calculated(n), true});
                break;
            }
            case TokenId::SEMICOLON:
            case TokenId::RETURN:
                eat(), instruction(0x00, 0xEE);
                break;
            case TokenId::CLEAR:
                eat(), instruction(0x00, 0xE0);
                break;
            case TokenId::BCD:
                eat(), instruction(0xF0 | registerOrAlias(), 0x33);
                break;
            case TokenId::DELAY:
                eat(), expect(":="), instruction(0xF0 | registerOrAlias(), 0x15);
                break;
            case TokenId::BUZZER:
                eat(), expect(":="), instruction(0xF0 | registerOrAlias(), 0x18);
                break;
            case TokenId::PITCH:
                eat(), expect(":="), instruction(0xF0 | registerOrAlias(), 0x3A);
                break;
            case TokenId::JUMP0:
                eat(), immediate(0xB0, value12bit());
                break;
            case TokenId::JUMP:
                eat(), immediate(0x10, value12bit());
                break;
            case TokenId::NATIVE:
                eat(), immediate(0x00, value12bit());
                break;
            case TokenId::AUDIO:
                eat(), instruction(0xF0, 0x02);
                break;
            case TokenId::SCROLL_DOWN:
                eat(), instruction(0x00, 0xC0 | value4bit());
                break;
            case TokenId::SCROLL_UP:
                eat(), instruction(0x00, 0xD0 | value4bit());
                break;
            case TokenId::SCROLL_RIGHT:
                eat(), instruction(0x00, 0xFB);
                break;
            case TokenId::SCROLL_LEFT:
                eat(), instruction(0x00, 0xFC);
                break;
            case TokenId::EXIT:
                eat(), instruction(0x00, 0xFD);
                break;
            case TokenId::LORES:
                eat(), instruction(0x00, 0xFE);
                break;
            case TokenId::HIRES:
                eat(), instruction(0x00, 0xFF);
                break;
            case TokenId::SPRITE: {
                eat();
                int x = registerOrAlias(), y = registerOrAlias();
                instruction(0xD0 | x, (y << 4) | value4bit());
                break;
            }
            case TokenId::PLANE: {
                eat();
                int n = value4bit();
                if (n > 3)
                    _isError = 1, _error = fmt::format("The plane bitmask must be [0,3], was {}.", n);
                instruction(0xF0 | n, 0x01);
                break;
            }
            case TokenId::SAVEFLAGS:
                eat(), instruction(0xF0 | registerOrAlias(), 0x75);
                break;
            case TokenId::LOADFLAGS:
                eat(), instruction(0xF0 | registerOrAlias(), 0x85);
                break;
            case TokenId::SAVE: {
                eat();
                int r = registerOrAlias();
                if (match("-"))
                    instruction(0x50 | r, (registerOrAlias() << 4) | 0x02);
                else
                    instruction(0xF0 | r, 0x55);
                break;
            }
            case TokenId::LOAD: {
                eat();
                int r = registerOrAlias();
                if (match("-"))
                    instruction(0x50 | r, (registerOrAlias() << 4) | 0x03);
                else
                    instruction(0xF0 | r, 0x65);
                break;
            }
            case TokenId::I_REG: {
                eat();
                if (match(":=")) {
                    if (match("long")) {
                        int a = value16bit(1, 2);
                        instruction(0xF0, 0x00);
                        instruction((a >> 8), a);
                    }
                    else if (match("hex"))
                        instruction(0xF0 | registerOrAlias(), 0x29);
                    else if (match("bighex"))
                        instruction(0xF0 | registerOrAlias(), 0x30);
                    else
                        immediate(0xA0, value12bit());
                }
                else if (match("+="))
                    instruction(0xF0 | registerOrAlias(), 0x1E);
                else {
                    auto t = next();
                    _isError = 1, _error = fmt::format("{} is not an operator that can target the i register.", t.formatValue());
                }
                break;
            }
            case TokenId::IF: {
                eat();
                int index = (peek_match("key", 1) || peek_match("-key", 1)) ? 2 : 3;
                if (peek_match("then", index)) {
                    conditional(0), expect("then");
                }
                else if (peek_match("begin", index)) {
                    conditional(1), expect("begin");
                    branches.push({here, _sourceLine, _sourcePos, "begin"});
                    instruction(0x00, 0x00);
                }
                else {
                    for (int z = 0; z <= index; z++)
                        if (!is_end())
                            next();
                    _isError = 1;
                    _error = "Expected 'then' or 'begin'.";
                }
                break;
            }
            case TokenId::ELSE: {
                eat();
                if (branches.empty()) {
                    _isError = 1;
                    _error = "This 'else' does not have a matching 'begin'.";
                    return;
                }
                if (std::string_view{branches.top().type} == "else") {
                    _isError = 1;
                    _error = "This 'if' already has an 'else' branch.";
                    return;
                }
                jump(branches.top().addr, here + 2);
                branches.pop();
                branches.push({here, peek_line, peek_pos, "else"});
                instruction(0x00, 0x00);
                break;
            }
            case TokenId::END: {
                eat();
                if (branches.empty()) {
                    _isError = 1;
                    _error = "This 'end' does not have a matching 'begin'.";
                    return;
                }
                jump(branches.top().addr, here);
                branches.pop();
                break;
            }
            case TokenId::LOOP: {
                eat();
                loops.push({here, peek_line, peek_pos, "loop"});
                whiles.push({-1, peek_line, peek_pos, "loop"});
                break;
            }
            case TokenId::WHILE: {
                eat();
                if (loops.empty()) {
                    _isError = 1;
                    _error = "This 'while' is not within a loop.";
                    return;
                }
                conditional(1);
                whiles.push({here, peek_line, peek_pos, "while"});
                immediate(0x10, 0);  // forward jump
                break;
            }
            case TokenId::AGAIN: {
                eat();
                if (loops.empty()) {
                    _isError = 1;
                    _error = "This 'again' does not have a matching 'loop'.";
                    return;
                }
                immediate(0x10, loops.top().addr);
                loops.pop();
                while (true) {
                    // works as loop always pushes a -1 while, but is it needed?
                    int a = whiles.top().addr;
                    whiles.pop();
                    if (a == -1)
                        break;
                    jump(a, here);
                }
                break;
            }
            case TokenId::MACRO: {
                eat();
                auto n = identifier("macro");
                if (_isError)
                    return;
                if (macros.count(n)) {
                    _isError = 1, _error = fmt::format("The name '{}' has already been defined.", n);
                    return;
                }
                auto& m = macros.emplace(n, Macro()).first->second;
                while (!_isError && !is_end() && !peek_match("{", 0))
                    m.args.push_back(identifier("macro argument"));
                macroBody("macro", n, m);
                break;
            }
            case TokenId::STRINGMODE: {
                eat();
                auto n = identifier("stringmode");
                if (_isError)
                    return;
                auto& s = stringModes.try_emplace(n, StringMode()).first->second;
                const auto alphabetToken = peek();
                const auto alphabet = detail::toUtf16(string());
                Macro macro;
                macroBody("string mode", n, macro);
                for (std::size_t index = 0; index < alphabet.size() && !_isError; ++index) {
                    const auto character = alphabet[index];
                    if (s.modes.contains(character)) {
                        _errorLine = alphabetToken.line;
                        _errorPos = alphabetToken.pos + static_cast<int>(index) + 1;
                        _isError = 1;
                        _error = fmt::format("String mode '{}' is already defined for the character '{}'.", n, detail::utf8Unit(character));
                        break;
                    }
                    s.values[character] = static_cast<int>(index);
                    s.modes.emplace(character, macro);
                }
                break;
            }
            default: {
                auto t = peek();
                if (_isError)
                    return;
                if (t.type == Token::Type::NUMBER) {
                    const double n = t.numValue;
                    next();
                    if (n < -128 || n > 255) {
                        _isError = 1, _error = fmt::format("Literal value '{}' does not fit in a byte- must be in range [-128,255].", n);
                    }
                    append(detail::toUint8JS(n));
                    return;
                }
                if (!t.strValue.empty())
                    immediate(0x20, value12bit());
                else {
                    // Just drop it, this is the end.
                    next();
                }
            }
        }
    }
}
#endif

cdp1802::AssemblerOptions Assembler::nativeOptions(const NativeBlock& block, bool measureOnly)
{
    cdp1802::AssemblerOptions options;
    options.embedded = true;
    options.measureOnly = measureOnly;
    options.origin = static_cast<uint16_t>(block.address);
    options.resolveSymbol = [this, &block](std::string_view name) -> std::optional<double> {
        if (const auto it = block.values.find(std::string{name}); it != block.values.end())
            return it->second;
        if (const auto it = _programLabels.find(name); it != _programLabels.end())
            return it->second;
        return std::nullopt;
    };
    return options;
}

void Assembler::nativeError(const NativeBlock& block, const cdp1802::Assembler& assembler)
{
    _isError = 1;
    _error = "cdp1802: " + assembler.errorMessage();
    _errorLine = block.token.nativeLine + assembler.errorLine() - 1;
    _errorPos = (assembler.errorLine() == 1 ? block.token.nativePos : 0) + assembler.errorColumn() - 1;
}

void Assembler::compileNativeBlock(Token token)
{
    if (token.nativeCpu != "cdp1802") {
        _isError = 1;
        _error = fmt::format("Unsupported :asm CPU '{}'; only cdp1802 is supported.", token.nativeCpu);
        return;
    }
    if (here < 0 || here > 0xFFFF) {
        _isError = 1;
        _error = ":asm cdp1802 requires a 16-bit origin.";
        return;
    }
    NativeBlock block{std::move(token), here};
    for (const auto& [name, value] : constants)
        block.values.emplace(name, value.value);
    cdp1802::Assembler assembler{block.token.strValue, nativeOptions(block, true)};
    if (!assembler.compile()) {
        nativeError(block, assembler);
        return;
    }
    block.size = assembler.embeddedSize();
    for (uint32_t byte = 0; byte < block.size && !_isError; ++byte)
        append(0);
    if (!_isError)
        _nativeBlocks.push_back(std::move(block));
}

bool Assembler::finishNativeBlocks()
{
    for (const auto& block : _nativeBlocks) {
        cdp1802::Assembler assembler{block.token.strValue, nativeOptions(block, false)};
        if (!assembler.compile()) {
            nativeError(block, assembler);
            return false;
        }
        if (assembler.contiguousData().size() != block.size) {
            _isError = 1;
            _error = "Native block changed size after resolving symbols.";
            _errorLine = block.token.line;
            _errorPos = block.token.pos;
            return false;
        }
        for (uint32_t byte = 0; byte < block.size; ++byte) {
            rom[block.address + byte] = assembler.contiguousData()[byte];
            romLineMap[block.address + byte] = block.token.nativeLine + assembler.byteSourceLines()[byte] - 1;
        }
    }
    return true;
}

Assembler::Assembler(std::string_view text, int startAddress)
: Lexer(text)
{
    has_main = 1;
    here = startAddress;
    this->startAddress = startAddress;
    length = 0;
    rom.resize(65536, 0);
    used.resize(65536, 0);
    romLineMap.resize(65536, 0xFFFFFFFF);

    if ((unsigned char)_source[0] == 0xEF && (unsigned char)_source[1] == 0xBB && (unsigned char)_source[2] == 0xBF)
        _source += 3;  // UTF-8 BOM
    skipWhitespace();

#define octo_kc(l, n) constants.emplace(("OCTO_KEY_" l), Constant{n, 0})
    octo_kc("1", 0x1), octo_kc("2", 0x2), octo_kc("3", 0x3), octo_kc("4", 0xC), octo_kc("Q", 0x4), octo_kc("W", 0x5), octo_kc("E", 0x6), octo_kc("R", 0xD), octo_kc("A", 0x7), octo_kc("S", 0x8), octo_kc("D", 0x9), octo_kc("F", 0xE), octo_kc("Z", 0xA),
        octo_kc("X", 0x0), octo_kc("C", 0xB), octo_kc("V", 0xF);

    aliases["unpack-hi"] = 0;
    aliases["unpack-lo"] = 1;
}

bool Assembler::compile()
{
    instruction(0x00, 0x00);  // reserve a jump slot for main
    while (!is_end() && !_isError) {
        _errorLine = _sourceLine;
        _errorPos = _sourcePos;
        compileStatement();
    }
    if (_isError)
        return false;
    while (length > startAddress && !used[length - 1])
        length--;
    _errorLine = _sourceLine, _errorPos = _sourcePos;

    if (has_main) {
        auto iter = constants.find("main");
        if (iter == constants.end() && !_programLabels.contains("main"))
            return _isError = 1, _error = "This program is missing a 'main' label.", false;
        jump(startAddress, iter != constants.end() ? detail::toInt32JS(iter->second.value) : _programLabels.at("main"));
    }
    if (!finishNativeBlocks())
        return false;
    if (!protos.empty()) {
        auto& pr = protos.begin()->second;
        _errorLine = pr.line, _errorPos = pr.pos;
        _isError = 1;
        _error = fmt::format("Undefined forward reference: {}", protos.begin()->first);
        return false;
    }
    if (!loops.empty()) {
        _isError = 1;
        _error = "This 'loop' does not have a matching 'again'.";
        _errorLine = loops.top().line, _errorPos = loops.top().pos;
        return false;
    }
    if (!branches.empty()) {
        _isError = 1;
        _error = fmt::format("This '{}' does not have a matching 'end'.", branches.top().type);
        _errorLine = branches.top().line, _errorPos = branches.top().pos;
        return false;
    }
    return true;  // success!
}


}
