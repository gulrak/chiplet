//---------------------------------------------------------------------------------------
// chiplet/octocompiler.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2024, Steffen Schümann <s.schuemann@pobox.com>
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

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include <span>

#include <chiplet/compileresult.hpp>
#include <ghc/fs_fwd.hpp>
#include "sha1.hpp"

namespace fs = ghc::filesystem;

namespace octo {
class Preprocessor;
class AssemblySession;
}

namespace emu {


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
    // A `:monitor` directive, see the Octo manual. Fields of a format are consecutive bytes
    // starting at the base, an index past the registers or memory reads as 0.
    struct MonitorInfo
    {
        enum Source { eMEMORY, eREGISTER };
        struct Field
        {
            enum Kind { eTEXT, eCHAR, eBINARY, eDECIMAL, eHEX };
            Kind kind{eTEXT};
            std::string text;     // the literal for eTEXT, the directive (like `%2i`) otherwise
            std::size_t length{0}; // bytes used by the field, 0 for eTEXT
        };
        std::string name;
        Source source{eMEMORY};
        uint32_t base{0};  // address or register index
        int length{-1};    // bytes/registers shown, -1 when the format is used
        std::vector<Field> format;
        // Renders like Octo: memory as `0x12 0x34`, registers as `0x1234`, formats with `%i` as
        // big-endian signed 32 bit, `%x`/`%b` zero padded per byte and `%c` as 7 bit ASCII.
        // `read(index)` gets base + offset and returns the memory byte or register for the source.
        // `text`, if given, formats the literal parts, so a debugger can expand `{...}` in them.
        std::string render(const std::function<uint8_t(uint32_t index)>& read, const std::function<std::string(std::string_view text)>& text = {}) const;
    };
    enum Mode { eC_OCTO };
    explicit OctoCompiler(Mode mode = eC_OCTO);
    ~OctoCompiler();
    void reset();
    bool setStartAddress(int startAddress) { if(_startAddress != startAddress) { _startAddress = startAddress; return true; } return false; }
    const CompileResult& compile(const fs::path& filename, std::span<const char> source, bool needsPreprocess = true);
    const CompileResult& compile(const fs::path& filename);
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
    size_t numSourceLines() const;
    void generateLineInfos(bool value);
    void setIncludePaths(const std::vector<std::string>& paths);
    void setProgressHandler(ProgressHandler handler);
    uint32_t codeSize() const;
    const uint8_t* code() const;
    const Sha1::Digest& sha1() const;
    std::pair<uint32_t, uint32_t> addrForLine(uint32_t line) const;
    uint32_t lineForAddr(uint32_t addr) const;
    std::string_view breakpointForAddr(uint32_t addr) const;
    std::vector<MonitorInfo> monitors() const;  // in order of definition, empty after an error
    // Labels and constants with an integer value, empty after an error
    std::vector<std::pair<std::string, int64_t>> symbolValues() const;

private:
    const CompileResult& doCompileOcto(const std::string& filename, const char* source, const char* end);
    const CompileResult& synthesizeError(const SourceLocation& location, const char* source, const char* end, const std::string& errorMessage);
    std::unique_ptr<octo::AssemblySession> _assemblySession;
    std::unique_ptr<octo::Preprocessor> _preprocessor;
    ProgressHandler _progress;
    bool _generateLineInfos{true};
    int _startAddress{0x200};
    CompileResult _compileResult;
};

} // namespace emu
