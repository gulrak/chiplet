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

#include <algorithm>
#include <cmath>
#include <charconv>
#include <memory>
#include <sstream>
#include <map>

#include "octo/assembler.hpp"
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
    _assembledFile = filename;
    _assembledSource.assign(source, end);
    while (!_assembledSource.empty() && _assembledSource.back() == '\0')
        _assembledSource.pop_back();
    _assemblySession = std::make_unique<octo::AssemblySession>();
    if(_progress) _progress(1, "compiling ...");
    _assemblySession->compile({source, static_cast<size_t>(end-source)}, _startAddress);
    if(_assemblySession->isError()) {
        return synthesizeError({filename, _assemblySession->errorLine(), _assemblySession->errorCol()}, source, end, _assemblySession->rawErrorMessage());
    }
    else {
        if(_progress) _progress(1, fmt::format("generated {} bytes of output", codeSize()));
    }
    auto includedFiles = std::move(_compileResult.includedFiles);
    _compileResult.reset();
    _compileResult.includedFiles = std::move(includedFiles);
    return _compileResult;
}

std::string OctoCompiler::listing() const
{
    if (!_assemblySession || isError() || !codeSize() || _assembledSource.empty())
        return {};
    // the addresses of the bytes of every line, they need not be together (after an :org e.g.)
    std::map<uint32_t, std::vector<uint32_t>> addressesOfLine;
    const auto start = static_cast<uint32_t>(_startAddress);
    const bool mainJump = _assemblySession->hasMainJump() && codeSize() >= 2;
    for (uint32_t offset = mainJump ? 2 : 0; offset < codeSize(); ++offset) {
        if (const auto line = lineForAddr(start + offset); line != 0xFFFFFFFF)
            addressesOfLine[line].push_back(start + offset);
    }
    // the bytes of a :vorg block are shown at the address they run at, each block starts a row with a note
    const auto blocks = virtualBlocks();
    auto shownAddress = [&](uint32_t address) {
        for (const auto& block : blocks) {
            if (address >= block.physicalAddress && address < block.physicalAddress + block.size)
                return address - block.physicalAddress + block.logicalAddress;
        }
        return address;
    };
    auto blockStartingAt = [&](uint32_t address) {
        return std::ranges::find_if(blocks, [address](const VirtualBlockInfo& block) { return block.size && block.physicalAddress == address; });
    };
    // the lines of the assembled text with their file and line, without the preprocessor's markers
    struct SourceLine
    {
        uint32_t index;  // in the assembled text
        std::string_view text;
        std::string file;
        int line;
    };
    std::vector<std::string_view> texts;
    for (std::string_view text = _assembledSource; !text.empty();) {
        const auto newline = text.find('\n');
        texts.push_back(text.substr(0, newline));
        text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
    }
    auto isMarker = [](std::string_view line) { return line.starts_with("#@line[") && line.ends_with(']'); };
    std::vector<SourceLine> lines;
    std::string file = _assembledFile;
    int nextLine = 1;
    for (size_t index = 0; index < texts.size(); ++index) {
        const auto text = texts[index];
        if (isMarker(text)) {
            // the preprocessor's "#@line[depth,line,file]" names the file and line of what follows
            if (const auto pos = extractFilePos(text.substr(0, text.size() - 1)); pos.line) {
                file = pos.file;
                nextLine = pos.line;
            }
            continue;
        }
        if (text.empty() && index + 1 < texts.size() && isMarker(texts[index + 1]))
            continue;  // the preprocessor's line before a marker
        lines.push_back({static_cast<uint32_t>(index), text, file, nextLine++});
    }
    // included files are named relative to the folder of the main file, like it is named
    const auto baseFolder = fs::absolute(fs::path(_assembledFile)).parent_path();
    auto shownName = [&](const std::string& name) {
        const auto path = fs::path(name);
        if (name == _assembledFile || !path.is_absolute())
            return name;
        const auto relative = path.lexically_normal().lexically_relative(baseFolder);
        return relative.empty() ? name : relative.generic_string();
    };
    std::ostringstream out;
    std::string shownFile;
    auto showFile = [&](const std::string& name) {
        if (name != shownFile) {
            out << "; " << shownName(name) << "\n";
            shownFile = name;
        }
    };
    if (!lines.empty())
        showFile(lines.front().file);
    if (mainJump) {
        // the jump to a main that isn't at the start comes first, like in memory, with the line of ': main'
        const auto mainIndex = lineForAddr(start);
        const auto mainLine = std::ranges::find(lines, mainIndex, &SourceLine::index);
        const auto bytes = fmt::format("{:02X} {:02X}", code()[0], code()[1]);
        if (mainLine == lines.end())
            out << fmt::format("{:04X}  {:23}  {:5}  (jump to main)\n", start, bytes, "");
        else if (mainLine->file == shownFile)
            out << fmt::format("{:04X}  {:23}  {:5}  (jump to main)\n", start, bytes, mainLine->line);
        else
            out << fmt::format("{:04X}  {:23}  {:5}  (jump to main in {})\n", start, bytes, mainLine->line, shownName(mainLine->file));
    }
    for (const auto& line : lines) {
        showFile(line.file);
        // rows of up to 8 bytes in a row of addresses, the first one with the line
        struct Row
        {
            uint32_t shown;     // the address of the first byte as shown
            uint32_t physical;  // and where it is stored
            std::string bytes;
        };
        std::vector<Row> rows;
        if (const auto found = addressesOfLine.find(line.index); found != addressesOfLine.end()) {
            uint32_t rowStart = 0, rowPhysical = 0, previous = 0;
            std::string bytes;
            int count = 0;
            for (const auto address : found->second) {
                const auto shown = shownAddress(address);
                if (count && (count == 8 || shown != previous + 1 || blockStartingAt(address) != blocks.end())) {
                    rows.push_back({rowStart, rowPhysical, bytes});
                    bytes.clear();
                    count = 0;
                }
                if (!count)
                    rowStart = shown, rowPhysical = address;
                bytes += fmt::format("{}{:02X}", count ? " " : "", code()[address - start]);
                previous = shown;
                ++count;
            }
            rows.push_back({rowStart, rowPhysical, bytes});
        }
        if (rows.empty())
            out << fmt::format("{:4}  {:23}  {:5}  {}\n", "", "", line.line, line.text);
        for (size_t row = 0; row < rows.size(); ++row) {
            if (const auto block = blockStartingAt(rows[row].physical); block != blocks.end())
                out << fmt::format("; :vorg {:04X}-{:04X} stored at {:04X}-{:04X}\n", block->logicalAddress, block->logicalAddress + block->size - 1,
                                   block->physicalAddress, block->physicalAddress + block->size - 1);
            if (!row)
                out << fmt::format("{:04X}  {:23}  {:5}  {}\n", rows[row].shown, rows[row].bytes, line.line, line.text);
            else
                out << fmt::format("{:04X}  {}\n", rows[row].shown, rows[row].bytes);
        }
    }
    return out.str();
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

void OctoCompiler::octopusWhitespace(bool value)
{
    _preprocessor->octopusWhitespace(value);
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

std::vector<OctoCompiler::MonitorInfo> OctoCompiler::monitors() const
{
    std::vector<MonitorInfo> result;
    if (!_assemblySession || isError())
        return result;
    _assemblySession->forEachMonitor([&](std::string_view name, const octo::Monitor& monitor) {
        MonitorInfo info{.name = std::string(name), .source = monitor.type == 0 ? MonitorInfo::eREGISTER : MonitorInfo::eMEMORY,
                         .base = static_cast<uint32_t>(monitor.base), .length = monitor.len};
        for (const auto& field : monitor.format) {
            auto kind = MonitorInfo::Field::eTEXT;
            if (field.type != octo::MonitorField::STRING) {
                switch (field.base) {
                    case octo::MonitorField::CHAR: kind = MonitorInfo::Field::eCHAR; break;
                    case octo::MonitorField::BINARY: kind = MonitorInfo::Field::eBINARY; break;
                    case octo::MonitorField::DECIMAL: kind = MonitorInfo::Field::eDECIMAL; break;
                    case octo::MonitorField::HEXADECIMAL: kind = MonitorInfo::Field::eHEX; break;
                }
            }
            info.format.push_back({.kind = kind, .text = std::string(field.text), .length = kind == MonitorInfo::Field::eTEXT ? 0 : field.length});
        }
        result.push_back(std::move(info));
    });
    return result;
}

std::vector<OctoCompiler::VirtualBlockInfo> OctoCompiler::virtualBlocks() const
{
    std::vector<VirtualBlockInfo> result;
    if (!_assemblySession || isError())
        return result;
    _assemblySession->forEachVirtualBlock([&](const octo::VirtualBlock& block) {
        result.push_back({static_cast<uint32_t>(block.physicalAddress), static_cast<uint32_t>(block.logicalAddress), static_cast<uint32_t>(block.size)});
    });
    return result;
}

std::vector<std::pair<std::string, int64_t>> OctoCompiler::symbolValues() const
{
    std::vector<std::pair<std::string, int64_t>> result;
    if (!_assemblySession || isError())
        return result;
    _assemblySession->forEachSymbol([&](std::string_view name, double value) {
        if (std::isfinite(value) && value == std::floor(value) && std::abs(value) < 9.0e15)
            result.emplace_back(std::string(name), static_cast<int64_t>(value));
    });
    std::ranges::sort(result);
    return result;
}

std::string OctoCompiler::MonitorInfo::render(const std::function<uint8_t(uint32_t index)>& read, const std::function<std::string(std::string_view text)>& text) const
{
    static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
    std::string result;
    auto appendHex = [&](uint8_t value) {
        result += HEX_DIGITS[value >> 4];
        result += HEX_DIGITS[value & 15];
    };
    if (format.empty()) {
        // Octo slices the 16 registers, so a register monitor never shows more than are left
        const auto count = source == eREGISTER ? std::min(length, 16 - static_cast<int>(std::min(base, 16u))) : length;
        if (source == eREGISTER)
            result += "0x";
        for (int i = 0; i < count; ++i) {
            if (source == eMEMORY) {
                if (i)
                    result += ' ';
                result += "0x";
            }
            appendHex(read(base + i));
        }
        return result;
    }
    uint32_t index = base;
    for (const auto& field : format) {
        switch (field.kind) {
            case Field::eTEXT:
                result += text ? text(field.text) : field.text;
                break;
            case Field::eCHAR:
                for (std::size_t i = 0; i < field.length; ++i)
                    result += static_cast<char>(read(index++) & 0x7F);
                break;
            case Field::eBINARY:
                for (std::size_t i = 0; i < field.length; ++i) {
                    const auto value = read(index++);
                    for (int bit = 7; bit >= 0; --bit)
                        result += (value >> bit) & 1 ? '1' : '0';
                }
                break;
            case Field::eDECIMAL: {
                // Octo shifts a JavaScript number, so it keeps the last four bytes as int32
                uint32_t value = 0;
                for (std::size_t i = 0; i < field.length; ++i)
                    value = (value << 8) | read(index++);
                result += std::to_string(static_cast<int32_t>(value));
                break;
            }
            case Field::eHEX:
                for (std::size_t i = 0; i < field.length; ++i)
                    appendHex(read(index++));
                break;
        }
    }
    return result;
}

void OctoCompiler::reset()
{
    if(_preprocessor)
        _preprocessor->reset();
    _compileResult.reset();
    _assembledFile.clear();
    _assembledSource.clear();
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
