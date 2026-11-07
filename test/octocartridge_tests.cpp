//
// Round trip checks for the Octo cartridge writer and reader and the LZW packer they rely on.
//
#include <doctest/doctest.h>

#include <chiplet/octocartridge.hpp>
#include <chiplet/octocompiler.hpp>
#include <ghc/gifimage.hpp>
#include <ghc/lzw.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace {

using BaseImage = emu::OctoCartridge::BaseImage;

std::string tempCart(const std::string& name)
{
    return (std::filesystem::temp_directory_path() / ("chiplet-cart-test-" + name + ".gif")).string();
}

std::vector<uint8_t> randomSymbols(size_t size, int codeSize, unsigned seed)
{
    std::mt19937 generator(seed);
    std::vector<uint8_t> result(size);
    for (auto& symbol : result) {
        symbol = generator() % (1u << codeSize);
    }
    return result;
}

bool lzwRoundTrips(const std::vector<uint8_t>& input, int codeSize)
{
    std::vector<uint8_t> packed;
    {
        auto output = std::back_inserter(packed);
        ghc::compression::LzwEncoder encoder(output, codeSize);
        encoder.encode(input);
    }
    auto from = packed.cbegin();
    ghc::compression::LzwDecoder decoder(from, packed.cend(), codeSize);
    auto unpacked = decoder.decompress();
    return unpacked && *unpacked == input;
}

std::string longSource()
{
    std::string source = ": main\n  loop again\n";
    while (source.size() < 40000) {
        source += "  0x12 0x34 0x56 0x78 # " + std::to_string(source.size()) + "\n";
    }
    return source;
}

std::string saveAndReloadSource(const std::string& source, const std::string& name, BaseImage base = BaseImage::CD48)
{
    const auto file = tempCart(name);
    emu::OctoCartridge writer(file);
    REQUIRE(writer.saveCartridge(source, "TEST", {}, base));
    emu::OctoCartridge reader(file);
    reader.loadCartridge();
    std::filesystem::remove(file);
    return reader.getSource();
}

struct TestFrame
{
    std::vector<uint8_t> pixels;
    std::vector<uint8_t> localColors;  // RGB
    uint8_t minCode;
};

// a 2x2 GIF with the given color tables, built by hand so the tests don't depend on GifImage::encode
std::vector<uint8_t> buildGif(const std::vector<uint8_t>& globalColors, int globalBits, const std::vector<TestFrame>& frames)
{
    std::vector<uint8_t> gif = {'G', 'I', 'F', '8', '9', 'a', 2, 0, 2, 0};
    gif.push_back(static_cast<uint8_t>((globalColors.empty() ? 0x70 : 0xF0) | (globalColors.empty() ? 0 : globalBits - 1)));
    gif.push_back(0);
    gif.push_back(0);
    gif.insert(gif.end(), globalColors.begin(), globalColors.end());
    for (const auto& frame : frames) {
        gif.insert(gif.end(), {0x2C, 0, 0, 0, 0, 2, 0, 2, 0});
        if (frame.localColors.empty()) {
            gif.push_back(0);
        }
        else {
            int bits = 1;
            while ((3u << bits) < frame.localColors.size()) {
                ++bits;
            }
            gif.push_back(static_cast<uint8_t>(0x80 | (bits - 1)));
            gif.insert(gif.end(), frame.localColors.begin(), frame.localColors.end());
        }
        gif.push_back(frame.minCode);
        std::vector<uint8_t> packed;
        {
            auto output = std::back_inserter(packed);
            ghc::compression::LzwEncoder encoder(output, frame.minCode);
            encoder.encode(frame.pixels);
        }
        gif.push_back(static_cast<uint8_t>(packed.size()));
        gif.insert(gif.end(), packed.begin(), packed.end());
        gif.push_back(0);
    }
    gif.push_back(0x3B);
    return gif;
}

void checkGifSurvivesRewrite(const std::vector<uint8_t>& gif, const std::string& name)
{
    ghc::GifImage original{std::span<const uint8_t>(gif.data(), gif.size())};
    REQUIRE(original.isValid());
    const auto file = tempCart(name);
    REQUIRE(original.writeToFile(file));
    auto reloaded = ghc::GifImage::fromFile(file);
    std::filesystem::remove(file);
    REQUIRE(reloaded.numFrames() == original.numFrames());
    for (size_t i = 0; i < original.numFrames(); ++i) {
        CHECK(reloaded.getFrame(i)._pixels == original.getFrame(i)._pixels);
        CHECK(reloaded.getFrame(i)._palette == original.getFrame(i)._palette);
    }
}

}  // namespace

TEST_SUITE("GifImage")
{
    TEST_CASE("local color tables are read as RGB with the right size")
    {
        const auto gif = buildGif({}, 0, {{{0, 1, 2, 3}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, 2}});
        ghc::GifImage image{std::span<const uint8_t>(gif.data(), gif.size())};
        REQUIRE(image.isValid());
        REQUIRE(image.numFrames() == 1);
        CHECK(image.getFrame(0)._palette == std::vector<uint32_t>{0x010203, 0x040506, 0x070809, 0x0A0B0C});
        CHECK(image.getFrame(0)._pixels == std::vector<uint8_t>{0, 1, 2, 3});
    }

    TEST_CASE("only local color tables survive a rewrite")
    {
        checkGifSurvivesRewrite(buildGif({}, 0, {{{0, 1, 2, 3}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, 2}, {{3, 2, 1, 0}, {9, 9, 9, 8, 8, 8, 7, 7, 7, 6, 6, 6}, 2}}), "gif-local");
    }

    TEST_CASE("two colors survive a rewrite")
    {
        checkGifSurvivesRewrite(buildGif({0, 0, 0, 255, 255, 255}, 1, {{{0, 1, 1, 0}, {}, 2}}), "gif-two-colors");
    }

    TEST_CASE("pixels outside of the color table survive a rewrite")
    {
        checkGifSurvivesRewrite(buildGif({0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3}, 2, {{{0, 4, 1, 4}, {}, 3}}), "gif-outside");
    }

    TEST_CASE("an invalid code size is rejected")
    {
        auto gif = buildGif({0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3}, 2, {{{0, 1, 2, 3}, {}, 2}});
        gif[10 + 3 + 12 + 10] = 255;  // header, screen data, color table, image descriptor
        ghc::GifImage image{std::span<const uint8_t>(gif.data(), gif.size())};
        CHECK_FALSE(image.isValid());
    }
}

TEST_SUITE("OctoCartridge")
{
    TEST_CASE("LZW round trips at every code size")
    {
        for (int codeSize = 2; codeSize <= 8; ++codeSize) {
            CAPTURE(codeSize);
            size_t failures = 0;
            for (size_t size = 1; size <= 600; ++size) {
                failures += lzwRoundTrips(randomSymbols(size, codeSize, size), codeSize) ? 0 : 1;
            }
            // these sizes fill the dictionary one or more times
            for (size_t size : {4000, 8200, 20480, 81920}) {
                failures += lzwRoundTrips(randomSymbols(size, codeSize, size + 1), codeSize) ? 0 : 1;
            }
            CHECK(failures == 0);
        }
    }

    TEST_CASE("source survives with both base images")
    {
        const std::string shortSource = ": main\n  loop again\n";
        CHECK(saveAndReloadSource(shortSource, "short-cd48", BaseImage::CD48) == shortSource);
        CHECK(saveAndReloadSource(shortSource, "short-octo", BaseImage::OCTO) == shortSource);
        const auto multiFrameSource = longSource();
        CHECK(saveAndReloadSource(multiFrameSource, "long-cd48", BaseImage::CD48) == multiFrameSource);
        CHECK(saveAndReloadSource(multiFrameSource, "long-octo", BaseImage::OCTO) == multiFrameSource);
    }

    TEST_CASE("a cartridge object can save more than once")
    {
        const auto file = tempCart("twice");
        const std::string source = ": main\n  loop again\n";
        emu::OctoCartridge writer(file);
        CHECK(writer.saveCartridge("# first\n", "ONE", {}));
        CHECK(writer.saveCartridge(source, "TWO", {}));
        emu::OctoCartridge reader(file);
        CHECK(reader.loadCartridge());
        CHECK(reader.getSource() == source);
        std::filesystem::remove(file);
    }

    TEST_CASE("options survive")
    {
        emu::OctoOptions options;
        options.tickrate = 1000;
        options.maxRom = 65024;
        options.rotation = 270;
        options.font = emu::OctoOptions::Font::Eti660;
        options.touchMode = emu::OctoOptions::Touch::GamePad;
        options.colors = {0x112233, 0x445566, 0x778899, 0xAABBCC, 0xDDEEFF, 0x010203};
        options.qShift = options.qLoadStore = options.qJump0 = options.qLogic = options.qClip = options.qVBlank = true;
        const auto file = tempCart("options");
        emu::OctoCartridge writer(file);
        writer.setOptions(options);
        REQUIRE(writer.saveCartridge(": main\n  loop again\n", "", {}));
        emu::OctoCartridge reader(file);
        REQUIRE(reader.loadCartridge());
        const auto& result = reader.getOptions();
        CHECK(result.tickrate == options.tickrate);
        CHECK(result.maxRom == options.maxRom);
        CHECK(result.rotation == options.rotation);
        CHECK(result.font == options.font);
        CHECK(result.touchMode == options.touchMode);
        CHECK(result.colors == options.colors);
        CHECK(result.qShift);
        CHECK(result.qLoadStore);
        CHECK(result.qJump0);
        CHECK(result.qLogic);
        CHECK(result.qClip);
        CHECK(result.qVBlank);
        std::filesystem::remove(file);
    }

    TEST_CASE("a palette of 16 colors survives and sets the first four colors")
    {
        emu::OctoOptions options;
        for (uint32_t i = 0; i < 16; ++i)
            options.palette.push_back(0x101010 * i + 1);
        const auto file = tempCart("palette");
        emu::OctoCartridge writer(file);
        writer.setOptions(options);
        REQUIRE(writer.saveCartridge(": main\n  loop again\n", "", {}));
        emu::OctoCartridge reader(file);
        REQUIRE(reader.loadCartridge());
        const auto& result = reader.getOptions();
        CHECK(result.palette == options.palette);
        for (size_t i = 0; i < 4; ++i)
            CHECK(result.colors[i] == options.palette[i]);
        // readers that only know the four colors get them from the palette as well
        const auto json = nlohmann::json::parse(reader.getJsonString());
        CHECK(json["options"]["blendColor"] == "#303031");
        std::filesystem::remove(file);
    }

    TEST_CASE("a palette in the options JSON wins, and more than 16 colors are cut")
    {
        auto json = nlohmann::json::parse(R"({"fillColor": "#ffffff", "palette": [
            "#000000", "#010101", "red", "#030303", "#040404", "#050505", "#060606", "#070707", "#080808",
            "#090909", "#0a0a0a", "#0b0b0b", "#0c0c0c", "#0d0d0d", "#0e0e0e", "#0f0f0f", "#ffffff"]})");
        emu::OctoCartridge cart(tempCart("palette-json"));
        cart.setOptions(json);
        const auto& result = cart.getOptions();
        REQUIRE(result.palette.size() == 16);
        CHECK(result.colors[1] == 0x010101);
        CHECK(result.colors[2] == 0xFF0000);
        CHECK(result.palette[15] == 0x0F0F0F);
    }

    TEST_CASE("default options survive unchanged")
    {
        const auto file = tempCart("defaults");
        emu::OctoCartridge writer(file);
        REQUIRE(writer.saveCartridge(": main\n  loop again\n", "", {}));
        emu::OctoCartridge reader(file);
        REQUIRE(reader.loadCartridge());
        CHECK(reader.getOptions().colors == emu::OctoOptions{}.colors);
        CHECK(reader.getOptions().palette.empty());
        CHECK(nlohmann::json::parse(reader.getJsonString())["options"].contains("palette") == false);
        std::filesystem::remove(file);
    }

    TEST_CASE("non ASCII text")
    {
        const std::string utf8 = ": main # Sch\xC3\xBCmann \xF0\x9F\x90\x99\n  loop again\n";
        CHECK(saveAndReloadSource(utf8, "utf8") == utf8);
        // invalid UTF-8 must not throw, the bad byte gets replaced
        const auto replaced = saveAndReloadSource(": main # Sch\xFCmann\n  loop again\n", "latin1");
        CHECK(replaced == ": main # Sch\xEF\xBF\xBDmann\n  loop again\n");
    }

    TEST_CASE("variant profiles")
    {
        emu::OctoCartridge cart(std::string("unused.gif"));
        CHECK_FALSE(cart.setOptionsFromVariant("nonsense"));
        CHECK(cart.setOptionsFromVariant("xo-chip"));
        CHECK(cart.getOptions().maxRom == 65024);
        CHECK_FALSE(cart.getOptions().qClip);
        CHECK(cart.setOptionsFromVariant("schip"));
        CHECK(cart.getOptions().maxRom == 3583);
        CHECK(cart.getOptions().qJump0);
        CHECK(cart.setOptionsFromVariant("chip-8"));
        CHECK(cart.getOptions().maxRom == 3232);
        CHECK(cart.getOptions().qVBlank);
        CHECK(cart.setOptionsFromVariant("octo"));
        CHECK(cart.getOptions().maxRom == 3584);
    }

    TEST_CASE("binary source format")
    {
        const std::vector<uint8_t> code = {0x00, 0xE0, 0x12, 0x00};
        CHECK(emu::OctoCartridge::binarySource(code) == ": main\n0x00 0xe0 0x12 0x00\n");
        std::vector<uint8_t> line(17, 0xAB);
        const auto text = emu::OctoCartridge::binarySource(line);
        CHECK(text.find('\n', 7) == 7 + 16 * 5 - 1);
        CHECK(text.ends_with("\n0xab\n"));
    }

    TEST_CASE("binary source assembles back to the same bytes")
    {
        for (size_t size : {1, 2, 3, 16, 17, 4000, 70000 - 512}) {
            CAPTURE(size);
            const auto code = randomSymbols(size, 8, static_cast<unsigned>(size));
            const auto text = emu::OctoCartridge::binarySource(code);
            emu::OctoCompiler compiler;
            REQUIRE(compiler.compile("binary.8o", text).resultType == emu::CompileResult::eOK);
            CHECK(std::vector<uint8_t>(compiler.code(), compiler.code() + compiler.codeSize()) == code);
        }
    }

    TEST_CASE("the image of a cartridge with a local color table")
    {
        const auto gif = buildGif({}, 0, {{{0, 1, 2, 3}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, 2}});
        emu::OctoCartridge cart(std::span<const uint8_t>(gif.data(), gif.size()));
        const auto image = cart.getImage();
        REQUIRE(image.size() == 4);
        const auto* bytes = reinterpret_cast<const uint8_t*>(&image[3]);
        CHECK(bytes[0] == 10);
        CHECK(bytes[1] == 11);
        CHECK(bytes[2] == 12);
        CHECK(bytes[3] == 255);
    }

    TEST_CASE("a plain GIF is not a cartridge")
    {
        // 10x10 pixels with four colors
        static const std::array<uint8_t, 69> plainGif = {
            0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x0a, 0x00, 0x0a, 0x00, 0x91, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x21, 0xf9, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00,
            0x00, 0x00, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x00, 0x02, 0x16, 0x8c, 0x2d, 0x99, 0x87, 0x2a, 0x1c, 0xdc, 0x33, 0xa0, 0x02, 0x75, 0xec, 0x95, 0xfa, 0xa8, 0xde, 0x60, 0x8c, 0x04, 0x91, 0x4c, 0x01, 0x00, 0x3b};
        emu::OctoCartridge cart(std::span<const uint8_t>(plainGif.data(), plainGif.size()));
        CHECK_FALSE(cart.loadCartridge());
        CHECK(cart.getSource().empty());
    }
}
