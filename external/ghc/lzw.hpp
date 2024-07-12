//---------------------------------------------------------------------------------------
// ghc::lzw - A GIF compatible LZW implementation for C++17
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
#include <ostream>
#include <optional>
#include <vector>

#include <ghc/span.hpp>

namespace ghc::compression {

using Code = uint16_t;
using ByteArray = std::vector<uint8_t>;
using ByteView = ghc::span<const uint8_t>;

namespace detail {

struct LzwDict
{
    struct Node
    {
        std::optional<Code> _prefix{};
        uint8_t _c{};
        std::optional<Code> _left{}, _right{};
        Node(uint8_t c)
            : _c(c)
        {
        }
        Node(std::optional<Code> prefix, uint8_t c)
            : _prefix(prefix)
            , _c(c)
        {
        }
    };
    constexpr static size_t MAX_ENTRIES{4096};
    std::vector<Node> _table;
    std::vector<uint8_t> _buffer;
    uint8_t _minSize;
    LzwDict(uint8_t minSize)
        : _minSize(minSize)
        , _buffer(MAX_ENTRIES, 0)
    {
        reset();
    }
    void reset()
    {
        _table.clear();
        _table.reserve(MAX_ENTRIES);
        for (size_t i = 0; i < (1u << _minSize); ++i) {
            pushNode({static_cast<uint8_t>(i)});
        }
        pushNode({0});
        pushNode({0});
    }
    void pushNode(Node node)
    {
        if (_table.size() <= MAX_ENTRIES)
            _table.push_back(node);
        else
            std::cout << "Oops!" << std::endl;
    }
    Code clearCode() const { return 1 << _minSize; }
    Code endCode() const { return clearCode() + 1; }
    std::optional<Code> searchAndInsert(std::optional<Code> i, uint8_t c)
    {
        if (i) {
            size_t idx = *i;
            Code tableSize = _table.size();
            if (auto j = _table[idx]._prefix; j) {
                while (true) {
                    auto& entry = _table[*j];
                    if (c < entry._c) {
                        if (auto& k = entry._left; k) {
                            j = k;
                        }
                        else {
                            entry._left = tableSize;
                            break;
                        }
                    }
                    else if (c > entry._c) {
                        if (auto& k = entry._right; k) {
                            j = k;
                        }
                        else {
                            entry._right = tableSize;
                            break;
                        }
                    }
                    else {
                        return j;
                    }
                }
            }
            else {
                _table[idx]._prefix = tableSize;
            }
            pushNode({c});
            return {};
        }
        else {
            return searchSymbol(c);
        }
    }
    ByteView resequence(std::optional<uint8_t> first, std::optional<Code> prev, std::optional<Code> code)
    {
        auto outPos = _buffer.end();
        uint16_t current = *code;
        if (current == nextCode()) {
            if(!first)
                return {};
            *--outPos = *first, current = *prev;
        }
        while (current > clearCode() && outPos > _buffer.begin()) {
            *--outPos = _table[current]._c, current = *_table[current]._prefix;
        }
        if (outPos == _buffer.begin()) {
            // ERROR sequence larger than MAX_ENTRIES
            return {};
        }
        *--outPos = _table[current]._c;
        return ByteView{outPos, _buffer.end()};
    }

    Code nextCode() const { return _table.size(); }
    Code searchSymbol(Code c) const { return _table[c]._c; }
};

}

template <class OutputIter>
class BitWriter
{
public:
    explicit BitWriter(OutputIter& output)
        : _output(output)
    {
    }
    ~BitWriter() { flush(); }
    void write(Code value, size_t numBits)
    {
        assert(numBits <= 16);
        _value |= ((uint32_t)value << _size);
        _size += numBits;
        while (_size >= 8) {
            *_output++ = static_cast<uint8_t>(_value & 0xff);
            _value >>= 8;
            _size -= 8;
        }
    }
    void flush()
    {
        if (_size) {
            *_output++ = static_cast<uint8_t>(_value & 0xff);
            _size = 0;
            _value = 0;
        }
    }

private:
    OutputIter& _output;
    uint32_t _value{};
    size_t _size{};
};

template <class InputIter>
class BitReader
{
public:
    explicit BitReader(InputIter& from, InputIter to)
        : _from(from)
        , _to(to)
    {
    }
    std::optional<Code> read(size_t numBits)
    {
        assert(numBits <= 16);
        Code result{};
        if(_from == _to)
            return {};
        while (_from != _to && _size < numBits) {
            _value |= static_cast<uint32_t>(*_from++) << _size;
            _size += 8;
        }
        result = _value & ((1 << numBits) - 1);
        _value >>= numBits;
        _size -= numBits;
        return result;
    }
    void flush()
    {
        _value = 0;
        _size = 0;
    }

protected:
    InputIter& _from;
    InputIter _to;
    uint32_t _value{};
    size_t _size{};
};

template <class OutputIter>
class LzwEncoder : private BitWriter<OutputIter>
{
public:
    using BitWriter<OutputIter>::write;
    using BitWriter<OutputIter>::flush;
    LzwEncoder(OutputIter& output, int minCodeSize)
        : BitWriter<OutputIter>(output)
        , _dict(minCodeSize)
        , _minCodeSize(minCodeSize)
        , _codeSize(minCodeSize + 1)
    {
        write(_dict.clearCode(), _codeSize);
    }
    ~LzwEncoder()
    {
        if (_i) {
            write(*_i, _codeSize);
        }
        write(_dict.endCode(), _codeSize);
        flush();
    }
    void encode(ByteView bytes)
    {
        for (auto c : bytes) {
            if(c > (1u<<_minCodeSize)) {
                std::cerr << "ERROR: Data contains values larger than minCodeSize allows!" << std::endl;
                return;
            }
            auto prev = _i;
            _i = _dict.searchAndInsert(prev, c);
            if (!_i) {
                if (auto code = prev; code) {
                    write(*code, _codeSize);
                }
                _i = _dict.searchSymbol(c);
            }
            auto nextCode = _dict.nextCode();
            if (nextCode > (1u << _codeSize)) {
                ++_codeSize;
            }
            if (nextCode >= detail::LzwDict::MAX_ENTRIES) {
                _dict.reset();
                _codeSize = _minCodeSize + 1;
                write(_dict.clearCode(), _codeSize);
                flush();
            }
        }
    }

private:
    detail::LzwDict _dict;
    size_t _minCodeSize;
    size_t _codeSize;
    std::optional<Code> _i{};
};

template <class InputIter>
class LzwDecoder : private BitReader<InputIter>
{
public:
    using BitReader<InputIter>::read;
    using BitReader<InputIter>::flush;
    LzwDecoder(InputIter& from, InputIter to, uint8_t minCodeSize)
        : BitReader<InputIter>(from, to)
        , _dict(minCodeSize)
        , _minCodeSize(minCodeSize)
        , _codeSize(minCodeSize + 1)
    {
    }

    std::optional<ByteArray> decompress()
    {
        ByteArray result;
        int buffer[ghc::compression::detail::LzwDict::MAX_ENTRIES]{};
        int size = _minCodeSize + 1;
        _dict.reset();
        std::optional<uint8_t> first;
        std::optional<Code> code;
        std::optional<Code> prev;
        while ((code = read(size))) {
            if (*code > _dict.nextCode() || *code == _dict.endCode())
                break;
            if (*code == _dict.clearCode()) {
                _dict.reset();
                size = _minCodeSize + 1;
                first.reset();
                prev.reset();
            }
            else if (!prev) {
                result.push_back(*code);
                prev = first = *code;
            }
            else {
                auto sequence = _dict.resequence(first, prev, code);
                if(sequence.empty())
                    return {};
                result.insert(result.end(), sequence.begin(), sequence.end());
                first = sequence.front();
                if (_dict.nextCode() < ghc::compression::detail::LzwDict::MAX_ENTRIES) {
                    _dict._table.push_back(detail::LzwDict::Node{prev, *first});
                    if ((_dict.nextCode() & ((1 << size) - 1)) == 0 && _dict.nextCode() < ghc::compression::detail::LzwDict::MAX_ENTRIES)
                        size++;
                }
                prev = code;
            }
        }
        return result;
    }

private:
    ByteView _data;
    detail::LzwDict _dict;
    size_t _minCodeSize;
    size_t _codeSize;
};

}
