//---------------------------------------------------------------------------------------
// memcunks.hpp
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

#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <ostream>
#include <optional>
#include <span>
#include <type_traits>

#include <ghc/bitenum.hpp>
#include <fmt/format.h>

namespace emu {

class ChunkedMemory
{
public:
    enum UsageType : uint16_t { eNONE = 0, eJUMP = 1, eCALL = 2, eEXECUTABLE = 3, eSPRITE = 4, eLOAD = 8, eSTORE = 16, eREAD = 32, eWRITE = 64, eAUDIO = 128, e0nnn = 256 };
    struct Chunk
    {
        Chunk() = default;
        Chunk(ChunkedMemory* parent, uint32_t offset_, const uint8_t* start_, const uint8_t* end_, UsageType usageType_ = eNONE)
            : _parent(parent)
            , _offset(offset_)
            , _start(start_)
            , _end(end_)
            , _usageType(usageType_)
        {
        }
        uint32_t startAddr() const { return _offset; }
        uint32_t endAddr() const { return _offset + size(); }
        const uint8_t* startData() const { return _start; }
        const uint8_t* endData() const { return _end; }
        uint32_t size() const { return static_cast<uint32_t>(_end - _start); }
        UsageType usageType() const { return _usageType; }
        void setUsageType(UsageType type) { _usageType = type; if(_parent) _parent->setChunkType(_offset, type); }
    private:
        friend class ChunkedMemory;
        ChunkedMemory* _parent{};
        uint32_t _offset{};
        const uint8_t* _start{};
        const uint8_t* _end{};
        UsageType _usageType{};
    };

    ChunkedMemory(std::span<const uint8_t> memView, uint32_t offset);
    uint32_t offset() const { return _chunks.begin()->first; }
    uint32_t size() const { return _chunks.rbegin()->second.startAddr() + _chunks.rbegin()->second.size() - offset(); }
    const uint8_t* startData() const { return _chunks.begin()->second.startData(); }
    const uint8_t* endData() const { return _chunks.rbegin()->second.endData(); }
    std::optional<Chunk> chunkWithAddress(uint32_t address);
    void setChunkType(uint32_t offset, UsageType usageType);
    std::pair<Chunk, Chunk> splitChunkAt(Chunk chunk, uint32_t address);
    std::pair<Chunk, Chunk> splitChunkAt(Chunk chunk, uint32_t address, uint32_t size);
    void mergeChunks();
    void dumpChunks(std::ostream& os);
    size_t numChunks() const { return _chunks.size(); }
    std::map<uint32_t, Chunk>::const_iterator begin() const { return _chunks.begin(); }
    std::map<uint32_t, Chunk>::const_iterator end() const { return _chunks.end(); }

private:
    Chunk* chunkAt(uint32_t);
    std::map<uint32_t, Chunk> _chunks;
};

GHC_ENUM_ENABLE_BIT_OPERATIONS(ChunkedMemory::UsageType);

}
