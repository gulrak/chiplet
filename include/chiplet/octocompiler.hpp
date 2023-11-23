//---------------------------------------------------------------------------------------
// src/emulation/octocompiler.hpp
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
#pragma once

#include <cstring>
#include <functional>
#include <sstream>
#include <stack>
#include <string_view>
#include <map>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include <ghc/fs_fwd.hpp>
#include "chip8meta.hpp"

namespace fs = ghc::filesystem;

namespace emu {

class Chip8Compiler;

struct SourceLocation {
    std::string file;
    int line{};
    int column{};
};

struct CompileResult {
    enum ResultType { eOK, eINFO, eWARNING, eERROR };
    struct Location {
        enum Type { eROOT, eINCLUDED, eINSTANTIATED };
        std::string file;
        int line;
        int column;
        Type type;
    };
    ResultType resultType{eOK};
    std::string errorMessage;
    std::vector<Location> locations;
    void reset()
    {
        resultType = eOK;
        errorMessage.clear();
        locations.clear();
    }
};

class OctoCompiler
{
public:
    using ProgressHandler = std::function<void(int verbosity, std::string msg)>;
    using Value = std::variant<std::monostate, int, double, std::string>;
    enum SymbolType { eLABEL, eCONST, eCALC, eMACRO, eALIAS };
    struct SymbolEntry {
        SymbolType type;
        Value value;
    };
    struct Token {
        enum Type { eNONE, eNUMBER, eSTRING, eDIRECTIVE, eIDENTIFIER, eOPERATOR, eKEYWORD, ePREPROCESSOR, eSPRITESIZE, eLCURLY, eRCURLY, eEOF };
        Type type{eNONE};
        double number{};
        std::string text;
        std::string_view raw;
        std::string_view prefix;
        uint32_t prefixLine{0};
        uint32_t line{0};
        uint32_t column{0};
        uint32_t length{0};
    };
    class Lexer {
    public:
        enum Mode { eCHIP8, eCHIP8STRICT, eMOTOROLA, eRCA };
        struct Exception : public std::exception {
            explicit Exception(std::string  message) : errorMessage(std::move(message)) {}
            ~Exception() noexcept override = default;
            const char* what() const noexcept override { return errorMessage.c_str(); }
            std::string errorMessage;
        };
        Lexer() = default;
        explicit Lexer(Lexer* parent) : _parent(parent) {}
        void setRange(const std::string& filename, const char* source, const char* end);
        Token::Type nextToken(bool preproc = false);
        const Token& token() const { return _token; }
        Mode mode() const { return _mode; }
        std::string cutPrefixLines();
        void consumeRestOfLine();
        bool expect(const std::string_view& literal) const;
        void errorLocation(CompileResult& result);
        std::vector<std::pair<int,std::string>> locationStack() const;
        const std::string& filename() const { return _filename; }
    private:
        char peek() const { return _srcPtr < _srcEnd ? *_srcPtr : '\0'; }
        bool checkFor(const std::string& key) const { return _srcPtr + key.size() <= _srcEnd && std::strncmp(_srcPtr, key.data(), key.size()) == 0; }
        char get() { return _srcPtr < _srcEnd ? *_srcPtr++ : '\0'; }
        bool isPreprocessor() const;
        Token::Type parseString();
        void skipWhitespace(bool preproc = false);
        void error(std::string msg);
        Lexer* _parent{nullptr};
        std::string _filename;
        const char* _srcPtr{nullptr};
        const char* _srcEnd{nullptr};
        Token _token;
        Mode _mode{eCHIP8};
        unsigned _tabSize{1};
    };
    enum Mode { eCHIPLET, eC_OCTO };
    explicit OctoCompiler(Mode mode = eC_OCTO);
    ~OctoCompiler();
    static void initializeTables();
    void reset();
    void setStartAddress(int startAddress) { _startAddress = startAddress; }
    const CompileResult& compile(const std::string& filename, const char* source, const char* end, bool needsPreprocess = true);
    const CompileResult& compile(const std::string& filename);
    const CompileResult& compile(const std::vector<std::string>& files);
    const CompileResult& preprocessFile(const std::string& inputFile, const char* source, const char* end);
    const CompileResult& preprocessFile(const std::string& inputFile);
    const CompileResult& preprocessFiles(const std::vector<std::string>& files);
    void dumpSegments(std::ostream& output);
    void define(std::string name, Value val = 1, SymbolType type = eCONST);
    std::optional<double> definedValue(std::string_view name) const;
    std::optional<int32_t> definedInteger(std::string_view name) const;
    const CompileResult& compileResult() const { return _compileResult; }
    bool isError() const { return _compileResult.resultType != CompileResult::eOK; }
    void generateLineInfos(bool value) { _generateLineInfos = value; }
    void setIncludePaths(const std::vector<std::string>& paths);
    void setProgressHandler(ProgressHandler handler) { _progress = handler; }
    uint32_t codeSize() const;
    const uint8_t* code() const;
    const std::string& sha1Hex() const;
    std::pair<uint32_t, uint32_t> addrForLine(uint32_t line) const;
    uint32_t lineForAddr(uint32_t addr) const;
    const char* breakpointForAddr(uint32_t addr) const;

private:
    inline Lexer& lexer()
    {
        if(!_lexerStack.empty())
            return _lexerStack.top();
        throw Lexer::Exception("Lexer stack empty!");
    }
    enum SegmentType { eCODE, eDATA };
    enum OutputControl { eACTIVE, eINACTIVE, eSKIP_ALL };
    const CompileResult& doCompileChiplet(const std::string& filename, const char* source, const char* end);
    const CompileResult& doCompileCOcto(const std::string& filename, const char* source, const char* end);
    const CompileResult& synthesizeError(const SourceLocation& location, const char* source, const char* end, const std::string& errorMessage);
    bool isTrue(const std::string_view& name) const;
    static bool isImage(const std::string& filename);
    Token::Type includeImage(std::string filename);
    void write(const std::string_view& text);
    void writeGenerated(const std::string_view& text);
    void writePrefix();
    void doWrite(const std::string_view& text, int line);
    void writeLineMarker();
    void error(std::string msg);
    void warning(std::string msg);
    void info(std::string msg);
    void flushSegment();
    static bool isRegister(const Token& token) ;
    std::string resolveFile(const fs::path& file);
    Mode _mode{eC_OCTO};
    std::ostringstream _collect;
    std::vector<std::pair<int,std::string>> _collectLocationStack;
    SegmentType _currentSegment{eCODE};
    std::string _lineMarker;
    std::stack<Lexer> _lexerStack;
    std::vector<std::string> _codeSegments;
    std::vector<std::string> _dataSegments;
    std::stack<OutputControl> _emitCode;
    std::map<std::string, SymbolEntry, std::less<>> _symbols;
    std::vector<fs::path> _includePaths;
    std::unique_ptr<Chip8Compiler> _compiler;
    using OpcodePattern = std::pair<std::vector<std::string>, const OpcodeInfo*>;
    using OpcodeList = std::vector<OpcodePattern>;
    static std::unordered_map<std::string_view, OpcodeList> _operators;
    static std::unordered_map<std::string_view, OpcodeList> _mnemonics;
    ProgressHandler _progress;
    bool _generateLineInfos{true};
    int _startAddress{0x200};
    CompileResult _compileResult;
};

} // namespace emu
