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

#if !defined(USE_GHC_SPAN) && __cplusplus >= 202002L && defined __has_include
#  if __has_include (<span>)
#define USE_STD_SPAN
#    include <span>
namespace ghc {
    using std::span;
    using std::dynamic_extend;
}
#  endif
#endif

#ifndef USE_STD_SPAN
#include <ghc/span.hpp>
#endif

namespace ghc {

using Code = uint16_t;
using ByteArray = std::vector<uint8_t>;
using ByteView = ghc::span<const uint8_t>;

#define INVALID_CODE 0xffff

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
    ByteView reconstruct(std::optional<Code> code)
    {
        auto outPos = _buffer.end();
        uint8_t symbol;
        if (code) {
            auto k = *code;
            if (k < _table.size()) {
                symbol = _table[k]._c;
                code = _table[k]._prefix;
                *--outPos = symbol;
            }
            else {
                // ERROR: Invalid code
                return {};
            }
        }
        while (code) {
            auto k = *code;
            if (outPos <= _buffer.begin()) {
                // ERROR: Inconsistent code table
                return {};
            }
            else {
                auto& node = _table[k];
                code = node._prefix;
                symbol = node._c;
                *--outPos = symbol;
            }
        }
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

class BitReader
{
public:
    explicit BitReader(ByteView input)
        : _input(input)
        , _iter(input.begin())
    {
    }
    std::optional<Code> read(size_t numBits)
    {
        assert(numBits <= 16);
        Code result{};
        if(_iter == _input.end())
            return {};
        while (_iter < _input.end() && _size < numBits) {
            _value |= static_cast<uint32_t>(*_iter++) << _size;
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

private:
    ByteView _input;
    ByteView::iterator _iter;
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

class LzwDecoder : private BitReader
{
public:
    LzwDecoder(ByteView data, int minCodeSize)
        : BitReader(data)
        , _dict(minCodeSize)
        , _minCodeSize(minCodeSize)
        , _codeSize(minCodeSize + 1)
    {
    }
    ByteArray decode()
    {
        ByteArray result;
        std::optional<Code> code;
        std::optional<Code> prev;
        while((code = read(_codeSize))) {
            if(*code == _dict.clearCode()) {
                _dict.reset();
                _codeSize = _minCodeSize + 1;
                flush();
            }
            else if(*code == _dict.endCode()) {
                return result;
            }
            else {
                auto nextCode = _dict.nextCode();
                if(*code > nextCode) {
                    // ERROR: Invalid code
                    return result;
                }
                if(prev) {
                    if(*code == nextCode) {
                        auto r = _dict.reconstruct(prev);
                        if(!r.empty()) {
                            auto c = r[0];
                            _dict.pushNode({prev, c});
                            auto r2 = _dict.reconstruct(code);
                            result.insert(result.end(), r2.begin(), r2.end());
                        }
                    }
                    else {
                        auto r = _dict.reconstruct(code);
                        if(!r.empty()) {
                            auto c = r[0];
                            _dict.pushNode({prev, c});
                            result.insert(result.end(), r.begin(), r.end());
                        }
                    }
                    if((nextCode & ((1u<<_codeSize) - 1u)) == 0 && (1u << _codeSize) < ghc::detail::LzwDict::MAX_ENTRIES) {
                        ++_codeSize;
                    }
                }
                else {
                    result.push_back(*code);
                }
                prev = code;
            }
        }
        return result;
    }
private:
    detail::LzwDict _dict;
    size_t _minCodeSize;
    size_t _codeSize;
};

}
