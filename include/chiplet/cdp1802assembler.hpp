//---------------------------------------------------------------------------------------
// chiplet/cdp1802assembler.hpp
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
#pragma once

#include <chiplet/compileresult.hpp>
#include <chiplet/sha1.hpp>
#include <ghc/fs_fwd.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = ghc::filesystem;

namespace emu {

// Assembles CDP1802 sources in the RCA COSMAC Level I syntax (see "CDP1802 Assembler Quick Reference.md"), the same
// assembler `chiplet --arch cdp1802` and the `:asm cdp1802` blocks of Octo use. Its interface follows OctoCompiler: the
// result of compile() tells about errors with their location (lines and columns are 1 based), a program is its segments
// (one per ORG, with the source line of every byte) and the contiguous code from the first ORG.
class Cdp1802Assembler
{
public:
    struct Segment
    {
        uint16_t address{0};
        std::vector<uint8_t> bytes;
        std::vector<uint32_t> lines;  // the source line of each byte
    };

    Cdp1802Assembler();
    ~Cdp1802Assembler();
    Cdp1802Assembler(const Cdp1802Assembler&) = delete;
    Cdp1802Assembler& operator=(const Cdp1802Assembler&) = delete;

    void reset();
    // Also create a traditional RCA style listing with the next compile()
    void setListing(bool listing) { _listing = listing; }

    // The file name is used for the error location, the source is the text to assemble; the second form reads the file
    const CompileResult& compile(const fs::path& filename, std::string_view source);
    const CompileResult& compile(const fs::path& filename);
    const CompileResult& compileResult() const { return _compileResult; }
    bool isError() const { return _compileResult.resultType != CompileResult::eOK; }

    // The program, empty after an error
    const std::vector<Segment>& segments() const { return _segments; }
    uint16_t startAddress() const { return _startAddress; }  // the address of the first ORG, where code() starts
    // The bytes from the start address to the last one assembled, gaps between segments are 0, bytes ORGed below the
    // start address are only in the segments
    const uint8_t* code() const { return _code.data(); }
    uint32_t codeSize() const { return static_cast<uint32_t>(_code.size()); }
    const Sha1::Digest& sha1() const { return _sha1; }  // of code()
    const std::string& listing() const { return _listingText; }

    // The source line of the byte at the address, and the first and last address of the bytes of a line (the line of
    // their statement), none for addresses or lines without code
    std::optional<uint32_t> lineForAddr(uint32_t address) const;
    std::optional<std::pair<uint32_t, uint32_t>> addrForLine(uint32_t line) const;

private:
    const CompileResult& fail(const std::string& filename, std::string message, int line, int column);

    bool _listing{false};
    CompileResult _compileResult;
    std::vector<Segment> _segments;
    uint16_t _startAddress{0};
    std::vector<uint8_t> _code;
    Sha1::Digest _sha1;
    std::string _listingText;
};

}  // namespace emu
