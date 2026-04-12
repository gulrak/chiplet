//---------------------------------------------------------------------------------------
// src/emulation/octocompiler.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2015, Steffen Schümann <s.schuemann@pobox.com>
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

#include <ghc/fs_impl.hpp>
#include <ghc/bit.hpp>

#include <chiplet/octocompiler.hpp>
#include <chiplet/chip8compiler.hpp>
#include <chiplet/chip8meta.hpp>

#include <fmt/format.h>

//#define STB_IMAGE_IMPLEMENTATION
#include <nothings/stb_image.h>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <memory>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "chiplet/imagehelper.hpp"
#include "chiplet/utility.hpp"
#include "chiplet/wavfile.hpp"

namespace {

constexpr int hexval(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

std::optional<img::Color> parseHexColor(std::string_view s) {
    if (!s.empty() && s.back() == ',') {
        s.remove_suffix(1);
    }
    if (s.empty()) return std::nullopt;
    img::Color out{};
    if (s.size() == 3) {
        int r = hexval(s[0]);
        int g = hexval(s[1]);
        int b = hexval(s[2]);
        if (r < 0 || g < 0 || b < 0) return std::nullopt;
        out.r = static_cast<uint8_t>(r * 17);
        out.g = static_cast<uint8_t>(g * 17);
        out.b = static_cast<uint8_t>(b * 17);
        return out;
    }
    if (s.size() == 6) {
        int r1 = hexval(s[0]), r2 = hexval(s[1]);
        int g1 = hexval(s[2]), g2 = hexval(s[3]);
        int b1 = hexval(s[4]), b2 = hexval(s[5]);
        if ((r1 | r2 | g1 | g2 | b1 | b2) < 0) return std::nullopt;
        out.r = static_cast<uint8_t>((r1 << 4) | r2);
        out.g = static_cast<uint8_t>((g1 << 4) | g2);
        out.b = static_cast<uint8_t>((b1 << 4) | b2);
        return out;
    }
    return std::nullopt;
}

std::optional<std::pair<int, int>> parseDimension(std::string_view s)
{
    auto pos = s.find('x');
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    std::pair<int, int> result;
    auto [ptr1, ec1] = std::from_chars(s.data(), s.data() + pos, result.first);
    if (ec1 != std::errc()) {
        return std::nullopt;
    }
    auto [ptr2, ec2] = std::from_chars(s.data() + pos + 1, s.data() + s.size(), result.second);
    if (ec2 != std::errc()) {
        return std::nullopt;
    }
    return result;
}

inline std::string loadTextFile(const std::string& file)
{
    std::ifstream is(file, std::ios::binary | std::ios::ate);
    std::streamsize size = is.tellg();
    is.seekg(0, std::ios::beg);

    std::string result(size, '\0');
    if (is.read(result.data(), size)) {
        return result;
    }

    return {};
}

}

namespace emu {

template<class... Ts> struct visitor : Ts... { using Ts::operator()...;  };
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;

static std::unordered_set<std::string> _preprocessor = {
    ":include", ":segment", ":if", ":else", ":end", ":unless", ":dump-options", ":config", ":asm"
};

static std::unordered_set<std::string> _directives = {
    ":", ":alias", ":assert", ":breakpoint", ":byte", ":calc", ":call", ":const", ":macro", ":monitor", ":next", ":org", ":pointer", ":pointer16", ":pointer24", ":proto", ":stringmode", ":unpack"
};

static std::unordered_set<std::string> _reserved = {
    "!=", "&=", "+=", "-=", "-key", ":=", ";", "<", "<<=", "<=", "=-", "==", ">", ">=", ">>=", "^=", "|=",
    "again", "audio", "bcd", "begin", "bighex", "buzzer", "clear", "delay", "else", "end", "hex", "hires", "if",
    "jump", "jump0", "key", "load", "loadflags", "loop", "lores", "native", "pitch", "plane", "random", "return",
    "save", "saveflags", "scroll-down", "scroll-left", "scroll-right", "scroll-up", "sprite", "then", "while"
};

static std::multimap<std::string_view, const OpcodeInfo*> _assemblerLookupTable;
std::unordered_map<std::string_view, OctoCompiler::OpcodeList> OctoCompiler::_operators;
std::unordered_map<std::string_view, OctoCompiler::OpcodeList> OctoCompiler::_mnemonics;

void OctoCompiler::initializeTables()
{
    if(_assemblerLookupTable.empty()) {
        for (const auto& info : detail::opcodes) {
            auto tokens = ::split(info.octo, ' ');
            if (!startsWith(info.octo, "vX") && !startsWith(info.octo, "i ") && !startsWith(info.octo, "0x")) {
                auto keywordSize = info.octo.find(' ');
                if(keywordSize == std::string::npos)
                    keywordSize = info.octo.size();
                auto keyword = info.octo.substr(0, keywordSize);
                _reserved.insert(keyword);
                _assemblerLookupTable.emplace(std::string_view{info.octo.data(), keywordSize}, &info);
                _mnemonics[std::string_view{info.octo.data(), keywordSize}].emplace_back(tokens, &info);
            }
            else if(startsWith(info.octo, "vX") && !startsWith(info.octo, "i ")) {
                _operators[std::string_view{info.octo.data() + tokens[0].size() + 1, tokens[1].size()}].emplace_back(tokens, &info);
            }
        }
    }
}

OctoCompiler::OctoCompiler(Mode mode)
    : _mode(mode)
{
    initializeTables();
}

OctoCompiler::~OctoCompiler() = default;

const CompileResult& OctoCompiler::compile(const fs::path& filename)
{
    std::vector<std::string> files;
    files.push_back(filename.string());
    return compile(files);
}

struct FilePos {
    std::string file;
    int depth{0};
    int line{0};
};

static FilePos extractFilePos(std::string_view info)
{
    int depth{0};
    int line{0};
    const auto* start = info.data() + 7; // 7 für '#@line['
    auto result = std::from_chars(start, info.data() + info.size(), depth);
    if(result.ptr == start || *result.ptr != ',')
        return {};
    start = result.ptr + 1;
    result = std::from_chars(start, info.data() + info.size(), line);
    if(result.ptr == start || *result.ptr != ',')
        return {};
    return {std::string(result.ptr + 1, (info.data() + info.size()) - result.ptr - 1), depth, line};
}

const CompileResult& OctoCompiler::compile(const fs::path& filename, std::span<const char> source, bool needsPreprocess)
{
    std::string preprocessed;
    auto* start = source.data();
    auto* end = source.data() + source.size() + 1;
    if(needsPreprocess) {
        preprocessFile(filename.string(), start, end);
        if(_compileResult.resultType != CompileResult::eOK)
            return _compileResult;
        std::ostringstream preprocessedStream;
        dumpSegments(preprocessedStream);
        preprocessed = preprocessedStream.str();
        start = preprocessed.data();
        end = preprocessed.data() + preprocessed.size();
    }
    return doCompileOcto(filename.string(), start, end);
}

const CompileResult& OctoCompiler::compile(const std::vector<std::string>& files)
{
    for(const auto& file : files) {
        preprocessFile(file);
        if(_compileResult.resultType != CompileResult::eOK)
            return _compileResult;
    }
    std::string preprocessed;
    {
        std::ostringstream preprocessedStream;
        dumpSegments(preprocessedStream);
        preprocessed = preprocessedStream.str();
    }
    return compile(fs::absolute(files.front()).string(), preprocessed, false);
}

const CompileResult& OctoCompiler::doCompileChiplet(const std::string& filename, const char* source, const char* end)
{
    try {
        _symbols.clear();
        auto lex = Lexer{};
        lex.setRange(filename, source, end);
        try {
            while (true) {
                auto token = lex.nextToken();
                if (token == Token::eEOF) {
                    break;
                }
                if (token == Token::ePREPROCESSOR) {
                    error("Preprocessor directive found in compilation stage!");
                }
                else if (token == Token::eDIRECTIVE) {
                    if (lex.expect(":alias")) {

                    }
                    else if (lex.expect(":const")) {
                        auto nameToken = lex.nextToken();
                        if (nameToken != Token::eIDENTIFIER || (lex.mode() == Lexer::eCHIP8 && nameToken != Token::eSTRING))
                            error("Identifier expected after ':const'.");
                        auto constName = lex.token().raw;
                        auto value = lex.nextToken();
                        if (value != Token::eIDENTIFIER && value != Token::eNUMBER)
                            error("Number or identifier expected after ':const <name>'.");
                        if (value == Token::eNUMBER) {
                            define(std::string(constName), lex.token().number);
                        }
                        else {
                            auto val = definedValue(lex.token().raw);
                            if(val) {
                                define(std::string(constName), *val);
                            }
                            else {
                                error("Expected a constant");
                            }
                        }
                    }
                }
                else if(isRegister(lex.token())) {
                    // register operation
                    const OpcodeInfo* opInfo = nullptr;
                    auto oper = lex.nextToken();
                    if(oper == Token::eOPERATOR) {
                        auto iter = _operators.find(lex.token().raw);
                        if(iter != _operators.end()) {
                            auto rhs = lex.nextToken();
                            if(!isRegister(lex.token())) {
                                // reg op reg
                                for(const auto& h : iter->second) {
                                    if(h.first.size() > 2 && h.first[2] == lex.token().raw) {
                                        opInfo = h.second;
                                        rhs = lex.nextToken();
                                        break;
                                    }
                                }
                                error("Expected right operand in opcode expression!");
                            }
                            if(isRegister(lex.token())) {
                                // generate code
                            }
                            else {
                                error("");
                            }
                        }
                    }
                }
                else if(auto range = _assemblerLookupTable.equal_range(lex.token().raw); range.first != _assemblerLookupTable.end()) {
                    for(auto iter = range.first; iter != range.second; ++iter) {
                        const auto& info = *iter->second;

                    }
                }
                else {
                    error("Unexpected token: " + std::string(lex.token().raw));
                }
            }
        }
        catch(Lexer::Exception& le) {
            synthesizeError({filename, (int)lex.token().line, (int)lex.token().column}, source, end, le.errorMessage);
        }
    }
    catch(std::exception& ex)
    {
        return _compileResult;
    }
    return _compileResult;
}

const CompileResult& OctoCompiler::doCompileOcto(const std::string& filename, const char* source, const char* end)
{
    std::string_view sourceCode = {source, size_t(end - source)};
    _compiler = std::make_unique<Chip8Compiler>();
    if(_progress) _progress(1, "compiling ...");
    _compiler->compile({source, static_cast<size_t>(end-source)}, _startAddress);
    if(_compiler->isError()) {
        return synthesizeError({filename, _compiler->errorLine(), _compiler->errorCol()}, source, end, _compiler->rawErrorMessage());
    }
    else {
        if(_progress) _progress(1, fmt::format("generated {} bytes of output", codeSize()));
    }
    _compileResult.reset();
    return _compileResult;
}

const CompileResult& OctoCompiler::synthesizeError(const SourceLocation& location, const char* source, const char* end, const std::string& errorMessage)
{
    if(_generateLineInfos) {
        std::stack<FilePos> filePosStack;
        FilePos ep;
        int line = 1;
        int fileLine = 1;
        for(auto iter = source; iter != end && line != location.line; ++iter) {
            if(*iter == '\n') {
                line++;
                fileLine++;
            }
            if(end - iter > 10 && *(iter + 1) == '#' && *(iter + 2) == '@') {
                auto iter2 = iter + 1;
                while(iter2 != end && *iter2 != '\n' && *iter2 != ']')
                    ++iter2;
                if(*iter2 == ']') {
                    ep = extractFilePos({iter+1, size_t(iter2 - iter - 1)});
                    if(!filePosStack.empty())
                        filePosStack.top().line = fileLine;
                    if(ep.line) {
                        while(!filePosStack.empty() && filePosStack.top().depth > ep.depth)
                            filePosStack.pop();
                        if(filePosStack.empty() || filePosStack.top().depth < ep.depth) {
                            filePosStack.push(ep);
                        }
                        else {
                            filePosStack.top() = ep;
                        }
                        fileLine = ep.line - 1;
                    }
                }
            }
        }
        if(!ep.file.empty()) {
            int i = 0;
            while(!filePosStack.empty()) {
                _compileResult.locations.push_back({
                    filePosStack.top().file,
                    i ? filePosStack.top().line : fileLine,
                    i ? 0 : location.column,
                    i ? CompileResult::Location::eINCLUDED : CompileResult::Location::eROOT
                });
                filePosStack.pop();
                ++i;
            }
            _compileResult.errorMessage = errorMessage;
            _compileResult.resultType = CompileResult::eERROR;
            return _compileResult;
        }
    }
    _compileResult.resultType = CompileResult::eERROR;
    _compileResult.errorMessage = errorMessage;
    _compileResult.locations = {{location.file, location.line, location.column, CompileResult::Location::eROOT}};
    return _compileResult;
}

const CompileResult& OctoCompiler::preprocessFiles(const std::vector<std::string>& files)
{
    for(const auto& file : files) {
        preprocessFile(file);
        if(_compileResult.resultType != CompileResult::eOK)
            break;
    }
    return _compileResult;
}

void OctoCompiler::setIncludePaths(const std::vector<std::string>& paths)
{
    _includePaths.clear();
    for(const auto& path : paths)
        _includePaths.push_back(path);
}

uint32_t OctoCompiler::codeSize() const
{
    return _compiler ? _compiler->codeSize() : 0;
}

const uint8_t* OctoCompiler::code() const
{
    return _compiler ? _compiler->code() : nullptr;
}

const Sha1::Digest& OctoCompiler::sha1() const
{
    static constexpr Sha1::Digest dummy;
    return _compiler ? _compiler->sha1() : dummy;
}
std::pair<uint32_t, uint32_t> OctoCompiler::addrForLine(uint32_t line) const
{
    return _compiler ? _compiler->addrForLine(line) : std::make_pair(0xFFFFFFFFu, 0xFFFFFFFFu);;
}

uint32_t OctoCompiler::lineForAddr(uint32_t addr) const
{
    return _compiler ? _compiler->lineForAddr(addr) : 0xFFFFFFFF;
}

std::string_view OctoCompiler::breakpointForAddr(uint32_t addr) const
{
    return _compiler ? _compiler->breakpointForAddr(addr) : "";
}

void OctoCompiler::Lexer::setRange(const std::string& filename, const char* source, const char* end)
{
    _filename = filename;
    _srcPtr = source;
    _srcEnd = end;
    _token.line = 1;
    _token.column = 1;
}

bool OctoCompiler::Lexer::isPreprocessor() const
{
    if(peek() == ':') {
        auto src = _srcPtr + 1;
        while(src < _srcEnd && std::isalpha(*src))
            ++src;
        if(_preprocessor.count(std::string(_srcPtr, src - _srcPtr))) {
            return true;
        }
    }
    return false;
}

void OctoCompiler::Lexer::skipWhitespace(bool preproc)
{
    auto start = _srcPtr;
    _token.prefixLine = _token.line;
    while (_srcPtr < _srcEnd && std::isspace(peek()) || peek() == '#')  {
        char c = get();
        _token.column += c == 9 ? _tabSize : 1;
        if(c == '#') {
            while (c && c != '\n')
                c = get();
        }
        if(c == '\n') {
            ++_token.line;
            _token.column = 1;
            if(preproc) {
                start = _srcPtr;
                _token.prefixLine = _token.line;
                preproc = false;
            }
        }
    }
    _token.prefix = {start, size_t(_srcPtr - start)};
}

OctoCompiler::Token::Type OctoCompiler::Lexer::nextToken(bool preproc)
{
    _token.column += _token.raw.size();
    skipWhitespace(preproc);

    if (peek() == '"') {
        return parseString();
    }
    else {
        const char* start = _srcPtr;
        while (peek() && !std::isspace(peek())) get();
        uint32_t len = _srcPtr - start;
        if (!peek() && !len)
            return _token.type = Token::eEOF;

        char* end{};
        _token.text = std::string(start, _srcPtr);
        _token.raw = {start, size_t(_srcPtr - start)};
        _token.number = std::strtod(start, &end);
        if (end && end != start && end != _srcPtr) {
            if (*start == '0' && len > 2) {
                if (*(start + 1) == 'x')
                    _token.number = (double)std::strtol(start+2, &end, 16);
                if (*(start + 1) == 'b')
                    _token.number = (double)std::strtol(start+2, &end, 2);
            }
            else if (*start == '-' && len > 3 && *(start + 1) == '0') {
                if (*(start + 2) == 'x')
                    _token.number = -(double)std::strtol(start+3, &end, 16);
                if (*(start + 2) == 'b')
                    _token.number = -(double)std::strtol(start+3, &end, 2);
            }
            /*else if((_token.number == 8 || _token.number == 16) && *end == 'x') {
                return _token.type = Token::eSPRITESIZE;
            }*/
        }
        else if(_mode == eRCA && *start == '#')
            _token.number = (double)std::strtol(start+1, &end, 16);
        else if(_mode == eMOTOROLA && *start == '$')
            _token.number = (double)std::strtol(start+1, &end, 16);
        if (end == _srcPtr)
            return _token.type = Token::eNUMBER;
        if (!preproc && std::isdigit(*start))
            error(fmt::format("The number could not be parsed: {}", _token.raw));
        if(*start == ':') {
            if (_directives.count(_token.text))
                return _token.type = Token::eDIRECTIVE;
            else if (_preprocessor.count(_token.text)) {
                // remove whitespace before preproc from prefix
                while(!_token.prefix.empty() && (_token.prefix.back() == ' ' || _token.prefix.back() == '\t'))
                       _token.prefix.remove_suffix(1);
                return _token.type = Token::ePREPROCESSOR;
            }
            else if (len > 1 && *(start + 1) != '=')
                error(fmt::format("Unknown directive: {}", _token.raw));
        }
        if(*start == '{')
            return _token.type = Token::eLCURLY;
        if(*start == '}')
            return _token.type = Token::eRCURLY;
        if (*start == '[')
            return _token.type = Token::eLSQUARE;
        if (*start == ']')
            return _token.type = Token::eRSQUARE;
        if(std::strchr("+-*/%@|<>^!.=:", *start))
            return _token.type = Token::eOPERATOR;
        if(_reserved.count(_token.text)) {
            _token.type = len > 1 && std::isalpha(*(start + 1)) ? Token::eKEYWORD : Token::eOPERATOR;
            return _token.type;
        }
        for(int i = 0; i < len; ++i) {
            if (!std::isalnum((uint8_t) * (start + i)) && *(start + i) != '-' && *(start + i) != '_') {
                _token.text = _token.raw;
                if(_mode == eCHIP8)
                    return _token.type = Token::eSTRING;
                error(fmt::format("Invalid token: {}", _token.raw));
            }
        }
        return _token.type = Token::eIDENTIFIER;
    }
}

void OctoCompiler::Lexer::consumeRestOfLine()
{
    // remove whitespace and comments at end of preproc
    while(_srcPtr != _srcEnd && (*_srcPtr == ' ' || *_srcPtr == '\t'))
        _srcPtr++;
    if(_srcPtr != _srcEnd && *_srcPtr == '#') {
        while(_srcPtr != _srcEnd && *_srcPtr != '\n')
            _srcPtr++;
    }
    if(_srcPtr != _srcEnd && *_srcPtr == '\n') {
        _srcPtr++;
        _token.line++;
    }
}

OctoCompiler::Token::Type OctoCompiler::Lexer::parseString()
{
    auto start = _srcPtr;
    auto quote = *_srcPtr++;
    std::string result;
    while(_srcPtr != _srcEnd && *_srcPtr != quote) {
        if(*_srcPtr == '\\') {
            if(++_srcPtr != _srcEnd) {
                auto c = *_srcPtr;
                if(c == 10 || c == 13) {
                    _token.column += size_t(_srcPtr - start);
                    error("Unexpected end of line after escaping backslash.");
                }
                if(c == 'n') {
                    c = 10;
                }
                else if(c == 'r') {
                    c = 13;
                }
                else if(c == 't') {
                    c = 9;
                }
                result.push_back(c);
            }
            else {
                _token.column += size_t(_srcPtr - start);
                error("Unexpected end after escaping backslash.");
            }
        }
        else if(*_srcPtr == 10 || *_srcPtr == 13) {
            _token.column += size_t(_srcPtr - start);
            error("Missing a closing \" in a string literal.");
        }
        else {
            result.push_back(*_srcPtr);
        }
        ++_srcPtr;
    }
    if(_srcPtr == _srcEnd) {
        _token.length = _srcPtr - start;
        _token.column += size_t(_srcPtr - start);
        error("Missing a closing \" in a string literal.");
    }
    ++_srcPtr;
    _token.text = result;
    _token.raw = {start, size_t(_srcPtr - start)};
    return Token::eSTRING;
}

void OctoCompiler::Lexer::errorLocation(CompileResult& cr)
{
    auto* parent = _parent;
    cr.locations.clear();
    cr.locations.push_back({_filename, static_cast<int>(_token.line), static_cast<int>(_token.column), CompileResult::Location::eROOT});
    //std::string includes;
    while (parent) {
        cr.locations.push_back({parent->_filename, static_cast<int>(parent->_token.line), static_cast<int>(parent->_token.column), CompileResult::Location::eINCLUDED});
        //includes = fmt::format("{}:{}:{}: info: Included from\n", parent->_filename, parent->_token.line, parent->_token.column) + includes;
        parent = parent->_parent;
    }
    //return fmt::format("{}{}:{}:{}: ", includes, _filename, _token.line, _token.column);
}

std::vector<std::pair<int,std::string>> OctoCompiler::Lexer::locationStack() const
{
    std::vector<std::pair<int,std::string>> result;
    result.reserve(10);
    auto* parent = this;
    while (parent) {
        result.insert(result.begin(), {parent->_token.line, parent->_filename});
        parent = parent->_parent;
    }
    return result;
}

std::string OctoCompiler::Lexer::cutPrefixLines()
{
    auto lastNL = _token.prefix.find_last_of('\n');
    if(lastNL != std::string_view::npos) {
        auto result = std::string(_token.prefix.substr(0, lastNL+1));
        _token.prefix.remove_prefix(lastNL + 1);
        return result;
    }
    return {};
}

void OctoCompiler::Lexer::error(std::string msg)
{
    throw Exception(msg);
}

bool OctoCompiler::Lexer::expect(const std::string_view& literal) const
{
    return _token.raw == literal;
}

void OctoCompiler::reset()
{
    _codeSegments.clear();
    _dataSegments.clear();
    _symbols.clear();
    _collect.str("");
    _collect.clear();
    _currentSegment = eCODE;
    _compileResult.reset();
}

void OctoCompiler::error(std::string msg)
{
    if(!_lexerStack.empty())
        lexer().errorLocation(_compileResult);
    else
        _compileResult.reset();
    _compileResult.errorMessage = std::move(msg);
    _compileResult.resultType = CompileResult::eERROR;
    throw std::runtime_error("");
}

void OctoCompiler::warning(std::string msg)
{
    if(!_lexerStack.empty())
        lexer().errorLocation(_compileResult);
    else
        _compileResult.reset();
    _compileResult.errorMessage = std::move(msg);
    _compileResult.resultType = CompileResult::eWARNING;
}

void OctoCompiler::info(std::string msg)
{
    if(!_lexerStack.empty())
        lexer().errorLocation(_compileResult);
    else
        _compileResult.reset();
    _compileResult.errorMessage = std::move(msg);
    _compileResult.resultType = CompileResult::eINFO;
}

size_t OctoCompiler::numSourceLines() const
{
    return _compiler->numSourceLines();
}

const CompileResult& OctoCompiler::preprocessFile(const std::string& inputFile, const char* source, const char* end)
{
    if(end - source >= 3 && *source == (char)0xef && *(source+1) == (char)0xbb && *(source+2) == (char)0xbf)
        source += 3; // skip BOM

    try {
        _lexerStack.emplace(_lexerStack.empty() ? nullptr : &_lexerStack.top());
        std::shared_ptr<int> guard(NULL, [&](int *) { _lexerStack.pop(); });
        auto& lex = lexer();
        lex.setRange(inputFile, source, end);
        _currentSegment = eCODE;
        writeLineMarker();
        try {
            auto token = lex.nextToken();
            while (true) {
                if (token == Token::eEOF) {
                    writePrefix();
                    break;
                }
                if (token == Token::ePREPROCESSOR) {
                    writePrefix();
                    if (lex.expect(":include")) {
                        auto next = lex.nextToken();
                        if (next != Token::eSTRING)
                            error("Expected string after ':include'.");
                        auto newFile = fs::absolute(inputFile).parent_path() / lex.token().text;
                        auto extension = ::toLower(newFile.extension().string());
                        if (isImage(extension)) {
                            token = includeImage(newFile.string());
                        }
                        else if (extension == ".bin" || extension == ".ch8") {
                            token = includeBinary(newFile.string());
                        }
                        else {
                            flushSegment();
                            auto oldSeg = _currentSegment;
                            preprocessFile(newFile.string());
                            _currentSegment = oldSeg;
                            token = lex.nextToken(true);
                        }
                    }
                    else if (lex.expect(":segment")) {
                        auto next = lex.nextToken();
                        if (next != Token::eIDENTIFIER || (lex.token().raw != "data" && lex.token().raw != "code"))
                            error("Expected 'data' or 'code' after ':segment'.");
                        flushSegment();
                        _currentSegment = (lex.token().raw == "code" ? eCODE : eDATA);
                        token = lex.nextToken(true);
                        writeLineMarker();
                    }
                    else if (lex.expect(":if")) {
                        auto option = lex.nextToken();
                        if (option != Token::eIDENTIFIER)
                            error("{}: Identifier expected after ':if'.");
                        if (!_emitCode.empty() && _emitCode.top() != eACTIVE) {
                            _emitCode.push(eSKIP_ALL);
                        }
                        else {
                            _emitCode.push(isTrue(lex.token().raw) ? eACTIVE : eINACTIVE);
                        }
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":unless")) {
                        auto option = lex.nextToken();
                        if (option != Token::eIDENTIFIER)
                            error("Identifier expected after ':unless'.");
                        if (!_emitCode.empty() && _emitCode.top() != eACTIVE) {
                            _emitCode.push(eSKIP_ALL);
                        }
                        else {
                            _emitCode.push(!isTrue(lex.token().raw) ? eACTIVE : eINACTIVE);
                        }
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":else")) {
                        if (_emitCode.empty())
                            error("Use of ':else' without ':if' or ':unless'.");
                        _emitCode.top() = _emitCode.top() == eINACTIVE ? eACTIVE : eSKIP_ALL;
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":end")) {
                        if (_emitCode.empty())
                            error("Use of ':end' without ':if' or ':unless'.");
                        _emitCode.pop();
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":dump-options")) {
                        // ignored for now
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":config")) {
                        token = lex.nextToken(true);
                        if (lex.expect("{")) {
                            std::string jsonStr = "{";
                            int braceCnt = 1;
                            token = lex.nextToken(true);
                            while(braceCnt && token != Token::eEOF) {
                                if(token == Token::eLCURLY)
                                    ++braceCnt;
                                else if(token == Token::eRCURLY)
                                    --braceCnt;
                                jsonStr += " " + std::string(lex.token().raw);
                                token = lex.nextToken(true);
                            }
                            if(braceCnt)
                                error("The ':config' JSON object parameter is not closed.");
                            else {
                                try {
                                    _compileResult.config = std::make_shared<nlohmann::json>(nlohmann::json::parse(jsonStr));
                                }
                                catch(...) {
                                    error("Error parsing the JSON object parameter of ':config'.");
                                }
                            }
                        }
                    }
                }
                else if (token == Token::eDIRECTIVE && lex.expect(":const") && (_emitCode.empty() || _emitCode.top() == eACTIVE)) {
                    writePrefix();
                    write(lex.token().raw);
                    auto nameToken = lex.nextToken();
                    if (nameToken != Token::eIDENTIFIER && !(lex.mode() == Lexer::eCHIP8 || nameToken != Token::eSTRING))
                        error("Identifier expected after ':const'.");
                    auto constName = lex.token().raw;
                    writePrefix();
                    write(lex.token().raw);
                    auto value = lex.nextToken();
                    if (value != Token::eIDENTIFIER && value != Token::eNUMBER)
                        error("Number or identifier expected after ':const <name>'.");
                    writePrefix();
                    write(lex.token().raw);
                    if (value == Token::eNUMBER) {
                        define(std::string(constName), lex.token().number);
                    }
                    token = lex.nextToken();
                }
                else {
                    writePrefix();
                    write(lex.token().raw);
                    token = lex.nextToken();
                }
            }
            flushSegment();
        }
        catch(Lexer::Exception& le) {
            _compileResult.errorMessage = le.errorMessage;
            _compileResult.resultType = CompileResult::eERROR;
            lex.errorLocation(_compileResult);
            return _compileResult;
        }
    }
    catch(std::exception& ex)
    {
        return _compileResult;
    }
    return _compileResult;
}

std::string OctoCompiler::resolveFile(const fs::path& file)
{
    if(file.is_absolute()) {
        std::error_code ec;
        if(fs::exists(file, ec))
            return file.string();
    }
    if(!_lexerStack.empty() && !lexer().filename().empty()) {
        std::error_code ec;
        auto newPath = fs::absolute(lexer().filename(), ec).parent_path() / file;
        if(!ec && fs::exists(newPath, ec))
            return newPath.string();
    }
    else {
        std::error_code ec;
        if(fs::exists(file, ec))
            return file.string();
    }
    for(const auto& path : _includePaths) {
        std::error_code ec;
        if(fs::exists(path / file, ec))
            return (path / file).string();
    }
    error(fmt::format("File not found: '{}'", file.string()));
    return "";
}

const CompileResult& OctoCompiler::preprocessFile(const std::string& inputFile)
{
    try {
        auto file = resolveFile(inputFile);
        if (_progress)
            _progress(_lexerStack.size() + 1, "preprocessing '" + inputFile + "' ...");
        auto content = loadTextFile(inputFile);
        preprocessFile(inputFile, content.data(), content.data() + content.size());
    }
    catch(std::runtime_error& ex)
    {
        
    }
    return _compileResult;
}

void OctoCompiler::doWrite(const std::string_view& text, int line)
{
    auto& lex = lexer();
    if(_generateLineInfos && line >= 0 && (_collectLocationStack.empty() || _collectLocationStack.back().first != line || lex.filename() != _collectLocationStack.back().second)) {
        auto locationStack = lex.locationStack();
        locationStack.back().first = line;
        auto iterOld = _collectLocationStack.begin();
        auto iterNew = locationStack.begin();
        while(iterOld != _collectLocationStack.end() && iterNew != locationStack.end() && *iterOld == *iterNew) {
            iterOld++;
            iterNew++;
        }
        if (_emitCode.empty() || _emitCode.top() == eACTIVE) {
            auto depth = iterNew - locationStack.begin();
            _collect << "\n";
            while (iterNew != locationStack.end()) {
                _collect << fmt::format("#@line[{},{},{}]\n", ++depth, iterNew->first, iterNew->second);
                iterNew++;
            }
        }
        std::swap(_collectLocationStack, locationStack);
    }
    if(!_collectLocationStack.empty())
        _collectLocationStack.back().first += std::count(text.begin(), text.end(), '\n');
    if (_emitCode.empty() || _emitCode.top() == eACTIVE)
        _collect << text;
}

void OctoCompiler::writePrefix()
{
    if(!_lexerStack.empty() && !lexer().token().prefix.empty()) {
        doWrite(lexer().token().prefix, lexer().token().prefixLine);
    }
}

void OctoCompiler::write(const std::string_view& text)
{
    if(!text.empty()) {
        doWrite(text, lexer().token().line);
    }
}

void OctoCompiler::writeGenerated(const std::string_view& text)
{
    if(!text.empty()) {
        doWrite(text, -1); // do not generate line info for generated code as it has no source lines
    }
}

void OctoCompiler::writeLineMarker()
{
    return;
    if(_generateLineInfos) {
        auto& lex = lexer();
        _collect << lex.cutPrefixLines() << fmt::format("#@line[{},{},{}]\n", _lexerStack.size(), lex.token().line, lex.filename());
    }
}

void OctoCompiler::flushSegment()
{
    if(_currentSegment == eCODE)
        _codeSegments.push_back(_collect.str());
    else
        _dataSegments.push_back(_collect.str());
    _collect.str("");
    _collect.clear();
    _collectLocationStack.clear();
}

bool OctoCompiler::isImage(const std::string& extension)
{
    return extension == ".png" || extension == ".gif" || extension == ".bmp" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga";
}

namespace {
enum ImageFilterType { eNEAREST, eDITHER };
std::vector<uint8_t> processImage(uint8_t* data, size_t width, size_t height, ImageFilterType filter, const std::vector<img::Color>& palette, bool megaChip)
{
    switch(filter) {
        case eNEAREST:
            return img::threshold(data, width, height, palette);
        case eDITHER:
            return img::dither(data, width, height, palette);
    }
    return {};
}
}

OctoCompiler::Token::Type OctoCompiler::includeImage(std::string filename)
{
    std::vector<img::Color> palette{{0,0,0}, {255,255,255}};
    int width,height,numChannels;
    int widthHint = -1, heightHint = -1;
    bool megaChip = false;
    ImageFilterType filter = eNEAREST;
    bool genLabels = true;
    bool genPalette = false;
    bool debug = false;
    auto& lex = lexer();
    auto token = lex.nextToken(true);
    while(token != Token::eEOF) {
        if (auto size = parseDimension(lex.token().raw); size) {
            widthHint = size.value().first;
            heightHint = size.value().second;
            if (megaChip) {
                if (widthHint == 0 || widthHint > 256 || heightHint == 0 || heightHint > 256)
                    error("Invalid size for mega-chip image.");
            }
            else if (!(widthHint == 8 || widthHint == 16) || heightHint == 0 || heightHint > 64) {
                error("Invalid size for 8xN or 16x16 image.");
            }
        }
        else if (token == Token::eIDENTIFIER && lex.token().text == "no-labels") {
            genLabels = false;
        }
        else if (token == Token::eIDENTIFIER && lex.token().text == "debug") {
            debug = true;
        }
        else if(token == Token::eIDENTIFIER && lex.token().text == "dither") {
            filter = eDITHER;
        }
        else if(token == Token::eIDENTIFIER && lex.token().text == "megachip") {
            megaChip = true;
        }
        else if (token == Token::eIDENTIFIER && lex.token().text == "palette") {
            genPalette = true;
        }
        else if (token == Token::eLSQUARE) {
            std::vector<img::Color> colors;
            if (megaChip) {
                colors.reserve(256);
                colors.push_back({0,0,0,0});
            }
            auto sv = lex.token().raw;
            if (sv.length() > 1) {
                sv.remove_prefix(1);
                auto col = parseHexColor(sv);
                if (!col)
                    error(fmt::format("Bad color value for image include: '{}'", sv));
                colors.push_back(*col);
                while (true) {
                    token = lex.nextToken(true);
                    if (token == Token::eEOF)
                        break;
                    if (token == Token::eRSQUARE)
                        break;
                    if (endsWith(lex.token().raw, "]")) {
                        auto sv = lex.token().raw;
                        sv.remove_suffix(1);
                        col = parseHexColor(sv);
                        if (!col)
                            error(fmt::format("Bad color value for image include: '{}'", sv));
                        colors.push_back(*col);
                        break;
                    }
                    auto sv = lex.token().raw;
                    if (sv.length() > 1) {
                        col = parseHexColor(sv);
                        if (!col)
                            error(fmt::format("Bad color value for image include: '{}'", sv));
                        colors.push_back(*col);
                    }
                }
            }
            if (auto n = colors.size(); (megaChip && n > 2 && n < 256) || (n != 0 && (n & (n - 1)) == 0 && n >= 2 && n <= 16))
                palette = colors;
            else
                error(fmt::format("Invalid color palette for image include, must be power of two and between 2 and 16 entries"));
        }
        else {
            break;
        }
        token = lex.nextToken(true);
    }
    auto* data = stbi_load(filename.c_str(), &width, &height, &numChannels, 4);
    if(!data) {
        error(fmt::format("Could not load image: '{}'", filename));
    }
    int spriteWidth, spriteHeight;
    if(widthHint > 0) {
        spriteWidth = widthHint;
        spriteHeight = heightHint;
    }
    else if ( width == 16 && height == 16 ) {
        spriteWidth = spriteHeight = 16;
    } else {
        int numRows = 1;
        while ( height % numRows != 0 || height / numRows >= 16 )
            numRows++;
        spriteWidth = 8;
        spriteHeight = height / numRows;
    }
    auto name = fs::path(filename).filename().stem().string();
    if(width % spriteWidth != 0)
        error(fmt::format("Image needs to be divisible by {}.", spriteWidth));
    auto processed = processImage(data, width, height, filter, palette, megaChip);
    stbi_image_free(data);
    std::string debugStr;
    bool asHex = true;
    if(debug && _progress) _progress(1, fmt::format("\nSprite dimension: {}x{}", spriteWidth, spriteHeight));
    if (!megaChip) {
        auto bitPlanes = ghc::countr_zero(palette.size());
        for (int y = 0; y < height; y += spriteHeight) {
            for (int x = 0; x < width; x += spriteWidth) {
                int index = y * width + x;
                for (int plane = 0; plane < bitPlanes; ++plane) {
                    size_t count = 0;
                    if (bitPlanes == 1) {
                        if(genLabels)
                            writeGenerated(fmt::format(": {}-{}-{}", name, x/8, y/spriteHeight));
                        if(debug && _progress) _progress(1, fmt::format("{} {},{}:", name, x/8, y/spriteHeight));
                    }
                    else {
                        if(genLabels)
                            writeGenerated(fmt::format(": {}-{}-{}-{}", name, plane, x/8, y/spriteHeight));
                        if(debug && _progress) _progress(1, fmt::format("{} {},{},{}:", name, plane, x/8, y/spriteHeight));
                    }
                    for (int rows = 0; rows < spriteHeight; rows++) {
                        //writeGenerated(" ");
                        for (int cols = 0; cols < spriteWidth / 8; cols++) {
                            uint8_t val = 0;
                            for (uint8_t bit = 0x80, i = 0; bit > 0; bit >>= 1, ++i) {
                                auto pixel = processed[index + rows * width + cols * 8 + i];
                                if (pixel & (1 << plane))
                                    val |= bit;
                                if(debug && _progress) debugStr += (pixel & (1 << plane)) ? "██" : "░░";
                            }
                            if (asHex) {
                                if (count++ % 16 == 0) {
                                    writeGenerated("\n ");
                                }
                                writeGenerated(fmt::format(" 0x{:02x}", val));
                            }
                            else
                                writeGenerated(fmt::format(" 0b{:08b}", val));
                        }
                        if(debug && _progress) {
                            _progress(1, debugStr);
                            debugStr.clear();
                        }
                        if (!asHex)
                            writeGenerated("\n");
                    }
                    if (asHex)
                        writeGenerated("\n");
                }
            }
        }
    }
    else {
        // MegaChip sprites
        for (int y = 0; y < height; y += spriteHeight) {
            for (int x = 0; x < width; x += spriteWidth) {
                int index = y * width + x;
                if(genLabels)
                    writeGenerated(fmt::format(": {}-{}-{}", name, x/spriteWidth, y/spriteHeight));
                if(debug && _progress) _progress(1, fmt::format("{} {},{}:", name, x/spriteWidth, y/spriteHeight));
                size_t count = 0;
                for (int rows = 0; rows < spriteHeight; rows++) {
                    for (int cols = 0; cols < spriteWidth; cols++) {
                        auto pixel = processed[index + rows * width + cols];
                        if (count++ % 16 == 0) {
                            writeGenerated("\n ");
                        }
                        writeGenerated(fmt::format(" 0x{:02x}", pixel));
                    }
                }
                writeGenerated("\n");
            }
        }
    }
    return token;
}

OctoCompiler::Token::Type OctoCompiler::includeBinary(std::string filename)
{
    auto data = loadFile(filename);
    if(!data) {
        error(fmt::format("Could not load binary file: '{}'", to_string(data.error())));
    }
    bool genLabels = true;
    auto& lex = lexer();
    auto token = lex.nextToken(true);
    while(token != Token::eEOF) {
        if(token == Token::eIDENTIFIER && lex.token().text == "no-labels")
        {
            genLabels = false;
        }
        else {
            break;
        }
        token = lex.nextToken(true);
    }
    auto name = fs::path(filename).filename().stem().string();
    if(genLabels)
        writeGenerated(fmt::format(": {}", name));
    DataBlockFormatter formatter{[this](std::string_view sv){writeGenerated(sv);}};
    for (auto byte : *data) {
        formatter.write(fmt::format(" 0x{:02x}", byte));
    }
    return lexer().nextToken(true);
}

OctoCompiler::Token::Type OctoCompiler::includeWav(std::string filename)
{
    auto& lex = lexer();
    auto token = lex.nextToken(true);
    std::optional<uint32_t> frequencyOverride{};
    while(token != Token::eEOF) {
        if(token == Token::eNUMBER)
        {
            if (lex.token().number <= 0 || lex.token().number > 96000)
                error(fmt::format("Invalid frequency value for audio include: '{}'.", lex.token().raw));
            frequencyOverride = lex.token().number;
        }
        else {
            break;
        }
        token = lex.nextToken(true);
    }
    try {
        WavFile<uint8_t> wav(filename, frequencyOverride);
        DataBlockFormatter formatter{[this](std::string_view sv){writeGenerated(sv);}};
        const auto& samples = wav.samples();
        for (auto sample : samples) {
            formatter.write(fmt::format(" 0x{:02x}", sample));
        }
    }
    catch(std::exception& ex) {
        error(fmt::format("Couldn't read audio file '{}': {}", filename, ex.what()));
    }
    return lexer().nextToken(true);
}

static int whitespaceLinesAtEnd(const std::string& text)
{
    int count = 0;
    for(auto iter = text.rbegin(); iter != text.rend(); ++iter) {
        if(!std::isspace((uint8_t)*iter))
            break;
        if(*iter == '\n')
            ++count;
    }
    return count;
}

static int whitespaceLinesAtStart(const std::string& text)
{
    int count = 0;
    for(auto iter = text.begin(); iter != text.end(); ++iter) {
        if(!std::isspace((uint8_t)*iter))
            break;
        if(*iter == '\n')
            ++count;
    }
    return count;
}

void OctoCompiler::dumpSegments(std::ostream& output)
{
    int endingWSLines = 2;
    for(auto& segment : _codeSegments) {
        if(!segment.empty()) {
            if(!_generateLineInfos) {
                auto sepLines = endingWSLines + whitespaceLinesAtStart(segment);
                for (int i = 0; i < 2 - sepLines; ++i)
                    output << '\n';
            }
            output << segment;
            if (segment.back() != '\n')
                output << '\n';
            if(!_generateLineInfos)
                endingWSLines = whitespaceLinesAtEnd(segment);
        }
    }
    for(auto& segment : _dataSegments) {
        if(!segment.empty()) {
            if(!_generateLineInfos) {
                auto sepLines = endingWSLines + whitespaceLinesAtStart(segment);
                for (int i = 0; i < 2 - sepLines; ++i)
                    output << '\n';
            }
            output << segment;
            if(segment.back() != '\n')
                output << '\n';
            if(!_generateLineInfos)
                endingWSLines = whitespaceLinesAtEnd(segment);
        }
    }
}

void OctoCompiler::define(std::string name, Value val, SymbolType type)
{
    _symbols[name] = {eCONST, std::move(val)};
}

bool OctoCompiler::isTrue(const std::string_view& name) const
{
    auto iter = _symbols.find(name);
    if(iter != _symbols.end()) {
        return std::visit(visitor{
            [](const std::monostate&) { return false; },
            [](int val) { return val != 0; },
            [](double val) { return std::fabs(val) > 0.0000001; },
            [](const std::string& val) { return !val.empty(); }
        }, iter->second.value);
    }
    return false;
}

std::optional<double> OctoCompiler::definedValue(std::string_view name) const
{
    auto iter = _symbols.find(name);
    if(iter != _symbols.end() && (iter->second.type == eCONST || iter->second.type == eCALC || iter->second.type == eLABEL)) {
        return std::visit(visitor{
                              [](const std::monostate&) -> std::optional<double> { return {}; },
                              [](int val) -> std::optional<double> { return (double)val; },
                              [](double val) -> std::optional<double> { return val; },
                              [](const std::string& val) -> std::optional<double> { return {}; }
                          }, iter->second.value);
    }
    return {};
}

std::optional<int32_t> OctoCompiler::definedInteger(std::string_view name) const
{
    auto iter = _symbols.find(name);
    if(iter != _symbols.end() && (iter->second.type == eCONST || iter->second.type == eCALC || iter->second.type == eLABEL)) {
        return std::visit(visitor{
                              [](const std::monostate&) -> std::optional<int32_t> { return {}; },
                              [](int val) -> std::optional<int32_t> { return val; },
                              [](double val) -> std::optional<int32_t> { return (int32_t)val; },
                              [](const std::string& val) -> std::optional<int32_t> { return {}; }
                          }, iter->second.value);
    }
    return {};
}

bool OctoCompiler::isRegister(const Token& token)
{
    if(token.type != Token::eSTRING && token.type != Token::eIDENTIFIER)
        return false;
    if(token.raw.size() == 2 && (token.raw[0] == 'v' || token.raw[0] == 'V') && std::isxdigit((uint8_t)token.raw[1]))
        return true;
    if(token.raw == "i" || token.raw == "I")
        return true;
    return false;
}

} // namespace emu
