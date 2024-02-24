//---------------------------------------------------------------------------------------
// src/emulation/octocartridge.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2022, Steffen Schümann <s.schuemann@pobox.com>
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

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <ghc/span.hpp>
#include <nlohmann/json_fwd.hpp>

namespace emu
{

struct OctoOptions
{
    static constexpr size_t OCTO_PALETTE_SIZE = 6;
    enum class Font { Octo, Vip, Dream6800, Eti660, SChip, FishNChips };
    enum class Touch { None, Swipe, Seg16, Seg16Fill, GamePad, Vip };
    // core settings
    int tickrate{20};                                  // {7,15,20,30,100,200,500,1000,10000,...}
    int maxRom{3584};                                  // {3232, 3583, 3584, 65024}
    int rotation{0};                                   // {0, 90, 180, 270}
    Font font{Font::Octo};                             // OCTO_FONT_...
    Touch touchMode{Touch::None};                      // OCTO_TOUCH_...
    std::array<uint32_t, OCTO_PALETTE_SIZE> colors{};  // OCTO_COLOR_... (ARGB)

    // quirks flags
    bool qShift{false};
    bool qLoadStore{false};
    bool qJump0{false};
    bool qLogic{false};
    bool qClip{false};
    bool qVBlank{false};

    OctoOptions() { colors = {0xFF996600, 0xFFFFCC00, 0xFFFF6600, 0xFF662200, 0xFF000000, 0xFFFFAA00}; }
};

class OctoCartridge
{
public:
    using DataSpan = ghc::span<const uint8_t>;
    using Data = std::vector<uint8_t>;

    OctoCartridge(std::string filename) : _filename(filename) {}
    bool loadCartridge();
    bool saveCartridge(const OctoOptions& options, std::string_view programSource, const std::string& label, const DataSpan& image);
    const DataSpan&  getImage(const std::string& text) const;
    const OctoOptions& getOptions() const;
    static uint32_t getColorFromName(const std::string& name);

private:
    nlohmann::json loadJson();
    void writePng(std::string filename) const;
    bool decode(DataSpan dataSpan);
    bool encode(DataSpan dataSpan);
    static Data decompress(DataSpan dataSpan, int mcs);
    char getCartByte(size_t& offset) const;
    static uint16_t readU16(const uint8_t*& data)
    {
        uint16_t result = *data++;
        result |= *data++ << 8;
        return result;
    }
    struct Frame {
        uint16_t _left{};
        uint16_t _top{};
        uint16_t _width{};
        uint16_t _height{};
        std::vector<uint32_t> _palette;
        Data _data;
    };
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
    OctoOptions _options;
    std::string _source;
};

}
