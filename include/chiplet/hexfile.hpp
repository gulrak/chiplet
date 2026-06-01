//---------------------------------------------------------------------------------------
// chiplet/hexfile.hpp
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
#pragma once

#include <algorithm>
#include <array>
#include <chiplet/sha1.hpp>
#include <cstdint>
#include <fstream>
#include <fmt/format.h>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace emu {

class HexFile
{
public:
    enum class Mode {
        READ,
        WRITE
    };

    using Range = std::pair<uint16_t, std::span<const uint8_t>>;

    explicit HexFile(const std::string& filename, Mode mode)
        : _filename{filename}
        , _mode{mode}
    {
        if (_mode == Mode::READ)
            readFile();
        else
            openForWriting();
    }

    HexFile(const HexFile&) = delete;
    HexFile& operator=(const HexFile&) = delete;

    HexFile(HexFile&& other) noexcept
        : _filename{std::move(other._filename)}
        , _mode{other._mode}
        , _stream{std::move(other._stream)}
        , _chunks{std::move(other._chunks)}
        , _ranges{std::move(other._ranges)}
        , _isClosed{other._isClosed}
    {
        other._isClosed = true;
        rebuildRanges();
    }

    HexFile& operator=(HexFile&& other)
    {
        if (this == &other)
            return *this;
        close();
        _filename = std::move(other._filename);
        _mode = other._mode;
        _stream = std::move(other._stream);
        _chunks = std::move(other._chunks);
        _ranges = std::move(other._ranges);
        _isClosed = other._isClosed;
        other._isClosed = true;
        rebuildRanges();
        return *this;
    }

    ~HexFile()
    {
        try {
            close();
        }
        catch (...) {
        }
    }

    void write(uint16_t address, std::span<const uint8_t> data)
    {
        if (_mode != Mode::WRITE)
            throw std::logic_error("HexFile is not open for writing");
        if (_isClosed)
            throw std::logic_error("HexFile is closed");
        if (data.size() > 0x10000u - address)
            throw std::out_of_range("Intel HEX write range exceeds 16-bit address space");

        size_t offset = 0;
        while (offset < data.size()) {
            const auto chunkSize = std::min<size_t>(data.size() - offset, 0xFF);
            writeRecord(address, std::span<const uint8_t>{data.data() + offset, chunkSize});
            address = static_cast<uint16_t>(address + chunkSize);
            offset += chunkSize;
        }
    }

    void close()
    {
        if (_isClosed)
            return;
        if (_mode == Mode::WRITE && _stream.is_open()) {
            writeRecord(0, {}, 0x01);
            _stream.close();
            if (!_stream)
                throw std::runtime_error("Failed closing Intel HEX file: " + _filename);
        }
        _isClosed = true;
    }

    const std::vector<Range>& ranges() const { return _ranges; }
    auto begin() const { return _ranges.begin(); }
    auto end() const { return _ranges.end(); }

    Sha1::Digest sha1Digest() const
    {
        if (_mode != Mode::READ)
            throw std::logic_error("HexFile is not open for reading");

        Sha1 sum;
        uint32_t position = _chunks.empty() ? 0 : _chunks.front().address;
        for (const auto& chunk : _chunks) {
            const auto address = static_cast<uint32_t>(chunk.address);
            while (position < address) {
                const auto fillSize = static_cast<size_t>(std::min<uint32_t>(address - position, ZERO_FILL.size()));
                sum.add(ZERO_FILL.data(), static_cast<uint32_t>(fillSize));
                position += fillSize;
            }
            sum.add(chunk.data.data(), static_cast<uint32_t>(chunk.data.size()));
            position += static_cast<uint32_t>(chunk.data.size());
        }
        sum.finalize();
        return static_cast<Sha1::Digest>(sum);
    }

private:
    struct Chunk {
        uint16_t address{};
        std::vector<uint8_t> data;
    };

    static constexpr std::array<uint8_t, 256> ZERO_FILL{};

    void normalizeChunks()
    {
        std::sort(_chunks.begin(), _chunks.end(), [](const Chunk& lhs, const Chunk& rhs) {
            return lhs.address < rhs.address;
        });

        std::vector<Chunk> merged;
        for (auto& chunk : _chunks) {
            if (chunk.data.empty())
                continue;
            if (!merged.empty()) {
                auto& last = merged.back();
                const auto lastEnd = static_cast<uint32_t>(last.address) + last.data.size();
                if (lastEnd > chunk.address)
                    throw std::runtime_error("Overlapping Intel HEX data records");
                if (lastEnd == chunk.address) {
                    last.data.insert(last.data.end(), chunk.data.begin(), chunk.data.end());
                    continue;
                }
            }
            merged.push_back(std::move(chunk));
        }
        _chunks = std::move(merged);
    }

    static int hexDigit(char ch)
    {
        if (ch >= '0' && ch <= '9')
            return ch - '0';
        if (ch >= 'A' && ch <= 'F')
            return ch - 'A' + 10;
        if (ch >= 'a' && ch <= 'f')
            return ch - 'a' + 10;
        return -1;
    }

    static uint8_t parseByte(const std::string& line, size_t pos)
    {
        if (pos + 2 > line.size())
            throw std::runtime_error("Truncated Intel HEX record");
        const auto hi = hexDigit(line[pos]);
        const auto lo = hexDigit(line[pos + 1]);
        if (hi < 0 || lo < 0)
            throw std::runtime_error("Invalid hex digit in Intel HEX record");
        return static_cast<uint8_t>((hi << 4) | lo);
    }

    static uint8_t checksum(uint8_t count, uint16_t address, uint8_t type, std::span<const uint8_t> data)
    {
        uint32_t sum = count + (address >> 8) + (address & 0xFF) + type;
        for (auto byte : data)
            sum += byte;
        return static_cast<uint8_t>((~sum + 1) & 0xFF);
    }

    void openForWriting()
    {
        _stream.open(_filename, std::ios::out | std::ios::trunc);
        if (!_stream)
            throw std::runtime_error("Unable to open Intel HEX file for writing: " + _filename);
    }

    void readFile()
    {
        std::ifstream input{_filename};
        if (!input)
            throw std::runtime_error("Unable to open Intel HEX file for reading: " + _filename);

        std::string line;
        bool foundEnd = false;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;
            readRecord(line, foundEnd);
            if (foundEnd)
                break;
        }
        if (!foundEnd)
            throw std::runtime_error("Intel HEX file is missing EOF record");
        normalizeChunks();
        rebuildRanges();
        _isClosed = true;
    }

    void readRecord(const std::string& line, bool& foundEnd)
    {
        if (line.front() != ':')
            throw std::runtime_error("Intel HEX record does not start with ':'");

        const auto count = parseByte(line, 1);
        const auto address = static_cast<uint16_t>((parseByte(line, 3) << 8) | parseByte(line, 5));
        const auto type = parseByte(line, 7);
        const auto expectedSize = 11 + static_cast<size_t>(count) * 2;
        if (line.size() != expectedSize)
            throw std::runtime_error("Intel HEX record has invalid length");

        std::vector<uint8_t> data;
        data.reserve(count);
        for (size_t index = 0; index < count; ++index)
            data.push_back(parseByte(line, 9 + index * 2));

        const auto readChecksum = parseByte(line, 9 + static_cast<size_t>(count) * 2);
        if (readChecksum != checksum(count, address, type, data))
            throw std::runtime_error("Intel HEX checksum mismatch");

        if (type == 0x00) {
            if (data.size() > 0x10000u - address)
                throw std::runtime_error("Intel HEX data record exceeds 16-bit address space");
            addChunk(address, std::move(data));
        }
        else if (type == 0x01) {
            if (count != 0)
                throw std::runtime_error("Intel HEX EOF record must not contain data");
            foundEnd = true;
        }
        else {
            throw std::runtime_error("Unsupported Intel HEX record type");
        }
    }

    void addChunk(uint16_t address, std::vector<uint8_t> data)
    {
        if (data.empty())
            return;
        if (!_chunks.empty()) {
            auto& last = _chunks.back();
            const auto lastEnd = static_cast<uint16_t>(last.address + last.data.size());
            if (lastEnd == address) {
                last.data.insert(last.data.end(), data.begin(), data.end());
                return;
            }
        }
        _chunks.push_back(Chunk{address, std::move(data)});
    }

    void rebuildRanges()
    {
        _ranges.clear();
        _ranges.reserve(_chunks.size());
        for (const auto& chunk : _chunks)
            _ranges.emplace_back(chunk.address, std::span<const uint8_t>{chunk.data.data(), chunk.data.size()});
    }

    void writeRecord(uint16_t address, std::span<const uint8_t> data, uint8_t type = 0x00)
    {
        const auto count = static_cast<uint8_t>(data.size());
        _stream << fmt::format(":{:02X}{:04X}{:02X}", count, address, type);
        for (auto byte : data)
            _stream << fmt::format("{:02X}", byte);
        _stream << fmt::format("{:02X}\n", checksum(count, address, type, data));
        if (!_stream)
            throw std::runtime_error("Failed writing Intel HEX file: " + _filename);
    }

    std::string _filename;
    Mode _mode{};
    std::ofstream _stream;
    std::vector<Chunk> _chunks;
    std::vector<Range> _ranges;
    bool _isClosed{};
};

} // namespace emu
