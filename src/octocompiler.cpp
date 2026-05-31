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

#include <chiplet/octocompiler.hpp>

#include <fmt/format.h>

#include <charconv>
#include <memory>

#include "octo/assembly_session.hpp"
#include "octo/preprocessor.hpp"

namespace emu {

OctoCompiler::OctoCompiler(Mode)
    : _preprocessor(std::make_unique<octo::Preprocessor>())
{
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

const CompileResult& OctoCompiler::doCompileOcto(const std::string& filename, const char* source, const char* end)
{
    std::string_view sourceCode = {source, size_t(end - source)};
    _assemblySession = std::make_unique<octo::AssemblySession>();
    if(_progress) _progress(1, "compiling ...");
    _assemblySession->compile({source, static_cast<size_t>(end-source)}, _startAddress);
    if(_assemblySession->isError()) {
        return synthesizeError({filename, _assemblySession->errorLine(), _assemblySession->errorCol()}, source, end, _assemblySession->rawErrorMessage());
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
    _compileResult = _preprocessor->preprocessFiles(files);
    return _compileResult;
}

void OctoCompiler::setIncludePaths(const std::vector<std::string>& paths)
{
    _preprocessor->setIncludePaths(paths);
}

void OctoCompiler::generateLineInfos(bool value)
{
    _generateLineInfos = value;
    _preprocessor->generateLineInfos(value);
}

void OctoCompiler::setProgressHandler(ProgressHandler handler)
{
    _progress = handler;
    _preprocessor->setProgressHandler(std::move(handler));
}

uint32_t OctoCompiler::codeSize() const
{
    return _assemblySession ? _assemblySession->codeSize() : 0;
}

const uint8_t* OctoCompiler::code() const
{
    return _assemblySession ? _assemblySession->code() : nullptr;
}

const Sha1::Digest& OctoCompiler::sha1() const
{
    static constexpr Sha1::Digest dummy;
    return _assemblySession ? _assemblySession->sha1() : dummy;
}
std::pair<uint32_t, uint32_t> OctoCompiler::addrForLine(uint32_t line) const
{
    return _assemblySession ? _assemblySession->addrForLine(line) : std::make_pair(0xFFFFFFFFu, 0xFFFFFFFFu);;
}

uint32_t OctoCompiler::lineForAddr(uint32_t addr) const
{
    return _assemblySession ? _assemblySession->lineForAddr(addr) : 0xFFFFFFFF;
}

std::string_view OctoCompiler::breakpointForAddr(uint32_t addr) const
{
    return _assemblySession ? _assemblySession->breakpointForAddr(addr) : "";
}

void OctoCompiler::reset()
{
    if(_preprocessor)
        _preprocessor->reset();
    _compileResult.reset();
}

size_t OctoCompiler::numSourceLines() const
{
    return _assemblySession->numSourceLines();
}

const CompileResult& OctoCompiler::preprocessFile(const std::string& inputFile, const char* source, const char* end)
{
    _compileResult = _preprocessor->preprocessFile(inputFile, source, end);
    return _compileResult;
}

const CompileResult& OctoCompiler::preprocessFile(const std::string& inputFile)
{
    _compileResult = _preprocessor->preprocessFile(inputFile);
    return _compileResult;
}

void OctoCompiler::dumpSegments(std::ostream& output)
{
    _preprocessor->dumpSegments(output);
}

void OctoCompiler::define(std::string name, Value val, SymbolType type)
{
    _preprocessor->define(name, std::move(val), type);
}

std::optional<double> OctoCompiler::definedValue(std::string_view name) const
{
    return _preprocessor->definedValue(name);
}

std::optional<int32_t> OctoCompiler::definedInteger(std::string_view name) const
{
    return _preprocessor->definedInteger(name);
}

} // namespace emu
