//---------------------------------------------------------------------------------------
// src/cli.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2020, Steffen Schümann <s.schuemann@pobox.com>
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

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <fstream>

#include <ghc/span.hpp>

class GifImage
{
public:
    using ByteArray = std::vector<uint8_t>;
    using ByteArrayView = ghc::span<const uint8_t>;
    struct Frame {
        uint16_t _left{};
        uint16_t _top{};
        uint16_t _width{};
        uint16_t _height{};
        uint16_t _delayTime{};
        std::vector<uint32_t> _palette;
        std::vector<uint8_t> _pixels;
    };

    explicit GifImage(std::string filename);
    explicit GifImage(ByteArrayView data);
    GifImage(uint16_t width, uint16_t height);

    bool isValid() const { return _isValid; }
    bool isEmpty() const { return _frames.empty(); }
    uint16_t width() const { return _width; }
    uint16_t height() const { return _height; }
    bool addFrame(ByteArrayView data, uint16_t delayTime_ms = 16);
    size_t numFrames() const { return _frames.size(); }
    const Frame getFrame(size_t index) { return _frames[index]; }
    bool writeToFile(std::string filename);


private:
    bool decode(ByteArrayView gifData);
    static ByteArray decompress(ByteArrayView dataSpan, int mcs);
    static uint16_t readU16(const uint8_t*& data)
    {
        uint16_t result = *data++;
        result |= *data++ << 8;
        return result;
    }
    std::string _filename;
    uint16_t _width{};
    uint16_t _height{};
    bool _is89a{};
    bool _hasGCT{};
    bool _isSorted{};
    uint8_t _backgroundIndex{};
    uint8_t _aspectRatio{};
    std::vector<uint32_t> _palette;
    std::vector<Frame> _frames;
    bool _isValid{false};
};

//---------------------------------------------------------------------------------------
//         Implementation
//---------------------------------------------------------------------------------------

//#ifdef GIFIMAGE_IMPLEMENTATION

GifImage::GifImage(std::string filename)
: _filename(std::move(filename))
{
    std::ifstream is(_filename, std::ios::binary | std::ios::ate);
    auto size = is.tellg();
    is.seekg(0, std::ios::beg);
    if(size > 16*1024*1024) {
        return;
    }
    std::vector<uint8_t> buffer(size);
    if (is.read((char*)buffer.data(), size)) {
        _isValid = decode(buffer);
    }
}

GifImage::GifImage(GifImage::ByteArrayView data)
{
    _isValid = decode(data);
}

GifImage::GifImage(uint16_t width, uint16_t height)
: _width(width)
, _height(height)
{
}

bool GifImage::decode(GifImage::ByteArrayView gifData)
{
    const auto* data = gifData.data();
    const uint8_t* end = data + gifData.size();
    if (std::string_view((const char*)data, 6u) == "GIF89a") {
        _is89a = true;
    }
    else if (std::string_view((const char*)data, 6u) != "GIF87a") {
        return false;
    }
    data += 6;
    _width = readU16(data);
    _height = readU16(data);
    _hasGCT = *data >> 7;
    _isSorted = (*data >> 3) & 1;
    size_t sizeCT = 1 << ((*data & 7) + 1);
    size_t colRes = 1 << (((*data++ >> 4) & 7) + 1);
    _backgroundIndex = *data++;
    _aspectRatio = *data++;
    // if(colRes != 8)
    //     return false;
    for (size_t i = 0; i < sizeCT; ++i, data += 3) {
        _palette.push_back((data[0] << 16) | (data[1] << 8) | data[2]);
    }
    while (data < end) {
        switch (*data++) {
            case 0x21:
                ++data;
                while (data < end) {
                    auto blockSize = *data++;
                    if (!blockSize)
                        break;
                    data += blockSize;
                }
                break;
            case 0x2c: {
                Frame frame{};
                frame._left = readU16(data);
                frame._top = readU16(data);
                frame._width = readU16(data);
                frame._height = readU16(data);
                auto p = *data++;
                if (p & 0x80) {
                    size_t sizeLCT = 1 << ((*data & 7) + 1);
                    for (size_t i = 0; i < sizeLCT; ++i, data += 3) {
                        frame._palette.push_back((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | 255);
                    }
                }
                auto minCode = *data++;
                const uint8_t* chunkEnd = data;
                ByteArray compressed;
                while (true) {
                    uint8_t s = *chunkEnd++;
                    if (!s)
                        break;
                    compressed.insert(compressed.end(), chunkEnd, chunkEnd + s);
                    chunkEnd += s;
                }
                auto frameData = decompress(compressed, minCode);
                frame._pixels = _frames.empty() ? ByteArray(_width * _height, 0) : _frames.back()._pixels;
                if (frame._width * frame._height <= frameData.size() && frame._left + frame._width <= _width && frame._top + frame._height <= _height) {
                    for (size_t line = frame._top; line < frame._top + frame._height && line < _height; ++line) {
                        std::memcpy(frame._pixels.data() + line * _width + frame._left, frameData.data() + (line - frame._top) * frame._width + frame._left, frame._width);
                    }
                }
                data = chunkEnd;
                _frames.push_back(frame);
                break;
            }
            case 0x3b:
                data = end;
                return true;
        }
    }
    return false;
}

GifImage::ByteArray GifImage::decompress(ByteArrayView dataSpan, int mcs)
{
    ByteArray result;
    int prefix[4096]{}, suffix[4096]{}, code[4096]{};
    int clear=1<<mcs, size=mcs+1, mask=(1<<size)-1, next=clear+2, old=-1;
    int first, i=0, b=0, d=0;
    for(int z=0;z<clear;z++)suffix[z]=z;
    while (i < dataSpan.size()) {
        while (b < size)
            d += (0xFF & dataSpan[i++]) << b, b += 8;
        int t = d & mask;
        d >>= size, b -= size;
        if (t > next || t == clear + 1)
            break;
        if (t == clear) {
            size = mcs + 1, mask = (1 << size) - 1, next = clear + 2, old = -1;
        }
        else if (old == -1)
            result.push_back(suffix[old = first = t]);
        else {
            int ci = 0, tt = t;
            if (t == next)
                code[ci++] = first, t = old;
            while (t > clear) {
                code[ci++] = suffix[t], t = prefix[t];
                if (ci >= 4096)
                    printf("overflowed code chunk!\n");
            }
            result.push_back(first = suffix[t]);
            while (ci > 0)
                result.push_back(code[--ci]);
            if (next < 4096) {
                prefix[next] = old, suffix[next++] = first;
                if ((next & mask) == 0 && next < 4096)
                    size++, mask += next;
            }
            old = tt;
        }
    }
    return result;
}

//#endif
