//
// Created by Steffen Schümann on 17.11.24.
//
#include <doctest/doctest.h>

#include <chiplet/chunkedmemory.hpp>

#include "../../cadmium/cmake-build-emscripten/_deps/fmtlib-src/include/fmt/chrono.h"

using namespace emu;

TEST_SUITE("ChunkedMemory")
{

    TEST_CASE("ChunkedMemory splitChunkAt with two parameters")
    {
        // Create test data
        std::vector<uint8_t> memoryData(1024);  // 1 KB of memory initialized to zero
        for (size_t i = 0; i < memoryData.size(); ++i) {
            memoryData[i] = static_cast<uint8_t>(i % 256);  // Fill with sample data
        }

        // Create a span over the memory data
        std::span<const uint8_t> memView(memoryData);

        // Define a virtual address offset
        uint32_t offset = 0x1000;

        // Create a ChunkedMemory instance
        ChunkedMemory chunkedMemory(memView, offset);

        // Obtain a reference to the initial chunk
        auto originalChunk = chunkedMemory.chunkWithAddress(0x1000);

        // Verify the initial state
        REQUIRE(originalChunk);
        REQUIRE(originalChunk->startAddr() == 0x1000);
        REQUIRE(originalChunk->endAddr() == 0x1000 + 1024);
        REQUIRE(originalChunk->size() == 1024);

        // Split the chunk at address 0x1200
        uint32_t splitAddress = 0x1200;

        auto [firstChunk, secondChunk] = chunkedMemory.splitChunkAt(*originalChunk, splitAddress);

        // Verify the properties of the first chunk
        REQUIRE(firstChunk.startAddr() == 0x1000);
        REQUIRE(firstChunk.endAddr() == splitAddress);
        REQUIRE(firstChunk.size() == splitAddress - 0x1000);

        // Verify the properties of the second chunk
        REQUIRE(secondChunk.startAddr() == splitAddress);
        REQUIRE(secondChunk.endAddr() == 0x1000 + 1024);
        REQUIRE(secondChunk.size() == (0x1000 + 1024) - splitAddress);

        // Verify that the data pointers are correct
        REQUIRE(firstChunk.startData() == memView.data());
        REQUIRE(firstChunk.endData() == memView.data() + (splitAddress - 0x1000));
        REQUIRE(secondChunk.startData() == memView.data() + (splitAddress - 0x1000));
        REQUIRE(secondChunk.endData() == memView.data() + 1024);

        // Verify that the chunks are correctly inserted into the _chunks map
        REQUIRE(chunkedMemory.chunkWithAddress(0x1000)->startAddr() == 0x1000);
        REQUIRE(chunkedMemory.chunkWithAddress(splitAddress)->startAddr() == splitAddress);

        // Verify that accessing an address outside the chunks throws an exception
        REQUIRE(!chunkedMemory.chunkWithAddress(0x0FFF));
        REQUIRE(!chunkedMemory.chunkWithAddress(0x1000 + 1024));
    }

    TEST_CASE("ChunkedMemory splitChunkAt with three parameters")
    {
        // Create test data
        std::vector<uint8_t> memoryData(2048);  // 2 KB of memory initialized to zero
        for (size_t i = 0; i < memoryData.size(); ++i) {
            memoryData[i] = static_cast<uint8_t>(i % 256);  // Fill with sample data
        }

        // Create a span over the memory data
        std::span<const uint8_t> memView(memoryData);

        // Define a virtual address offset
        uint32_t offset = 0x1000;

        // Create a ChunkedMemory instance
        ChunkedMemory chunkedMemory(memView, offset);

        // Obtain a reference to the initial chunk
        auto originalChunk = chunkedMemory.chunkWithAddress(0x1000);

        // Verify the initial state
        REQUIRE(originalChunk);
        REQUIRE(originalChunk->startAddr() == 0x1000);
        REQUIRE(originalChunk->endAddr() == 0x1000 + 2048);
        REQUIRE(originalChunk->size() == 2048);

        // Split the chunk at address 0x1400 with a size limit of 512 bytes
        uint32_t splitAddress = 0x1400;
        uint32_t sizeLimit = 512;  // Limit the second chunk to 512 bytes

        auto [firstChunk, limitedSecondChunk] = chunkedMemory.splitChunkAt(*originalChunk, splitAddress, sizeLimit);

        // Verify the properties of the first chunk
        REQUIRE(firstChunk.startAddr() == 0x1000);
        REQUIRE(firstChunk.endAddr() == splitAddress);
        REQUIRE(firstChunk.size() == splitAddress - 0x1000);
        REQUIRE(firstChunk.size() == 1024);

        // Verify the properties of the limited second chunk
        REQUIRE(limitedSecondChunk.startAddr() == splitAddress);
        REQUIRE(limitedSecondChunk.endAddr() == splitAddress + sizeLimit);
        REQUIRE(limitedSecondChunk.size() == sizeLimit);
        REQUIRE(limitedSecondChunk.size() == 512);

        // Verify that a suffix chunk exists
        uint32_t suffixAddress = splitAddress + sizeLimit;
        auto suffixChunk = chunkedMemory.chunkWithAddress(suffixAddress);
        REQUIRE(suffixChunk);
        REQUIRE(suffixChunk->startAddr() == suffixAddress);
        REQUIRE(suffixChunk->endAddr() == 0x1000 + 2048);
        REQUIRE(suffixChunk->size() == (0x1000 + 2048) - suffixAddress);
        REQUIRE(suffixChunk->size() == 2048 - 1024 - 512);

        // Verify that the data pointers are correct
        REQUIRE(firstChunk.startData() == memView.data());
        REQUIRE(firstChunk.endData() == memView.data() + (splitAddress - 0x1000));
        REQUIRE(limitedSecondChunk.startData() == memView.data() + (splitAddress - 0x1000));
        REQUIRE(limitedSecondChunk.endData() == limitedSecondChunk.startData() + sizeLimit);
        REQUIRE(suffixChunk->startData() == limitedSecondChunk.endData());
        REQUIRE(suffixChunk->endData() == memView.data() + 2048);

        // Verify that the chunks are correctly inserted into the _chunks map
        REQUIRE(chunkedMemory.chunkWithAddress(0x1000)->startAddr() == 0x1000);
        REQUIRE(chunkedMemory.chunkWithAddress(splitAddress)->startAddr() == splitAddress);
        REQUIRE(chunkedMemory.chunkWithAddress(suffixAddress)->startAddr() == suffixAddress);

        // Verify that accessing an address outside the chunks throws an exception
        REQUIRE(!chunkedMemory.chunkWithAddress(0x0FFF));
        REQUIRE(!chunkedMemory.chunkWithAddress(0x1000 + 2048));
    }

    TEST_CASE("ChunkedMemory mergeChunks")
    {
        // Create test data
        std::vector<uint8_t> memoryData(4096);  // 4 KB of memory initialized to zero
        for (size_t i = 0; i < memoryData.size(); ++i) {
            memoryData[i] = static_cast<uint8_t>(i % 256);  // Fill with sample data
        }

        // Create a span over the memory data
        std::span<const uint8_t> memView(memoryData);

        // Define a virtual address offset
        uint32_t offset = 0x1000;

        // Create a ChunkedMemory instance
        ChunkedMemory chunkedMemory(memView, offset);

        // Split the initial chunk into several chunks
        auto initialChunk = chunkedMemory.chunkWithAddress(0x1000);

        // Split at 0x1400
        auto [chunk1, chunk2] = chunkedMemory.splitChunkAt(*initialChunk, 0x1400);

        // Split chunk2 at 0x1800
        auto [chunk2a, chunk3] = chunkedMemory.splitChunkAt(chunk2, 0x1800);

        // Split chunk3 at 0x1C00
        auto [chunk3a, chunk4] = chunkedMemory.splitChunkAt(chunk3, 0x1C00);

        // Set usage types
        chunkedMemory.setChunkType(0x1000, ChunkedMemory::eJUMP);
        chunkedMemory.setChunkType(0x1400, ChunkedMemory::eCALL);
        chunkedMemory.setChunkType(0x1800, ChunkedMemory::eLOAD);
        chunkedMemory.setChunkType(0x1C00, ChunkedMemory::eLOAD);

        // Verify initial state
        REQUIRE(chunkedMemory.chunkWithAddress(0x1000)->usageType() == ChunkedMemory::eJUMP);
        REQUIRE(chunkedMemory.chunkWithAddress(0x1400)->usageType() == ChunkedMemory::eCALL);
        REQUIRE(chunkedMemory.chunkWithAddress(0x1800)->usageType() == ChunkedMemory::eLOAD);
        REQUIRE(chunkedMemory.chunkWithAddress(0x1C00)->usageType() == ChunkedMemory::eLOAD);

        // Perform the merge
        chunkedMemory.mergeChunks();

        // Verify the merged chunks
        // After merging:
        // - chunk1 and chunk2a should be merged into a single code chunk
        // - chunk3a and chunk4 should be merged into a single non-code chunk

        // Check that there are only two chunks in the map
        REQUIRE(chunkedMemory.numChunks() == 2);

        // Retrieve the chunks
        auto mergedChunk1 = chunkedMemory.chunkWithAddress(chunkedMemory.offset());
        auto mergedChunk2 = chunkedMemory.chunkWithAddress(mergedChunk1->startAddr() + mergedChunk1->size());

        // Verify the first merged chunk (code chunk)
        REQUIRE(mergedChunk1->startAddr() == 0x1000);
        REQUIRE(mergedChunk1->endAddr() == 0x1800);
        REQUIRE(mergedChunk1->usageType() == ChunkedMemory::eEXECUTABLE);  // Usage type remains the same as the first chunk

        // Verify the second merged chunk (non-code chunk)
        REQUIRE(mergedChunk2->startAddr() == 0x1800);
        REQUIRE(mergedChunk2->endAddr() == 0x1000 + 4096);
        REQUIRE(mergedChunk2->usageType() == ChunkedMemory::eLOAD);  // Usage type remains the same as the first non-code chunk

        // Verify that accessing addresses within the merged chunks returns the correct usage types
        REQUIRE(chunkedMemory.chunkWithAddress(0x1000)->usageType() == ChunkedMemory::eEXECUTABLE);
        REQUIRE(chunkedMemory.chunkWithAddress(0x1400)->usageType() == ChunkedMemory::eEXECUTABLE);
        REQUIRE(chunkedMemory.chunkWithAddress(0x17FF)->usageType() == ChunkedMemory::eEXECUTABLE);

        REQUIRE(chunkedMemory.chunkWithAddress(0x1800)->usageType() == ChunkedMemory::eLOAD);
        REQUIRE(chunkedMemory.chunkWithAddress(0x1C00)->usageType() == ChunkedMemory::eLOAD);
        REQUIRE(chunkedMemory.chunkWithAddress(0x1FFF)->usageType() == ChunkedMemory::eLOAD);
        REQUIRE(chunkedMemory.chunkWithAddress(0x1000 + 4095)->usageType() == ChunkedMemory::eLOAD);

        // Verify that accessing an address outside the chunks throws an exception
        REQUIRE(!chunkedMemory.chunkWithAddress(0x0FFF));
        REQUIRE(!chunkedMemory.chunkWithAddress(0x1000 + 4096));
    }
}
