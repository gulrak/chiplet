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

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ghc/gifimage.hpp>
#include <nlohmann/json_fwd.hpp>

namespace emu
{

struct OctoOptions
{
    static constexpr size_t OCTO_PALETTE_SIZE = 6;
    static constexpr size_t MAX_PALETTE_SIZE = 16;
    enum class Font { Octo, Vip, Dream6800, Eti660, SChip, FishNChips };
    enum class Touch { None, Swipe, Seg16, Seg16Fill, GamePad, Vip };
    // core settings
    int tickrate{20};                                  // {7,15,20,30,100,200,500,1000,10000,...}
    int maxRom{3584};                                  // {3232, 3583, 3584, 65024}
    int rotation{0};                                   // {0, 90, 180, 270}
    Font font{Font::Octo};                             // OCTO_FONT_...
    Touch touchMode{Touch::None};                      // OCTO_TOUCH_...
    std::array<uint32_t, OCTO_PALETTE_SIZE> colors{};  // OCTO_COLOR_... (RGB)
    // Optional, not known to Octo: the colors of the plane values 0 to 15 (RGB). If set, its first
    // four entries are also what colors[0..3] hold, so Octo shows at least those right.
    std::vector<uint32_t> palette;

    // quirks flags
    bool qShift{false};
    bool qLoadStore{false};
    bool qJump0{false};
    bool qLogic{false};
    bool qClip{false};
    bool qVBlank{false};

    OctoOptions() { colors = {0x996600, 0xFFCC00, 0xFF6600, 0x662200, 0x000000, 0xFFAA00}; }
};

class OctoCartridge : private ghc::GifImage
{
public:
    using DataSpan = std::span<const uint8_t>;
    using Data = std::vector<uint8_t>;
    enum class BaseImage { CD48, OCTO };

    OctoCartridge(std::string filename) : _filename(std::move(filename)) {}
    OctoCartridge(ByteView data) : GifImage(data) {}
    bool loadCartridge();
    bool saveCartridge(std::string_view programSource, const std::string& label, const DataSpan& image, BaseImage base = BaseImage::CD48);
    std::vector<uint32_t> getImage() const;
    const OctoOptions& getOptions() const;
    void setOptions(nlohmann::json& options);
    void setOptions(OctoOptions& options);
    bool setOptionsFromVariant(std::string_view variant);
    const std::string& getSource() const;
    const std::string& getJsonString() const;
    static uint32_t getColorFromName(const std::string& name, uint32_t defaultColor = 0);
    // The source of a binary only cartridge: a ': main' at the program start, followed by a hex dump
    static std::string binarySource(const DataSpan& code);

private:
    void printLabel(const std::string& label, uint8_t color);
    char getCartByte(size_t& offset) const;
    std::string _filename;
    std::string _jsonStr;
    OctoOptions _options;
    std::string _source;
};

}
