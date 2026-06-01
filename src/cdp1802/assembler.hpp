//---------------------------------------------------------------------------------------
// cdp1802/assembler.hpp
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

#include "lexer.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace cdp1802 {

enum class ListingMode {
    NONE,
    TRADITIONAL,
    MODERN
};

struct AssemblerOptions
{
    ListingMode listingMode{ListingMode::NONE};
};

struct AssemblySegment
{
    uint16_t address{};
    std::vector<uint8_t> bytes;
};

class Assembler
{
public:
    enum class OperandKind { CONTROL, IO, REGISTER, IMMEDIATE, BRANCH, LONG_BRANCH };

    struct Instruction {
        uint8_t opcode{};
        OperandKind kind{};
    };

    explicit Assembler(std::string_view text, AssemblerOptions options = {});

    bool compile();
    bool isError() const { return !_errorMessage.empty(); }
    const std::string& errorMessage() const { return _errorMessage; }
    int errorLine() const { return _errorLine; }
    int errorColumn() const { return _errorColumn; }
    const std::vector<AssemblySegment>& segments() const { return _segments; }
    const std::vector<uint8_t>& contiguousData() const { return _contiguousData; }
    const std::string& listing() const { return _listing; }
    uint16_t startAddress() const { return _startAddress; }

private:
    enum class Pass { SYMBOLS, EMIT };

    struct Value {
        int number{};
        bool known{true};
    };

    void resetCursor();
    bool parseProgram(Pass pass);
    void parseStatement(Pass pass);
    void parseLabelOrEquate(Pass pass);
    void parseDirective(Pass pass, std::string_view name);
    void parseInstruction(Pass pass, std::string_view mnemonic, const Instruction& instruction);
    void parseData(Pass pass);
    void parseDataList(Pass pass);
    std::vector<uint8_t> parseDataValue(Pass pass);
    Value parseExpression(Pass pass);
    Value parsePrimary(Pass pass);
    Value parseAddressFunction(Pass pass);
    uint8_t valueByte(Value value, std::string_view what);
    uint16_t valueAddress(Value value, std::string_view what);
    uint8_t parseRegister(Pass pass);
    uint8_t parsePort(Pass pass);
    void emit(Pass pass, uint8_t byte);
    void setOrigin(Pass pass, uint16_t address);
    void buildListing();
    void buildTraditionalListing();
    void buildModernListing();
    std::string byteTextForLine(int lineNumber, bool spaced) const;
    std::vector<std::string_view> sourceLines() const;
    void skipStatement();
    void skipLine();
    void consumeSeparators();
    bool atStatementEnd() const;
    bool match(TokenType type);
    bool matchIdentifier(std::string_view text);
    bool check(TokenType type) const;
    bool checkIdentifier(std::string_view text) const;
    const Token& peek(size_t offset = 0) const;
    const Token& previous() const;
    const Token& advance();
    void expect(TokenType type, std::string_view what);
    void fail(const Token& token, std::string message);

    static const std::unordered_map<std::string_view, Instruction>& instructions();
    static bool isInstruction(std::string_view text);
    static bool isDirective(std::string_view text);

    std::string_view _source;
    AssemblerOptions _options;
    Lexer _lexer;
    std::vector<Token> _tokens;
    size_t _current{};
    uint16_t _pc{};
    uint16_t _statementAddress{};
    int _statementLine{};
    uint16_t _startAddress{};
    bool _haveStartAddress{};
    bool _finished{};
    std::unordered_map<std::string, int> _symbols;
    std::vector<AssemblySegment> _segments;
    std::vector<uint8_t> _contiguousData;
    std::map<int, std::vector<uint8_t>> _listingBytesByLine;
    std::map<int, uint16_t> _listingAddressByLine;
    std::map<int, uint16_t> _listingEndAddressByLine;
    std::string _listing;
    std::string _errorMessage;
    int _errorLine{};
    int _errorColumn{};
};

} // namespace cdp1802
