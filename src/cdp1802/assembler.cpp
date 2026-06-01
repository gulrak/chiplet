#include "assembler.hpp"

#include <algorithm>
#include <fmt/format.h>

namespace cdp1802 {

namespace {

using InstructionMap = std::unordered_map<std::string_view, Assembler::Instruction>;

InstructionMap makeInstructionMap()
{
    using enum Assembler::OperandKind;
    return {
        {"IDL", {0x00, CONTROL}},
        {"LDN", {0x00, REGISTER}},
        {"INC", {0x10, REGISTER}},
        {"DEC", {0x20, REGISTER}},
        {"BR", {0x30, BRANCH}},
        {"BQ", {0x31, BRANCH}},
        {"BZ", {0x32, BRANCH}},
        {"BDF", {0x33, BRANCH}},
        {"BPZ", {0x33, BRANCH}},
        {"BGE", {0x33, BRANCH}},
        {"B1", {0x34, BRANCH}},
        {"B2", {0x35, BRANCH}},
        {"B3", {0x36, BRANCH}},
        {"B4", {0x37, BRANCH}},
        {"NBR", {0x38, BRANCH}},
        {"SKP", {0x38, CONTROL}},
        {"BNQ", {0x39, BRANCH}},
        {"BNZ", {0x3A, BRANCH}},
        {"BNF", {0x3B, BRANCH}},
        {"BM", {0x3B, BRANCH}},
        {"BL", {0x3B, BRANCH}},
        {"BN1", {0x3C, BRANCH}},
        {"BN2", {0x3D, BRANCH}},
        {"BN3", {0x3E, BRANCH}},
        {"BN4", {0x3F, BRANCH}},
        {"LDA", {0x40, REGISTER}},
        {"STR", {0x50, REGISTER}},
        {"IRX", {0x60, CONTROL}},
        {"OUT", {0x60, IO}},
        {"OUT1", {0x61, CONTROL}},
        {"OUT2", {0x62, CONTROL}},
        {"OUT3", {0x63, CONTROL}},
        {"OUT4", {0x64, CONTROL}},
        {"OUT5", {0x65, CONTROL}},
        {"OUT6", {0x66, CONTROL}},
        {"OUT7", {0x67, CONTROL}},
        {"INP", {0x68, IO}},
        {"INP1", {0x69, CONTROL}},
        {"INP2", {0x6A, CONTROL}},
        {"INP3", {0x6B, CONTROL}},
        {"INP4", {0x6C, CONTROL}},
        {"INP5", {0x6D, CONTROL}},
        {"INP6", {0x6E, CONTROL}},
        {"INP7", {0x6F, CONTROL}},
        {"RET", {0x70, CONTROL}},
        {"DIS", {0x71, CONTROL}},
        {"LDXA", {0x72, CONTROL}},
        {"STXD", {0x73, CONTROL}},
        {"ADC", {0x74, CONTROL}},
        {"SDB", {0x75, CONTROL}},
        {"SHRC", {0x76, CONTROL}},
        {"RSHR", {0x76, CONTROL}},
        {"SMB", {0x77, CONTROL}},
        {"SAV", {0x78, CONTROL}},
        {"MARK", {0x79, CONTROL}},
        {"REQ", {0x7A, CONTROL}},
        {"SEQ", {0x7B, CONTROL}},
        {"ADDI", {0x7C, IMMEDIATE}},
        {"SDBI", {0x7D, IMMEDIATE}},
        {"SHLC", {0x7E, CONTROL}},
        {"RSHL", {0x7E, CONTROL}},
        {"SMBI", {0x7F, IMMEDIATE}},
        {"GLO", {0x80, REGISTER}},
        {"GHI", {0x90, REGISTER}},
        {"PLO", {0xA0, REGISTER}},
        {"PHI", {0xB0, REGISTER}},
        {"LBR", {0xC0, LONG_BRANCH}},
        {"LBQ", {0xC1, LONG_BRANCH}},
        {"LBZ", {0xC2, LONG_BRANCH}},
        {"LBDF", {0xC3, LONG_BRANCH}},
        {"NOP", {0xC4, CONTROL}},
        {"LSNQ", {0xC5, CONTROL}},
        {"LSNZ", {0xC6, CONTROL}},
        {"LSNF", {0xC7, CONTROL}},
        {"LSKP", {0xC8, CONTROL}},
        {"NLBR", {0xC8, LONG_BRANCH}},
        {"LBNQ", {0xC9, LONG_BRANCH}},
        {"LBNZ", {0xCA, LONG_BRANCH}},
        {"LBNF", {0xCB, LONG_BRANCH}},
        {"LSIE", {0xCC, CONTROL}},
        {"LSQ", {0xCD, CONTROL}},
        {"LSZ", {0xCE, CONTROL}},
        {"LSDF", {0xCF, CONTROL}},
        {"SEP", {0xD0, REGISTER}},
        {"CALL", {0xD4, LONG_BRANCH}},
        {"EXIT", {0xD5, CONTROL}},
        {"SEX", {0xE0, REGISTER}},
        {"LDX", {0xF0, CONTROL}},
        {"OR", {0xF1, CONTROL}},
        {"AND", {0xF2, CONTROL}},
        {"XOR", {0xF3, CONTROL}},
        {"ADD", {0xF4, CONTROL}},
        {"SD", {0xF5, CONTROL}},
        {"SHR", {0xF6, CONTROL}},
        {"SM", {0xF7, CONTROL}},
        {"LDI", {0xF8, IMMEDIATE}},
        {"ORI", {0xF9, IMMEDIATE}},
        {"ANI", {0xFA, IMMEDIATE}},
        {"XRI", {0xFB, IMMEDIATE}},
        {"ADI", {0xFC, IMMEDIATE}},
        {"SDI", {0xFD, IMMEDIATE}},
        {"SHL", {0xFE, CONTROL}},
        {"SMI", {0xFF, IMMEDIATE}},
    };
}

} // namespace

Assembler::Assembler(std::string_view text, AssemblerOptions options)
    : _source{text}
    , _options{options}
    , _lexer{text}
{
}

bool Assembler::compile()
{
    if (_lexer.isError()) {
        _errorMessage = _lexer.errorMessage();
        _errorLine = 1;
        _errorColumn = 1;
        return false;
    }

    _tokens = _lexer.tokens();
    _symbols.clear();
    for (int reg = 0; reg < 16; ++reg) {
        const char suffix = reg < 10 ? static_cast<char>('0' + reg) : static_cast<char>('A' + reg - 10);
        _symbols[fmt::format("R{}", suffix)] = reg;
    }
    _segments.clear();
    _contiguousData.clear();
    _listing.clear();
    _listingBytesByLine.clear();
    _listingAddressByLine.clear();
    _listingEndAddressByLine.clear();
    _errorMessage.clear();
    _finished = false;
    _pc = 0;
    _startAddress = 0;
    _haveStartAddress = false;

    if (!parseProgram(Pass::SYMBOLS))
        return false;

    resetCursor();
    _finished = false;
    _pc = 0;
    _segments.clear();
    _contiguousData.clear();
    _listingBytesByLine.clear();
    _listingAddressByLine.clear();
    _listingEndAddressByLine.clear();
    _haveStartAddress = false;
    if (!parseProgram(Pass::EMIT))
        return false;
    if (_options.listingMode != ListingMode::NONE)
        buildListing();
    return true;
}

void Assembler::resetCursor()
{
    _current = 0;
}

bool Assembler::parseProgram(Pass pass)
{
    resetCursor();
    consumeSeparators();
    while (!check(TokenType::END_OF_FILE) && !_finished && !isError()) {
        parseStatement(pass);
        consumeSeparators();
    }
    return !isError();
}

void Assembler::parseStatement(Pass pass)
{
    _statementAddress = _pc;
    _statementLine = peek().line;
    if (pass == Pass::EMIT)
        _listingAddressByLine.try_emplace(_statementLine, _statementAddress);

    if (check(TokenType::COMMA)) {
        parseData(pass);
        return;
    }

    if (!check(TokenType::IDENTIFIER)) {
        skipStatement();
        return;
    }

    if (peek(1).type == TokenType::COLON && (peek(2).type == TokenType::EQUAL || (peek(2).type == TokenType::IDENTIFIER && peek(2).text == "EQU"))) {
        const auto name = advance().text;
        advance();
        advance();
        const auto value = parseExpression(pass);
        if (value.known)
            _symbols[name] = value.number;
        return;
    }

    parseLabelOrEquate(pass);
    if (atStatementEnd())
        return;

    if (check(TokenType::COMMA)) {
        parseData(pass);
        return;
    }

    if (!check(TokenType::IDENTIFIER)) {
        fail(peek(), "Expected instruction or directive");
        return;
    }

    const auto name = advance().text;
    if (isDirective(name)) {
        parseDirective(pass, name);
        return;
    }

    const auto iter = instructions().find(name);
    if (iter == instructions().end()) {
        fail(previous(), fmt::format("Unknown CDP1802 instruction '{}'", name));
        return;
    }
    parseInstruction(pass, name, iter->second);
}

void Assembler::parseLabelOrEquate(Pass pass)
{
    if (!check(TokenType::IDENTIFIER))
        return;

    const auto name = peek().text;
    if (peek(1).type == TokenType::EQUAL || checkIdentifier("EQU")) {
        advance();
        if (match(TokenType::EQUAL) || matchIdentifier("EQU")) {
            const auto value = parseExpression(pass);
            if (value.known)
                _symbols[name] = value.number;
        }
        return;
    }

    if (peek(1).type == TokenType::COLON) {
        advance();
        advance();
        if (pass == Pass::SYMBOLS)
            _symbols[name] = _pc;
    }
}

void Assembler::parseDirective(Pass pass, std::string_view name)
{
    if (name == "ORG") {
        const auto address = valueAddress(parseExpression(pass), "origin");
        setOrigin(pass, address);
    }
    else if (name == "EQU") {
        fail(previous(), "EQU requires a symbol name");
    }
    else if (name == "DC" || name == "DB") {
        parseDataList(pass);
    }
    else if (name == "END") {
        _finished = true;
    }
    else if (name == "PAGE") {
        skipStatement();
    }
}

void Assembler::parseInstruction(Pass pass, std::string_view, const Instruction& instruction)
{
    using enum OperandKind;
    switch (instruction.kind) {
        case CONTROL:
            emit(pass, instruction.opcode);
            break;
        case IO:
            emit(pass, static_cast<uint8_t>(instruction.opcode + parsePort(pass)));
            break;
        case REGISTER:
            emit(pass, static_cast<uint8_t>(instruction.opcode + parseRegister(pass)));
            break;
        case IMMEDIATE:
            emit(pass, instruction.opcode);
            emit(pass, valueByte(parseExpression(pass), "immediate"));
            break;
        case BRANCH:
            emit(pass, instruction.opcode);
            emit(pass, static_cast<uint8_t>(valueAddress(parseExpression(pass), "branch address") & 0xFF));
            break;
        case LONG_BRANCH: {
            emit(pass, instruction.opcode);
            const auto address = valueAddress(parseExpression(pass), "long branch address");
            emit(pass, static_cast<uint8_t>(address >> 8));
            emit(pass, static_cast<uint8_t>(address & 0xFF));
            break;
        }
    }

    if (match(TokenType::COMMA)) {
        auto bytes = parseDataValue(pass);
        for (auto byte : bytes)
            emit(pass, byte);
    }
}

void Assembler::parseData(Pass pass)
{
    expect(TokenType::COMMA, "','");
    parseDataList(pass);
}

void Assembler::parseDataList(Pass pass)
{
    if (atStatementEnd())
        return;
    auto bytes = parseDataValue(pass);
    for (auto byte : bytes)
        emit(pass, byte);
    while (match(TokenType::COMMA)) {
        bytes = parseDataValue(pass);
        for (auto byte : bytes)
            emit(pass, byte);
    }
}

std::vector<uint8_t> Assembler::parseDataValue(Pass pass)
{
    if (check(TokenType::STRING)) {
        auto text = advance().text;
        return {text.begin(), text.end()};
    }

    const auto value = parseExpression(pass);
    if (!value.known)
        return {0};
    if (value.number > 0xFF)
        return {static_cast<uint8_t>(value.number >> 8), static_cast<uint8_t>(value.number & 0xFF)};
    return {static_cast<uint8_t>(value.number & 0xFF)};
}

Assembler::Value Assembler::parseExpression(Pass pass)
{
    auto value = parsePrimary(pass);
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        const auto op = previous().type;
        const auto rhs = parsePrimary(pass);
        value.known = value.known && rhs.known;
        if (value.known) {
            if (op == TokenType::PLUS)
                value.number += rhs.number;
            else
                value.number -= rhs.number;
        }
    }
    return value;
}

Assembler::Value Assembler::parsePrimary(Pass pass)
{
    if (match(TokenType::NUMBER))
        return {previous().value, true};
    if (match(TokenType::STAR))
        return {_statementAddress, true};
    if (checkIdentifier("A") && (peek(1).type == TokenType::LPAREN || peek(1).type == TokenType::DOT))
        return parseAddressFunction(pass);
    if (match(TokenType::IDENTIFIER)) {
        const auto iter = _symbols.find(previous().text);
        if (iter != _symbols.end())
            return {iter->second, true};
        if (pass == Pass::SYMBOLS)
            return {0, false};
        fail(previous(), fmt::format("Undefined symbol '{}'", previous().text));
        return {};
    }
    fail(peek(), "Expected expression");
    return {};
}

Assembler::Value Assembler::parseAddressFunction(Pass pass)
{
    advance();
    int selector = -1;
    if (match(TokenType::DOT)) {
        if (!match(TokenType::NUMBER) || (previous().value != 0 && previous().value != 1)) {
            fail(previous(), "Expected A.0(...) or A.1(...)");
            return {};
        }
        selector = previous().value;
    }
    expect(TokenType::LPAREN, "'('");
    auto value = parseExpression(pass);
    expect(TokenType::RPAREN, "')'");
    if (!value.known)
        return value;
    if (selector == 0)
        value.number &= 0xFF;
    else if (selector == 1)
        value.number = (value.number >> 8) & 0xFF;
    return value;
}

uint8_t Assembler::valueByte(Value value, std::string_view what)
{
    if (!value.known)
        return 0;
    if (value.number < 0 || value.number > 0xFF)
        fail(previous(), fmt::format("{} out of byte range", what));
    return static_cast<uint8_t>(value.number & 0xFF);
}

uint16_t Assembler::valueAddress(Value value, std::string_view what)
{
    if (!value.known)
        return 0;
    if (value.number < 0 || value.number > 0xFFFF)
        fail(previous(), fmt::format("{} out of address range", what));
    return static_cast<uint16_t>(value.number & 0xFFFF);
}

uint8_t Assembler::parseRegister(Pass pass)
{
    const auto value = parseExpression(pass);
    if (!value.known)
        return 0;
    if (value.number < 0 || value.number > 0x0F)
        fail(previous(), "Register out of range");
    return static_cast<uint8_t>(value.number & 0x0F);
}

uint8_t Assembler::parsePort(Pass pass)
{
    const auto value = parseExpression(pass);
    if (!value.known)
        return 0;
    if (value.number < 1 || value.number > 15)
        fail(previous(), "I/O port out of range");
    return static_cast<uint8_t>(value.number & 0x07);
}

void Assembler::emit(Pass pass, uint8_t byte)
{
    if (pass == Pass::EMIT) {
        if (_segments.empty())
            setOrigin(pass, _pc);
        _segments.back().bytes.push_back(byte);
        const auto offset = static_cast<size_t>(_pc - _startAddress);
        if (_pc >= _startAddress) {
            if (_contiguousData.size() < offset)
                _contiguousData.resize(offset, 0);
            if (_contiguousData.size() == offset)
                _contiguousData.push_back(byte);
            else
                _contiguousData[offset] = byte;
        }
        _listingAddressByLine.try_emplace(_statementLine, _statementAddress);
        _listingBytesByLine[_statementLine].push_back(byte);
        _listingEndAddressByLine[_statementLine] = static_cast<uint16_t>(_pc + 1);
    }
    ++_pc;
}

void Assembler::setOrigin(Pass pass, uint16_t address)
{
    _pc = address;
    if (!_haveStartAddress) {
        _startAddress = address;
        _haveStartAddress = true;
    }
    if (pass == Pass::EMIT) {
        _listingEndAddressByLine[_statementLine] = address;
        if (_haveStartAddress && address >= _startAddress) {
            const auto offset = static_cast<size_t>(address - _startAddress);
            if (_contiguousData.size() < offset)
                _contiguousData.resize(offset, 0);
        }
        _segments.push_back(AssemblySegment{address, {}});
    }
}

void Assembler::buildListing()
{
    if (_options.listingMode == ListingMode::TRADITIONAL)
        buildTraditionalListing();
    else if (_options.listingMode == ListingMode::MODERN)
        buildModernListing();
}

std::string Assembler::byteTextForLine(int lineNumber, bool spaced) const
{
    std::string byteText;
    const auto bytesIter = _listingBytesByLine.find(lineNumber);
    if (bytesIter == _listingBytesByLine.end())
        return byteText;
    for (auto byte : bytesIter->second) {
        if (spaced && !byteText.empty())
            byteText += ' ';
        byteText += fmt::format("{:02X}", byte);
    }
    return byteText;
}

void Assembler::buildTraditionalListing()
{
    constexpr size_t PREFIX_WIDTH = 20;
    constexpr size_t DATA_WIDTH = PREFIX_WIDTH - 6;
    _listing.clear();
    _listing.reserve(std::max<size_t>(_source.size() * 2, 32));

    const auto lines = sourceLines();
    uint16_t runningAddress = 0;
    for (size_t index = 0; index < lines.size(); ++index) {
        const auto lineNumber = static_cast<int>(index + 1);
        const auto addrIter = _listingAddressByLine.find(lineNumber);
        auto byteText = byteTextForLine(lineNumber, false);
        uint16_t address = addrIter != _listingAddressByLine.end() ? addrIter->second : runningAddress;

        const auto firstBytes = byteText.substr(0, DATA_WIDTH);
        auto prefix = fmt::format("{:04X} {};", address, firstBytes);
        if (prefix.size() < PREFIX_WIDTH)
            prefix.append(PREFIX_WIDTH - prefix.size(), ' ');
        _listing += fmt::format("{}{:04d}   {}\r\n", prefix, lineNumber, lines[index]);

        size_t byteOffset = DATA_WIDTH;
        uint16_t continuationAddress = static_cast<uint16_t>(address + DATA_WIDTH / 2);
        while (byteOffset < byteText.size()) {
            const auto bytes = byteText.substr(byteOffset, DATA_WIDTH);
            prefix = fmt::format("{:04X} {};", continuationAddress, bytes);
            if (prefix.size() < PREFIX_WIDTH)
                prefix.append(PREFIX_WIDTH - prefix.size(), ' ');
            _listing += prefix + "\r\n";
            byteOffset += DATA_WIDTH;
            continuationAddress = static_cast<uint16_t>(continuationAddress + bytes.size() / 2);
        }

        const auto endIter = _listingEndAddressByLine.find(lineNumber);
        if (endIter != _listingEndAddressByLine.end())
            runningAddress = endIter->second;
    }
    _listing += "0000\r\n";
}

void Assembler::buildModernListing()
{
    _listing.clear();
    _listing.reserve(std::max<size_t>(_source.size() * 2, 32));
    _listing += "Line  Addr  Bytes                           Source\n";

    const auto lines = sourceLines();
    uint16_t runningAddress = 0;
    for (size_t index = 0; index < lines.size(); ++index) {
        const auto lineNumber = static_cast<int>(index + 1);
        const auto addrIter = _listingAddressByLine.find(lineNumber);
        const auto address = addrIter != _listingAddressByLine.end() ? addrIter->second : runningAddress;
        auto byteText = byteTextForLine(lineNumber, true);
        _listing += fmt::format("{:04d}  {:04X}  {:<30} {}\n", lineNumber, address, byteText, lines[index]);
        const auto endIter = _listingEndAddressByLine.find(lineNumber);
        if (endIter != _listingEndAddressByLine.end())
            runningAddress = endIter->second;
    }
}

std::vector<std::string_view> Assembler::sourceLines() const
{
    std::vector<std::string_view> lines;
    size_t start = 0;
    while (start < _source.size()) {
        auto end = _source.find('\n', start);
        if (end == std::string_view::npos) {
            lines.push_back(_source.substr(start));
            return lines;
        }
        size_t lineEnd = end;
        if (lineEnd > start && _source[lineEnd - 1] == '\r')
            --lineEnd;
        lines.push_back(_source.substr(start, lineEnd - start));
        start = end + 1;
    }
    if (_source.empty())
        lines.push_back({});
    return lines;
}

void Assembler::skipStatement()
{
    while (!atStatementEnd() && !check(TokenType::END_OF_FILE))
        advance();
}

void Assembler::skipLine()
{
    while (!check(TokenType::END_OF_LINE) && !check(TokenType::END_OF_FILE))
        advance();
}

void Assembler::consumeSeparators()
{
    while (match(TokenType::SEMICOLON) || match(TokenType::END_OF_LINE)) {
    }
}

bool Assembler::atStatementEnd() const
{
    return check(TokenType::SEMICOLON) || check(TokenType::END_OF_LINE) || check(TokenType::END_OF_FILE);
}

bool Assembler::match(TokenType type)
{
    if (!check(type))
        return false;
    advance();
    return true;
}

bool Assembler::matchIdentifier(std::string_view text)
{
    if (!checkIdentifier(text))
        return false;
    advance();
    return true;
}

bool Assembler::check(TokenType type) const
{
    return peek().type == type;
}

bool Assembler::checkIdentifier(std::string_view text) const
{
    return peek().type == TokenType::IDENTIFIER && peek().text == text;
}

const Token& Assembler::peek(size_t offset) const
{
    const auto index = std::min(_current + offset, _tokens.size() - 1);
    return _tokens[index];
}

const Token& Assembler::previous() const
{
    return _tokens[_current - 1];
}

const Token& Assembler::advance()
{
    if (!check(TokenType::END_OF_FILE))
        ++_current;
    return previous();
}

void Assembler::expect(TokenType type, std::string_view what)
{
    if (match(type))
        return;
    fail(peek(), fmt::format("Expected {}", what));
}

void Assembler::fail(const Token& token, std::string message)
{
    if (!_errorMessage.empty())
        return;
    _errorMessage = std::move(message);
    _errorLine = token.line;
    _errorColumn = token.column;
}

const std::unordered_map<std::string_view, Assembler::Instruction>& Assembler::instructions()
{
    static const auto map = makeInstructionMap();
    return map;
}

bool Assembler::isInstruction(std::string_view text)
{
    return instructions().count(text) != 0;
}

bool Assembler::isDirective(std::string_view text)
{
    return text == "ORG" || text == "DC" || text == "DB" || text == "END" || text == "PAGE";
}

} // namespace cdp1802
