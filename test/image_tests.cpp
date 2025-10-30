//
// Created by Steffen Schümann on 25.10.25.
//
#include <doctest/doctest.h>

#include <chiplet/imagehelper.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <chiplet/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <nothings/stb_image_write.h>

struct Image
{
    int width;
    int height;
    int numChannels;
    std::vector<uint8_t> data;
    std::vector<uint8_t> loaded;
};

void scaleDown4x(Image& image)
{
    const int newWidth = image.width / 4;
    const int newHeight = image.height / 4;
    std::vector<uint8_t> newData(newWidth * newHeight * 4);

    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            int srcPos = (y * 4 * image.width + x * 4) * 4;
            int dstPos = (y * newWidth + x) * 4;
            newData[dstPos] = image.data[srcPos];
            newData[dstPos + 1] = image.data[srcPos + 1];
            newData[dstPos + 2] = image.data[srcPos + 2];
            newData[dstPos + 3] = image.data[srcPos + 3];
        }
    }

    image.width = newWidth;
    image.height = newHeight;
    image.data = std::move(newData);
}

void scaleUp4x(Image& image)
{
    const int newWidth = image.width * 4;
    const int newHeight = image.height * 4;
    std::vector<uint8_t> newData(newWidth * newHeight * 4);

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            int srcPos = (y * image.width + x) * 4;
            for (int dy = 0; dy < 4; ++dy) {
                for (int dx = 0; dx < 4; ++dx) {
                    int dstPos = ((y * 4 + dy) * newWidth + (x * 4 + dx)) * 4;
                    newData[dstPos] = image.data[srcPos];
                    newData[dstPos + 1] = image.data[srcPos + 1];
                    newData[dstPos + 2] = image.data[srcPos + 2];
                    newData[dstPos + 3] = image.data[srcPos + 3];
                }
            }
        }
    }

    image.width = newWidth;
    image.height = newHeight;
    image.data = std::move(newData);
}

Image loadImage(const std::string& filename)
{
    int width,height,numChannels;
    auto* data = stbi_load(filename.c_str(), &width, &height, &numChannels, 4);
    auto result = Image{width, height, numChannels, std::vector<uint8_t>(data, data + width * height * 4)};
    stbi_image_free(data);
    result.loaded = result.data;
    return result;
}

void saveImage(const std::string& filename, const Image& image)
{
    stbi_write_png(filename.c_str(), image.width, image.height, image.numChannels, image.data.data(), 0);
}

std::vector<uint8_t> threshold(Image& image, const std::vector<img::Color>& palette)
{
    return img::threshold(image.data.data(), image.width, image.height, palette);
}

std::vector<uint8_t> dither(Image& image, const std::vector<img::Color>& palette)
{
    return img::dither(image.data.data(), image.width, image.height, palette);
}

TEST_SUITE("Images")
{
    TEST_CASE("default bw")
    {
        auto original = loadImage(IMAGES_PATH "/clock.png");
        CHECK_EQ(original.width, 128);
        CHECK_EQ(original.height, 128);
        CHECK_EQ(original.numChannels, 4);
        CHECK_EQ(original.data.size(), 128 * 128 * 4);

        auto expectedBW = loadImage(IMAGES_PATH "/clock-bw.png");
        std::vector<img::Color> palette{{0,0,0}, {255,255,255}};
        auto defaultBW = img::threshold(original.data.data(), original.width, original.height, palette);
        saveImage(IMAGES_PATH "/clock-bw-test.png", original);
        CHECK_EQ(defaultBW.size(), original.width * original.height);
        CHECK_EQ(original.data, expectedBW.data);
    }

    TEST_CASE("adjusted bw")
    {
        auto original = loadImage(IMAGES_PATH "/clock.png");
        auto expectedBG = loadImage(IMAGES_PATH "/clock-bg.png");
        std::vector<img::Color> palette{{0,0,0}, {0xAA,0xAA,0xAA}};
        auto defaultBG = img::threshold(original.data.data(), original.width, original.height, palette);
        saveImage(IMAGES_PATH "/clock-bg-test.png", original);
        CHECK_EQ(defaultBG.size(), original.width * original.height);
        // The reference images are not correct in that the clock-bg one is black white, but the
        // expected results need to be in the palette, so would be black and 0xAA gray.
        for (auto& c : original.data) if (c == 0xAA) c = 0xFF;
        CHECK_EQ(original.data, expectedBG.data);
    }

    TEST_CASE("four colors")
    {
        auto original = loadImage(IMAGES_PATH "/clock.png");
        auto expectedBG = loadImage(IMAGES_PATH "/clock-four-colors.png");
        std::vector<img::Color> palette{{0,0,0}, {0xA2,0x26,0x32}, {0xB8, 0x6F, 0x51}, {0xFF, 0xFF, 0xFF}};
        auto defaultBG = img::threshold(original.data.data(), original.width, original.height, palette);
        saveImage(IMAGES_PATH "/clock-four-colors-test.png", original);
        CHECK_EQ(defaultBG.size(), original.width * original.height);
        CHECK_EQ(original.data, expectedBG.data);
    }

    TEST_CASE("dithered")
    {
        auto original = loadImage(IMAGES_PATH "/clock.png");
        // The reference images are scaled by 4, so for the dithering to work it needs to be downscaled first
        scaleDown4x(original);
        auto expectedBG = loadImage(IMAGES_PATH "/clock-dithered.png");
        std::vector<img::Color> palette{{0,0,0}, {0xA2,0x26,0x32}, {0xB8, 0x6F, 0x51}, {0xFF, 0xFF, 0xFF}};
        auto defaultBG = img::dither(original.data.data(), original.width, original.height, palette);
        // Undo the downscaling by upscaling the result again
        scaleUp4x(original);
        saveImage(IMAGES_PATH "/clock-dithered-test.png", original);
        CHECK_EQ(defaultBG.size() * 16, original.width * original.height);
        CHECK_EQ(original.data, expectedBG.data);
    }

}