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

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <iterator>
#include <optional>
#include <memory>

#include <fstream>
#include <iostream>

#include <ghc/span.hpp>
#include <fmt/format.h>
#include <ghc/hexdump.hpp>
#include <ghc/lzw.hpp>

#ifdef GIF_DEBUG_OUTPUT
#define GIF_DEBUG_LOG(x) std::cout << (x) << std::endl
#else
#define GIF_DEBUG_LOG(x)
#endif

class GifImage
{
public:
    using ByteArray = std::vector<uint8_t>;
    using ByteView = ghc::span<const uint8_t>;

private:
    class SubblockInserter
    {
    public:
        using container_type = std::vector<uint8_t>;
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;
        explicit SubblockInserter(ByteArray& buffer, size_t maxSize = 255)
            : _impl(new Impl{maxSize, buffer, buffer.size(), 0})
        {
            _impl->_buffer.push_back(0);
        }
        ~SubblockInserter()
        {
            if(_impl.unique() && _impl->_inserted) {
                _impl->_buffer[_impl->_subBlockStart] = _impl->_inserted;
                _impl->_buffer.push_back(0);
            }
        }
        SubblockInserter& operator=(uint8_t val)
        {
            _impl->_buffer.push_back(val);
            ++_impl->_inserted;
            if(_impl->_inserted == _impl->_maxSize) {
                _impl->_buffer[_impl->_subBlockStart] = _impl->_maxSize;
                _impl->_subBlockStart = _impl->_buffer.size();
                _impl->_inserted = 0;
                _impl->_buffer.push_back(0);
            }
            return *this;
        }
        SubblockInserter& operator*() { return *this; }
        SubblockInserter& operator++() { return *this; }
        SubblockInserter operator++(int) { return *this; }

    private:
        struct Impl {
            size_t _maxSize{};
            std::vector<uint8_t>& _buffer;
            size_t _subBlockStart{};
            size_t _inserted{};
        };
        std::shared_ptr<Impl> _impl;
    };

    class SubblockReader
    {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = uint8_t;
        using difference_type = std::ptrdiff_t;
        using pointer = const uint8_t*;
        using reference = const uint8_t&;
        SubblockReader(ByteView input)
            : _buffer(input)
            , _src(input.begin())
        {
            if(!input.empty())
                _bytesLeft = _blockSize = *_src++;
        }
        reference operator*() const
        {
            return *_src;
        }
        SubblockReader& operator++()
        {
            if(_src < _buffer.end()) {
                if (_bytesLeft) {
                    --_bytesLeft;
                    _src++;
                }
                if(_blockSize && !_bytesLeft) {
                    _bytesLeft = _blockSize = *_src++;
                    if(_src == _buffer.end()) {
                        _bytesLeft = _blockSize = 0;
                    }
                }
            }
            else {
                _bytesLeft = _blockSize = 0;
            }
            return *this;
        }
        SubblockReader operator++(int)
        {
            auto temp = *this;
            ++(*this);
            return temp;
        }
        bool operator==(const SubblockReader& other) const
        {
            return _src == other._src || (!_blockSize && !other._blockSize);
        }
        bool operator!=(const SubblockReader& other) const
        {
            return !(*this == other);
        }
        size_t consumed() const
        {
            return _buffer.empty() ? 0 : _src - _buffer.begin();
        }
        SubblockReader end() const
        {
            SubblockReader sbr;
            sbr._buffer = _buffer;
            sbr._src = _buffer.end();
            sbr._bytesLeft = sbr._blockSize = 0;
            return sbr;
        }
        /*bool operator<(const SubblockReader& other) const
        {
            return _src < (other._buffer.empty() ? _buffer.end() : other._src);
        }*/
    private:
        SubblockReader() = default;
        ByteView _buffer;
        ByteView::iterator _src{};
        size_t _blockSize{};
        size_t _bytesLeft{};
    };

public:
    struct Frame {
        struct ControlExtension {
            enum DisposalMethod { unspecified, doNotDispose, restoreToBackground, restoreToPrevious };
            DisposalMethod _disposalMethod{unspecified};
            bool _userInput{false};
            bool _transparency{false};
            uint16_t _delayTime{3};
            uint8_t _transparentColor{0};
        };
        uint16_t _left{};
        uint16_t _top{};
        uint16_t _width{};
        uint16_t _height{};
        std::vector<uint32_t> _palette;
        std::vector<uint8_t> _pixels;
        bool _isInterlaced{false};
        bool _isSorted{false};
        std::optional<ControlExtension> _controlExtension;
    };

    GifImage() = default;
    explicit GifImage(ByteView data);
    GifImage(uint16_t width, uint16_t height);

    bool isValid() const { return _isValid; }
    bool isEmpty() const { return _frames.empty(); }
    uint16_t width() const { return _width; }
    uint16_t height() const { return _height; }
    bool addFrame(ByteView data, uint16_t delayTime_ms = 16);
    size_t numFrames() const { return _frames.size(); }
    const Frame& getFrame(size_t index) { return _frames[index]; }
    bool writeToFile(std::string filename);
    const std::vector<uint8_t> compressed() const { return _compressedBytes; }

    static GifImage fromFile(std::string filename);

protected:
    int minCodeSize() const { return _minCodeSize; }
    bool decodeFile(std::string filename);
    bool decode(ByteView gifData);
    bool encode(ByteArray& outputBuffer);
    static ByteArray getBlockData(const uint8_t*& data)
    {
        ByteArray result;
        while (true) {
            uint8_t blockSize = *data++;
            if (!blockSize)
                break;
            result.insert(result.end(), data, data + blockSize);
            data += blockSize;
        }
        return result;
    }
    static uint16_t readU16(const uint8_t*& data)
    {
        uint16_t result = *data++;
        result |= *data++ << 8;
        return result;
    }
    static void appendU8(ByteArray& buffer, uint8_t val)
    {
        buffer.push_back(val);
    }
    static void appendU8(ByteArray& buffer, std::initializer_list<uint8_t> vals)
    {
        buffer.insert(buffer.end(), vals.begin(), vals.end());
    }
    static void appendU16(ByteArray& buffer, uint16_t val)
    {
        buffer.push_back(val & 0xff);
        buffer.push_back(val >> 8);
    }
    static void appendU16(ByteArray& buffer, std::initializer_list<uint16_t> vals)
    {
        for(auto v : vals) appendU16(buffer, v);
    }
    static void appendTo(ByteArray& buffer, ByteView data)
    {
        buffer.insert(buffer.end(), data.begin(), data.end());
    }
    static void appendTo(ByteArray& buffer, const char* cstr)
    {
        buffer.insert(buffer.end(), cstr, cstr + std::strlen(cstr));
    }
    std::string _filename;
    uint16_t _width{};
    uint16_t _height{};
    bool _is89a{};
    bool _isSorted{false};
    uint8_t _colorResolution{};
    uint8_t _backgroundIndex{};
    uint8_t _aspectRatio{};
    std::string _comment;
    std::vector<uint32_t> _palette;
    std::vector<Frame> _frames;
    ByteArray _appExtension;
    std::vector<uint8_t> _compressedBytes;
    int _minCodeSize{};
    bool _isValid{false};
};

//---------------------------------------------------------------------------------------
//         Implementation
//---------------------------------------------------------------------------------------

//#ifdef GIFIMAGE_IMPLEMENTATION

inline GifImage GifImage::fromFile(std::string filename)
{
    GifImage result;
    result.decodeFile(filename);
    return result;
}

inline GifImage::GifImage(GifImage::ByteView data)
{
    _isValid = decode(data);
}

inline GifImage::GifImage(uint16_t width, uint16_t height)
: _width(width)
, _height(height)
{
}

inline bool GifImage::decodeFile(std::string filename)
{
    std::ifstream is(filename, std::ios::binary | std::ios::ate);
    if(is.is_open()) {
        size_t size = is.tellg();
        is.seekg(0, std::ios::beg);
        if (size > 16 * 1024 * 1024) {
            return false;
        }
        std::vector<uint8_t> buffer(size);
        if (is.read((char*)buffer.data(), size)) {
            return decode(buffer);
        }
    }
    return false;
}

inline bool GifImage::decode(GifImage::ByteView gifData)
{
    const auto* data = gifData.data();
    const uint8_t* end = data + gifData.size();
    bool success = false;
    if (std::string_view((const char*)data, 6u) == "GIF89a") {
        _is89a = true;
    }
    else if (std::string_view((const char*)data, 6u) != "GIF87a") {
        return success;
    }
    data += 6;
    _width = readU16(data);
    _height = readU16(data);
    bool hasGCT = *data >> 7;
    _isSorted = (*data >> 3) & 1;
    size_t sizeCT = 1 << ((*data & 7) + 1);
    _colorResolution = 1 << (((*data++ >> 4) & 7) + 1);
    _backgroundIndex = *data++;
    _aspectRatio = *data++;
    if(hasGCT) {
        for (size_t i = 0; i < sizeCT; ++i, data += 3) {
            _palette.push_back((data[0] << 16) | (data[1] << 8) | data[2]);
        }
    }
#ifdef GIF_DEBUG_OUTPUT
    std::cout << "---START-DECODE---" << std::endl;
    ghc::hexDump(std::cout, gifData.data(), data - gifData.data(), true);
#endif
    std::optional<Frame::ControlExtension> controlExtension;
    while (data < end) {
#ifdef GIF_DEBUG_OUTPUT
        const auto* start = data;
        std::cout << "----" << std::endl;
#endif
        switch (*data++) {
            case 0x21: {
                auto extensionType = *data++;
                auto extensionBytes = getBlockData(data);
                _is89a = true;
                controlExtension.reset();
                switch (extensionType) {
                    case 0x01:
                        GIF_DEBUG_LOG("Plain Text Extension (21 01):");
                        break;
                    case 0xf9: {
                        GIF_DEBUG_LOG("Graphics Control Extension (21 f9):");
                        if(extensionBytes.size() == 4) {
                            auto packed = extensionBytes[0];
                            controlExtension = {Frame::ControlExtension::DisposalMethod((packed >> 2) & 7), (packed & 2) != 0, (packed & 1) != 0, static_cast<uint16_t>(extensionBytes[1] | (extensionBytes[2] << 8)), extensionBytes[3]};
                        }
                        break;
                    }
                    case 0xfe: {
                        GIF_DEBUG_LOG("Comment Extension (21 fe):");
                        _comment.assign(extensionBytes.begin(), extensionBytes.end());
                        break;
                    }
                    case 0xff:
                        GIF_DEBUG_LOG("Application Extension (21 ff):");
                        _appExtension = extensionBytes;
                        break;
                    default:
                        GIF_DEBUG_LOG("Unknown extension type " + std::to_string(extensionType));
                }
                break;
            }
            case 0x2c: {
                GIF_DEBUG_LOG("Image Descriptor (2c):");
                Frame frame{};
                frame._left = readU16(data);
                frame._top = readU16(data);
                frame._width = readU16(data);
                frame._height = readU16(data);
                frame._controlExtension = controlExtension;
                auto p = *data++;
                frame._isInterlaced = (p & 0x40) != 0;
                frame._isSorted = (p & 0x20) != 0;
                if (p & 0x80) {
                    size_t sizeLCT = 1 << ((*data & 7) + 1);
                    for (size_t i = 0; i < sizeLCT; ++i, data += 3) {
                        frame._palette.push_back((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | 255);
                    }
                }
#ifdef GIF_DEBUG_OUTPUT
                ghc::hexDump(std::cout, start, data - start, true, start - gifData.data());
                start = data;
                std::cout << "Image Data:" << std::endl;
#endif
                auto minCode = *data++;
#if 1
                ByteView comp{};
                SubblockReader sbr(ByteView{data, end});
                ghc::compression::LzwDecoder lzw(sbr, sbr.end(), minCode);
                auto pixels = lzw.decompress();
                data += sbr.consumed();
#else
                const uint8_t* chunkEnd = data;

                ByteArray compressed;
                while (true) {
                    uint8_t blockSize = *chunkEnd++;
                    if (!blockSize)
                        break;
                    compressed.insert(compressed.end(), chunkEnd, chunkEnd + blockSize);
                    chunkEnd += blockSize;
                }
                ByteArray compressed2;
                SubblockReader sbrt(ByteView{data, end});
                while(sbrt != sbrt.end()) {
                    compressed2.push_back(*sbrt++);
                }
                ghc::hexDump(std::cout, compressed.data(), compressed.size());
                ghc::hexDump(std::cout, compressed2.data(), compressed2.size());
                ByteView comp{};
                auto iter = compressed.begin();
                ghc::compression::LzwDecoder lzw(iter, compressed.end(), minCode);
                auto pixels = lzw.decompress();
                data = chunkEnd;
                if(_frames.empty()) {
                    _compressedBytes = compressed;
                }
#endif
                if(pixels) {
                    frame._pixels = std::move(*pixels);
                }
                if(_frames.empty()) {
                    _minCodeSize = minCode;
                }
                /*auto frameData = decompress(compressed, minCode);
                frame._pixels = _frames.empty() ? ByteArray(_width * _height, 0) : _frames.back()._pixels;
                if (frame._width * frame._height <= frameData.size() && frame._left + frame._width <= _width && frame._top + frame._height <= _height) {
                    for (size_t line = frame._top; line < frame._top + frame._height && line < _height; ++line) {
                        std::memcpy(frame._pixels.data() + line * _width + frame._left, frameData.data() + (line - frame._top) * frame._width + frame._left, frame._width);
                    }
                }*/
                _frames.push_back(frame);
                break;
            }
            case 0x3b:
                data = end;
                success = true;
                break;
            default:
                data = end;
                break;
        }
#ifdef GIF_DEBUG_OUTPUT
        ghc::hexDump(std::cout, start, data - start, true, start - gifData.data());
#endif
    }
    GIF_DEBUG_LOG("---END-DECODE---");
    return success;
}

inline bool GifImage::encode(GifImage::ByteArray& out)
{
    appendTo(out, _is89a ? "GIF89a" : "GIF87a");
    // Logical Screen Descriptor
    appendU16(out, _width);
    appendU16(out, _height);
    int colBitsG = _palette.size() ? std::ceil(std::log(_palette.size()) / std::log(2)) : 1;
    appendU8(out, (_palette.size() ? 0x80 : 0) | ((_colorResolution - 1) << 4) | (colBitsG - 1) | (_isSorted ? 8 : 0));
    appendU8(out, _backgroundIndex);
    appendU8(out, _aspectRatio);
    // Global Color Table
    for(int i = 0; i < (1u<<colBitsG); ++i) {
        if(i < _palette.size()) {
            appendU8(out, {static_cast<uint8_t>((_palette[i] >> 16) & 0xff), static_cast<uint8_t>((_palette[i] >> 8u) & 0xffu), static_cast<uint8_t>(_palette[i] & 0xffu)});
        }
        else {
            appendU8(out, {0,0,0});
        }
    }
    if(!_appExtension.empty()) {
        // Application Extension if available
        appendU8(out, {0x21, 0xff});
        std::copy(_appExtension.begin(), _appExtension.end(), SubblockInserter(out, 0xb));
    }
    if(!_comment.empty()) {
        // Comment Extension if available
        appendU8(out, {0x21, 0xfe});
        std::copy(_comment.begin(), _comment.end(), SubblockInserter(out));
    }
    for(const auto& frame : _frames) {
        if(frame._controlExtension) {
            // Graphics Control Extension
            appendU8(out, {0x21, 0xf9, 4, static_cast<uint8_t>((frame._controlExtension->_disposalMethod)<<2 | (frame._controlExtension->_userInput?2:0) | (frame._controlExtension->_transparency?1:0))});
            appendU16(out, frame._controlExtension->_delayTime);  // 1s/100
            appendU8(out, {frame._controlExtension->_transparentColor, 0});
        }
        // Image Descriptor
        appendU8(out, 0x2C);
        appendU16(out, {frame._left, frame._top, frame._width, frame._height});
        int colBitsL = 0;
        if(frame._palette.empty()) {
            appendU8(out, (frame._isInterlaced ? 0x40 : 0) | (frame._isSorted ? 0x20 : 0));
        }
        else {
            colBitsL = std::ceil(std::log(frame._palette.size()) / std::log(2));
            appendU8(out, (frame._palette.size() ? 0x80 : 0) | (colBitsL - 1) | (frame._isInterlaced ? 0x40 : 0) | (frame._isSorted ? 0x20 : 0));
            // Local Color Table
            for (int i = 0; i < (1u << colBitsL); ++i) {
                if (i < frame._palette.size()) {
                    appendU8(out, {static_cast<uint8_t>((frame._palette[i] >> 16) & 0xff), static_cast<uint8_t>((frame._palette[i] >> 8u) & 0xffu), static_cast<uint8_t>(frame._palette[i] & 0xffu)});
                }
                else {
                    appendU8(out, {0,0,0});
                }
            }
        }
        // Image Data
        auto minCodeSize = (frame._palette.empty() ? colBitsG : colBitsL);
        appendU8(out, minCodeSize);
        SubblockInserter sbi(out); // Generates sequence of [<lengh> <data>]* 00
        ghc::compression::LzwEncoder lzw(sbi, minCodeSize);
        lzw.encode(frame._pixels);
    }
    // Trailer
    appendU8(out, 0x3B);
    return true;
}

inline bool GifImage::writeToFile(std::string filename)
{
    ByteArray image;
    encode(image);
    std::ofstream os(filename, std::ios::binary);
    os.write(reinterpret_cast<const char*>(image.data()), image.size());
    return !!os;
}

//#endif
