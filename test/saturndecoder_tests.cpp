//---------------------------------------------------------------------------------------
// saturndecoder_tests.cpp
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
#include <doctest/doctest.h>

#include <chiplet/decoder/saturndecoder.hpp>
#include "xoschiro256p.hpp"

class SaturnDecoder : public emu::SaturnDecoder
{
public:
    static constexpr uint32_t MASK = 0x3FF;
    SaturnDecoder() : emu::SaturnDecoder()
    {
        for (const auto& table : getVarTables()) {
            uint8_t count = 0;
            for (const auto& entry : table) {
                if (!entry.empty())
                    ++count;
            }
            _numOperandValues.push_back(count);
        }
    }
    void reset() { _ram.fill(0); }
    unsigned readNibble(uint32_t address) const override { return _ram[address & MASK]; }
    uint32_t genArg(const InstructionInfo* info, unsigned index)
    {
        using namespace emu::saturn;
        if (info->numVars <= index) {
            return 0;
        }
        auto rnd = _rnd();
        if (info->varTypes[index] < vt_maxtables) {
            const auto& table = getVarTables().at(info->varTypes[index]);
            rnd = rnd % _numOperandValues[info->varTypes[index]];
            for (size_t i = 0; i < 16; ++i) {
                if (!table.at(i).empty()) {
                    if (rnd)
                        --rnd;
                    else
                        return i + info->varArgs[index];
                }
            }
            throw std::runtime_error("failed to find a valid value");
        }
        switch (info->varTypes[index]) {
            case vt_const:
                return rnd % (info->varArgs[index] * 4);
            case vt_nzconst:
                return rnd % (info->varArgs[index] * 4) + 1;
            case vt_pcofs:
                return rnd % (info->varArgs[index] * 4);
            case vt_varconst:
                throw std::runtime_error("not implemented");
            case vt_hwflags:
                return rnd & 0xF;
            default:
                throw std::runtime_error("invalid variable type");
        }
    }
    void createInstruction(uint32_t address, OpcodeId id, std::optional<unsigned> seed)
    {
        const auto* info = getInstructionInfo(id);
        auto opcodeSize = info->opcodeSize < 0 ? -info->opcodeSize : info->opcodeSize;
        if (seed)
            _rnd.reseed(*seed);

        // start with the fixed opcode nibbles
        for (size_t i = 0; i < opcodeSize; ++i)
        {
            _ram[(address + i) & MASK] = (info->opcode >> ((info->opcodeSize - 1 - i) * 4)) & 0xF;
        }
        if (info->opcodeSize < 0) {
            auto size = genArg(info, 0);
            auto rnd = _rnd();
            _ram[(address + info->varOffsets[0]) & MASK] = size & 0xF;
            if (size < 16) rnd = rnd % (size * 4);
            for (size_t i = 0; i < size; ++i) {
                _ram[(address + opcodeSize + i) & MASK] = rnd & 0xF;
                rnd >>= 4;
            }
        }
        else {
            for (size_t i = 0; i < info->numVars; ++i) {
                uint64_t arg = genArg(info, i);
                for (size_t j = 0; j < info->varSizes[i]; ++j) {
                    _ram[(address + info->varOffsets[i] + j) & MASK] = arg & 0xF;
                    arg >>= 4;
                }
            }
        }
    }
    DecodeResult decodeInstruction(uint32_t address) const { return decode(address); }
    Xoschiro256Plus _rnd;
    std::array<uint8_t,MASK+1> _ram{};
    std::vector<uint8_t> _numOperandValues;
};

TEST_CASE("<saturndecoder>")
{
    SaturnDecoder saturn;
    for (size_t i = 0; i < SaturnDecoder::numInstructions(); ++i) {
        INFO("opcode ", std::string(saturn.getInstructionInfo(i)->fmt));
        for (size_t j = 0; j < 10000; ++j) {
            saturn.reset();
            saturn.createInstruction(0x100, static_cast<SaturnDecoder::OpcodeId>(i), j);
            auto decoded = saturn.decodeInstruction(0x100);
            CHECK(decoded.oid == i);
            if (decoded.oid != i)
                break;
        }
    }
}
