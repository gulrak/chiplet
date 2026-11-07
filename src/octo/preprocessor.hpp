//---------------------------------------------------------------------------------------
//
//  octo/preprocessor.hpp
//
//  A preprocessor for Octo CHIP-8 assembly language, suitable for embedding in other
//  tools and environments. Compared to its original form it depends heavily on C++20
//  standard library.
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
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <stack>
#include <string_view>
#include <utility>
#include <vector>

#include <ghc/fs_fwd.hpp>
#include <nlohmann/json_fwd.hpp>

#include <chiplet/octocompiler.hpp>
#include "lexer.hpp"

namespace octo {

struct PreprocessScanResult
{
    std::vector<Token> tokens;
};

class Preprocessor
{
public:
    using CompileResult = emu::CompileResult;
    using ProgressHandler = emu::OctoCompiler::ProgressHandler;
    using SymbolEntry = emu::OctoCompiler::SymbolEntry;
    using SymbolType = emu::OctoCompiler::SymbolType;
    using Value = emu::OctoCompiler::Value;

    Preprocessor() = default;

    PreprocessScanResult scan(std::string_view source) const;
    void reset();
    void generateLineInfos(bool value) { _generateLineInfos = value; }
    void setIncludePaths(const std::vector<std::string>& paths);
    void setProgressHandler(ProgressHandler handler) { _progress = std::move(handler); }
    void define(std::string name, Value val = 1, SymbolType type = emu::OctoCompiler::eCONST);
    std::optional<double> definedValue(std::string_view name) const;
    std::optional<int32_t> definedInteger(std::string_view name) const;
    const CompileResult& preprocessFile(const std::string& inputFile, const char* source, const char* end);
    const CompileResult& preprocessFile(const std::string& inputFile);
    const CompileResult& preprocessFiles(const std::vector<std::string>& files);
    void dumpSegments(std::ostream& output);
    const CompileResult& compileResult() const { return _compileResult; }

private:
    using SourceToken = octo::Token;
    using Lexer = octo::Lexer;

    enum SegmentType { CODE, DATA };
    enum OutputControl { ACTIVE, INACTIVE, SKIP_ALL };

    inline Lexer& lexer()
    {
        if(!_lexerStack.empty())
            return _lexerStack.top();
        throw Lexer::Exception("Lexer stack empty!");
    }

    bool isTrue(std::string_view name) const;
    static bool isImage(const std::string& extension);
    SourceToken::Type includeImage(std::string filename);
    SourceToken::Type includeBinary(std::string filename);
    SourceToken::Type includeWav(std::string filename);
    void write(const std::string_view& text);
    void writeGenerated(const std::string_view& text);
    void writePrefix();
    void doWrite(const std::string_view& text, int line);
    void writeLineMarker();
    void error(std::string msg);
    void warning(std::string msg);
    void info(std::string msg);
    void flushSegment();
    std::string resolveFile(const ghc::filesystem::path& file);

    std::ostringstream _collect;
    std::vector<std::pair<int,std::string>> _collectLocationStack;
    SegmentType _currentSegment{CODE};
    std::stack<Lexer> _lexerStack;
    std::vector<std::string> _codeSegments;
    std::vector<std::string> _dataSegments;
    std::stack<OutputControl> _emitCode;
    std::map<std::string, SymbolEntry, std::less<>> _symbols;
    std::vector<ghc::filesystem::path> _includePaths;
    ProgressHandler _progress;
    bool _generateLineInfos{true};
    CompileResult _compileResult;
};

}
