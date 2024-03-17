//
// Created by Steffen Schümann on 28.02.24.
//
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cctype>

#define private public
#define protected public
//#define GIF_DEBUG_OUTPUT
#include "../include/chiplet/gifimage.hpp"
#include <ghc/lzw.hpp>
#undef private

const uint8_t sample_gif[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x0a, 0x00, 0x0a, 0x00, 0x91, 0x00, 0x00, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x21, 0xf9, 0x04, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x00, 0x02, 0x16, 0x8c, 0x2d, 0x99,
    0x87, 0x2a, 0x1c, 0xdc, 0x33, 0xa0, 0x02, 0x75, 0xec, 0x95, 0xfa, 0xa8, 0xde, 0x60, 0x8c, 0x04,
    0x91, 0x4c, 0x01, 0x00, 0x3b
};


int main()
{
    //GifImage gif(sample_gif);
    //auto gif = GifImage::fromFile("/home/schuemann/Downloads/murder.gif");
    auto gif = GifImage::fromFile("../../murder.gif");
    //auto gif = GifImage::fromFile("/home/schuemann/attic/dev/c-octo/carts/test_tiny.gif");
    //ghc::hexDump(std::cout, gif.compressed().data(), gif.compressed().size());
    const auto& frame = gif.getFrame(0);
    auto compr = gif.compressed();
    std::cout << "LZW-Data (" << gif.minCodeSize() << "):" << std::endl;
    ghc::hexCode(std::cout, compr.data(), compr.size());
    std::vector<uint8_t> compressed;
    compressed.reserve(frame._pixels.size());
    {
        auto output = std::back_inserter(compressed);
        auto lzw = ghc::compression::LzwEncoder(output, gif.minCodeSize());
        lzw.encode(frame._pixels);
    }
    ghc::hexDump(std::cout, compressed.data(), compressed.size());
    gif.writeToFile("test.gif");
    auto gif2 = GifImage::fromFile("test.gif");
    //compress3(std::cout, gif.compressed(), 7);
    return 0;
}
