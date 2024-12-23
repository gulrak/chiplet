//---------------------------------------------------------------------------------------
// memcunks.cpp
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

#include <chiplet/chunkedmemory.hpp>

namespace emu {

ChunkedMemory::ChunkedMemory(std::span<const uint8_t> memView, uint32_t offset)
{
    // Use try_emplace to construct the Chunk in-place within the map
    _chunks.try_emplace(offset,                           // Key for the map
                        this,
                        offset,                           // Chunk's offset
                        memView.data(),                   // Chunk's start pointer
                        memView.data() + memView.size(),  // Chunk's end pointer
                        eNONE                             // Chunk's usageType
    );
}

ChunkedMemory::Chunk* ChunkedMemory::chunkAt(uint32_t address)
{
    auto it = _chunks.upper_bound(address);
    if (it == _chunks.begin()) {
        return nullptr;
    }
    --it;
    Chunk& chunk = it->second;
    if (address < chunk.startAddr() || address >= chunk.endAddr()) {
        return nullptr;
    }
    return &chunk;
}

std::optional<ChunkedMemory::Chunk> ChunkedMemory::chunkWithAddress(uint32_t address)
{
    auto* chunk = chunkAt(address);
    if (chunk) {
        return *chunk;
    }
    return {};
}

void ChunkedMemory::setChunkType(uint32_t offset, UsageType usageType)
{
    if (auto* chunk = chunkAt(offset)) {
        chunk->_usageType = usageType;
    }
}

std::pair<ChunkedMemory::Chunk, ChunkedMemory::Chunk> ChunkedMemory::splitChunkAt(Chunk chunk, uint32_t address)
{
    assert(address > chunk.startAddr() && address < chunk.endAddr());
    uint32_t splitOffset = address - chunk._offset;
    const uint8_t* splitPtr = chunk._start + splitOffset;
    auto originalUsageType = chunk._usageType;
    _chunks.erase(chunk._offset);
    auto [iter1, inserted1] = _chunks.try_emplace(chunk._offset, this, chunk._offset, chunk._start, splitPtr, originalUsageType);
    auto [iter2, inserted2] = _chunks.try_emplace(address, this, address, splitPtr, chunk._end, originalUsageType);
    assert(inserted1 && inserted2);
    return {iter1->second, iter2->second};
}

std::pair<ChunkedMemory::Chunk, ChunkedMemory::Chunk> ChunkedMemory::splitChunkAt(Chunk chunk, uint32_t address, uint32_t size)
{
    auto [firstChunk, secondChunk] = splitChunkAt(chunk, address);
    uint32_t splitAddress = address + size;
    if (splitAddress >= secondChunk.endAddr()) {
        return {firstChunk, secondChunk};
    }
    auto [limitedSecondChunk, suffixChunk] = splitChunkAt(secondChunk, splitAddress);
    return {firstChunk, limitedSecondChunk};
}

void ChunkedMemory::mergeChunks()
{
    if (_chunks.empty()) {
        return;
    }
    auto currentIt = _chunks.begin();
    while (currentIt != _chunks.end()) {
        Chunk& currentChunk = currentIt->second;
        auto nextIt = std::next(currentIt);
        while (nextIt != _chunks.end()) {
            Chunk& nextChunk = nextIt->second;
            if (currentChunk.endAddr() != nextChunk.startAddr()) {
                break;
            }
            const bool currentIsCode = (currentChunk._usageType & eEXECUTABLE) != eNONE;
            const bool nextIsCode = (nextChunk._usageType & eEXECUTABLE) != eNONE;
            if (currentIsCode != nextIsCode) {
                break;
            }
            currentChunk._end = nextChunk._end;
            currentChunk._usageType |= nextChunk._usageType;
            nextIt = _chunks.erase(nextIt);
        }
        currentIt = nextIt;
    }
}
void ChunkedMemory::dumpChunks(std::ostream& os)
{
    os << "------------" << std::endl;
    os << "    Chunks:";
    for (auto [offset, chunk] : _chunks) {
        os << fmt::format("   [0x{:04X}, 0x{:04X}, {})", offset, offset + chunk.size(), static_cast<int>(chunk._usageType));
    }
    os << std::endl;
}

}
