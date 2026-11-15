//---------------------------------------------------------------------------------------
// src/cdp1802assembler.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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
// CDP1802 assembler interface for Chiplet and its users
//---------------------------------------------------------------------------------------
#include <chiplet/cdp1802assembler.hpp>
#include <chiplet/utility.hpp>

#include "cdp1802/assembler.hpp"

namespace emu {

Cdp1802Assembler::Cdp1802Assembler() = default;
Cdp1802Assembler::~Cdp1802Assembler() = default;

void Cdp1802Assembler::reset()
{
    _compileResult.reset();
    _segments.clear();
    _startAddress = 0;
    _code.clear();
    _sha1 = {};
    _listingText.clear();
}

const CompileResult& Cdp1802Assembler::fail(const std::string& filename, std::string message, int line, int column)
{
    _compileResult.resultType = CompileResult::eERROR;
    _compileResult.errorMessage = std::move(message);
    _compileResult.locations.push_back({filename, line, column, CompileResult::Location::eROOT});
    return _compileResult;
}

const CompileResult& Cdp1802Assembler::compile(const fs::path& filename)
{
    std::error_code ec;
    if (!fs::is_regular_file(filename, ec)) {
        reset();
        return fail(filename.string(), "Couldn't read file '" + filename.string() + "'", 0, 0);
    }
    return compile(filename, loadTextFile(filename));
}

const CompileResult& Cdp1802Assembler::compile(const fs::path& filename, std::string_view source)
{
    reset();
    cdp1802::Assembler assembler(source, {.listingMode = _listing ? cdp1802::ListingMode::TRADITIONAL : cdp1802::ListingMode::NONE});
    if (!assembler.compile() || assembler.isError())
        return fail(filename.string(), assembler.errorMessage(), assembler.errorLine(), assembler.errorColumn());
    // the source lines of the bytes are in the order they were assembled, the order of the segments
    const auto& byteLines = assembler.byteSourceLines();
    size_t lineIndex = 0;
    for (const auto& segment : assembler.segments()) {
        Segment result{segment.address, segment.bytes, {}};
        result.lines.reserve(segment.bytes.size());
        for (size_t i = 0; i < segment.bytes.size(); ++i, ++lineIndex)
            result.lines.push_back(lineIndex < byteLines.size() ? static_cast<uint32_t>(byteLines[lineIndex]) : 0);
        if (!result.bytes.empty())
            _segments.push_back(std::move(result));
    }
    _startAddress = assembler.startAddress();
    _code = assembler.contiguousData();
    Sha1 sum;
    sum.add(_code.data(), _code.size());
    sum.finalize();
    _sha1 = Sha1::Digest(sum);
    _listingText = assembler.listing();
    _compileResult.errorMessage = "No errors.";
    return _compileResult;
}

std::optional<uint32_t> Cdp1802Assembler::lineForAddr(uint32_t address) const
{
    // a later segment over the same address wins, like when it is loaded
    for (auto segment = _segments.rbegin(); segment != _segments.rend(); ++segment) {
        if (address >= segment->address && address - segment->address < segment->bytes.size()) {
            const auto line = segment->lines[address - segment->address];
            return line ? std::optional<uint32_t>(line) : std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<std::pair<uint32_t, uint32_t>> Cdp1802Assembler::addrForLine(uint32_t line) const
{
    std::optional<std::pair<uint32_t, uint32_t>> range;
    for (const auto& segment : _segments) {
        for (size_t i = 0; i < segment.lines.size(); ++i) {
            if (segment.lines[i] != line)
                continue;
            const auto address = segment.address + static_cast<uint32_t>(i);
            if (!range)
                range = std::make_pair(address, address);
            else
                range = std::make_pair(std::min(range->first, address), std::max(range->second, address));
        }
    }
    return range;
}

}  // namespace emu
