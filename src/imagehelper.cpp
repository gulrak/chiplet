//---------------------------------------------------------------------------------------
// chiplet/imagehelper.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2025, Steffen Schümann <s.schuemann@pobox.com>
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
#include <chiplet/imagehelper.hpp>

#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include <../external/nothings/stb_image.h>

#include "chiplet/utility.hpp"

namespace img {

std::vector<Color> ImageFile::fetchPalette() const
{
    if (!_palette) {
        load();
        if (isBMP()) {

        }
    }
}
std::span<const uint8_t> ImageFile::fetchPixels() const
{
    if (!_pixels) {
        load();
        int numChannels{};
        auto* data =  stbi_load_from_memory(static_cast<const stbi_uc*>(_fileData.data()), _fileData.size(), &_width, &_height, &numChannels, 4);
        if (!data) {
            throw std::runtime_error("Failed to load image: " + _filename.string());
        }
        _pixels = std::span<const uint8_t>(data, _width * _height * 4);
        stbi_image_free(data);
    }
    return *_pixels;
}

void ImageFile::load()
{
    if (_fileData.empty()) {
        _fileData = loadFile(_filename);
    }
}

bool ImageFile::isPNG() const
{
    static constexpr std::uint8_t MAGIC[8] = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};
    if (_fileData.size() <= 8)
        return false;
    for (int i = 0; i < 8; ++i)
        if (_fileData[i] != MAGIC[i])
            return false;
    return true;
}

bool ImageFile::isBMP() const
{
    if (_fileData.size() < 18)
        return false;
    if (!(_fileData[0] == 'B' && _fileData[1] == 'M'))
        return false;
    auto bfSize     = rd32(_fileData.data() + 2);
    auto bfOffBits  = rd32(_fileData.data() + 10);
    auto dibSize = rd32(_fileData.data() + 14);
    if (_fileData.size() < 14 + dibSize)
        return false;
    return true;
}

}  // namespace img
