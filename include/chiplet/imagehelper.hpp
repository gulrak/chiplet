//---------------------------------------------------------------------------------------
// chiplet/imagehelper.hpp
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
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <ghc/fs_fwd.hpp>

namespace img {

struct Color
{
    uint8_t r{}, g{}, b{}, a{0xff};
};

struct Palette
{
    std::vector<Color> colors;
};

class ImageFile
{
public:
    explicit ImageFile(const std::string& filename)
        : _filename(filename)
    {
    }
    explicit ImageFile(ghc::filesystem::path path)
        : _filename(std::move(path))
    {
    }

    std::vector<Color> fetchPalette() const;
    std::span<const uint8_t> fetchPixels() const;

private:
    void load();
    bool isPNG() const;
    bool isBMP() const;
    ghc::filesystem::path _filename;
    std::vector<uint8_t> _fileData{};
    int _width{}, _height{};
    std::optional<Palette> _palette;
    std::optional<std::vector<uint8_t>> _pixels{};
};

constexpr uint8_t clampU8(double v)
{
    if (v < 0.0)
        return 0;
    if (v > 255.0)
        return 255;
    return static_cast<uint8_t>(v);
}

inline std::pair<size_t, Color> nearestColor(const std::array<double, 3>& test, uint8_t alpha, std::span<const Color> palette)
{
    double bestDist = std::numeric_limits<double>::infinity();
    const bool transparence = (!palette.empty() && palette[0].a == 0);
    Color best{};
    size_t idx = 0;
    size_t bestIdx = 0;
    if (transparence) {
        palette = palette.subspan(1);
        ++idx;
        if (alpha == 0) {
            return {0, {0, 0, 0, 0}};
        }
    }
    for (const auto& col : palette) {
        const double rmean = (test[0] + static_cast<double>(col.r)) / 2.0;
        const double r = test[0] - static_cast<double>(col.r);
        const double g = test[1] - static_cast<double>(col.g);
        const double b = test[2] - static_cast<double>(col.b);

        // Approximation from https://stackoverflow.com/questions/2103368/color-logic-algorithm
        const double dist = std::sqrt((((512.0 + rmean) * r * r) / 256.0) + (4.0 * g * g) + (((767.0 - rmean) * b * b) / 256.0));

        if (dist < bestDist) {
            bestIdx = idx;
            bestDist = dist;
            best = col;
        }
        ++idx;
    }
    return {bestIdx, best};
}

// Threshold an image to the given palette
inline std::vector<uint8_t> threshold(uint8_t* pixels, int width, int height, std::span<const Color> palette)
{
    std::vector<uint8_t> result;
    result.reserve(width * height);
    const int stride = width * 4;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int index = y * stride + x * 4;

            auto [newIdx, newColor] = nearestColor({static_cast<double>(pixels[index + 0]), static_cast<double>(pixels[index + 1]), static_cast<double>(pixels[index + 2])}, pixels[index + 3], palette);

            pixels[index + 0] = newColor.r;
            pixels[index + 1] = newColor.g;
            pixels[index + 2] = newColor.b;
            // pixels[index + 3] (alpha) left unchanged
            result.push_back(newIdx);
        }
    }
    return result;
}

// Floyd–Steinberg dither to the given palette
inline std::vector<uint8_t> dither(uint8_t* pixels, int width, int height, std::span<const Color> palette)
{
    std::vector<uint8_t> result;
    result.reserve(width * height);
    const int stride = width * 4;
    const size_t N = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;

    // Work in doubles to preserve propagated error
    std::vector<double> buf(N);
    for (size_t i = 0; i < N; ++i)
        buf[i] = static_cast<double>(pixels[i]);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = y * stride + x * 4;

            auto [newIdx, newCol] = nearestColor({buf[index + 0], buf[index + 1], buf[index + 2]}, buf[index + 3], palette);
            result.push_back(newIdx);
            bool transparent = (newCol.a == 0);
            const double errR = transparent ? 0.0 : buf[index + 0] - static_cast<double>(newCol.r);
            const double errG = transparent ? 0.0 : buf[index + 1] - static_cast<double>(newCol.g);
            const double errB = transparent ? 0.0 : buf[index + 2] - static_cast<double>(newCol.b);

            buf[index + 0] = static_cast<double>(newCol.r);
            buf[index + 1] = static_cast<double>(newCol.g);
            buf[index + 2] = static_cast<double>(newCol.b);
            // alpha left as-is in buf

            // Propagate error (Floyd–Steinberg kernel)
            // X + 1
            if (x < width - 1) {
                buf[index + 4 + 0] += errR * 7.0 / 16.0;
                buf[index + 4 + 1] += errG * 7.0 / 16.0;
                buf[index + 4 + 2] += errB * 7.0 / 16.0;
            }

            if (y == height - 1)
                continue;
            index += stride;  // move to next row

            // X - 1, Y + 1
            if (x > 0) {
                buf[index - 4 + 0] += errR * 3.0 / 16.0;
                buf[index - 4 + 1] += errG * 3.0 / 16.0;
                buf[index - 4 + 2] += errB * 3.0 / 16.0;
            }

            // X, Y + 1
            buf[index + 0] += errR * 5.0 / 16.0;
            buf[index + 1] += errG * 5.0 / 16.0;
            buf[index + 2] += errB * 5.0 / 16.0;

            // X + 1, Y + 1
            if (x < width - 1) {
                buf[index + 4 + 0] += errR * 1.0 / 16.0;
                buf[index + 4 + 1] += errG * 1.0 / 16.0;
                buf[index + 4 + 2] += errB * 1.0 / 16.0;
            }
        }
    }

    // Copy back (clamped) into the byte image
    for (size_t i = 0; i < N; i += 4) {
        pixels[i + 0] = clampU8(buf[i + 0]);
        pixels[i + 1] = clampU8(buf[i + 1]);
        pixels[i + 2] = clampU8(buf[i + 2]);
        // keep original alpha from input
        // pixels[i + 3] = clampU8(buf[i + 3]);
    }
    return result;
}

struct PngPalette
{
    std::vector<std::array<std::uint8_t, 3>> rgb;    // palette colors
    std::optional<std::vector<std::uint8_t>> alpha;  // alpha for first K entries (<= rgb.size())
    std::uint8_t color_type = 0xFF;                  // from IHDR (expect 3 for indexed-color)
};

namespace detail {
inline std::uint32_t readU32BE(const std::uint8_t* p) noexcept
{
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) | (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

template <typename T>
inline void ensure(T&& cond, std::string_view msg)
{
    if (!cond)
        throw std::runtime_error(std::string(msg));
}
[[noreturn]] inline void fail(const char* msg)
{
    throw std::runtime_error(msg);
}
inline std::uint16_t readU16LE(const std::uint8_t* p)
{
    return std::uint16_t(p[0]) | (std::uint16_t(p[1]) << 8);
}
inline std::uint32_t readU32LE(const std::uint8_t* p)
{
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
}  // namespace detail

inline Palette extractPngPalette(std::span<const std::uint8_t> bytes)
{
    using namespace detail;

    static constexpr std::uint8_t SIG[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    ensure(bytes.size() >= 8, "Not a PNG: too small");
    for (int i = 0; i < 8; ++i)
        ensure(bytes[i] == SIG[i], "Bad PNG signature");

    std::size_t off = 8;
    Palette out;
    bool saw_PLTE = false;

    auto safe_advance = [&](std::size_t need) {
        ensure(off + need <= bytes.size(), "PNG truncated");
        auto p = bytes.data() + off;
        off += need;
        return p;
    };

    while (off + 12 <= bytes.size()) {
        const auto* lenp = safe_advance(4);
        const auto len = std::size_t(readU32BE(lenp));
        const auto* typep = safe_advance(4);
        const std::array<char, 5> type = {char(typep[0]), char(typep[1]), char(typep[2]), char(typep[3]), 0};

        const auto* datap = safe_advance(len);
        (void)safe_advance(4);  // CRC (ignored here)
        uint8_t color_type = 0xFF;
        if (type[0] == 'I' && type[1] == 'H' && type[2] == 'D' && type[3] == 'R') {
            ensure(len >= 13, "IHDR too short");
            color_type = datap[9];  // 10th byte of IHDR
        }
        else if (type[0] == 'P' && type[1] == 'L' && type[2] == 'T' && type[3] == 'E') {
            ensure(len % 3 == 0, "PLTE length not multiple of 3");
            const std::size_t n = len / 3;
            ensure(n >= 1 && n <= 256, "PLTE has invalid entry count");
            out.colors.resize(n);
            for (std::size_t i = 0; i < n; ++i) {
                out.colors[i] = {datap[i * 3 + 0], datap[i * 3 + 1], datap[i * 3 + 2]};
            }
            saw_PLTE = true;
        }
        else if (type[0] == 't' && type[1] == 'R' && type[2] == 'N' && type[3] == 'S') {
            // For indexed-color, tRNS is K alpha bytes for the first K palette entries.
            if (len > 0) {
                out.colors.resize(len);
                for (std::size_t i = 0; i < len; ++i) {
                    out.colors[i].a = datap[i];
                }
            }
        }
        else if (type[0] == 'I' && type[1] == 'E' && type[2] == 'N' && type[3] == 'D') {
            break;
        }
        else {
            // skip other chunks
        }
    }

    detail::ensure(saw_PLTE, "No PLTE chunk found");

    return out;
}

inline Palette extractPngPalette(const ghc::filesystem::path& path)
{
    ghc::filesystem::ifstream f(path, std::ios::binary);
    detail::ensure(!!f, "Failed to open file: " + path.string());
    std::vector<std::uint8_t> buf(ghc::filesystem::file_size(path));
    if (!f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size())))
        throw std::runtime_error("Failed to read file: " + path.string());
    return extractPngPalette(std::span<const std::uint8_t>(buf.data(), buf.size()));
}

struct BmpPalette
{
    // B,G,R triples as you probably want them in RGB order:
    std::vector<std::array<std::uint8_t, 3>> rgb;
    // Optional per-entry 4th byte from RGBQUAD (usually 0; meaningful in V4/V5).
    // For OS/2 CORE (12-byte DIB), there is no 4th byte, so this stays std::nullopt.
    std::optional<std::vector<std::uint8_t>> alpha_or_reserved;
    std::uint16_t bit_count = 0;    // from DIB
    std::uint32_t colors_used = 0;  // from DIB (may be 0)
};

inline Palette extractBmpPalette(std::span<const std::uint8_t> bytes)
{
    using namespace detail;
    if (bytes.size() < 14)
        fail("BMP too small for file header");

    // --- BITMAPFILEHEADER (14 bytes) ---
    if (!(bytes[0] == 'B' && bytes[1] == 'M'))
        fail("Not a BMP (missing 'BM')");
    const std::uint32_t bfSize = readU32LE(bytes.data() + 2);
    const std::uint32_t bfOffBits = readU32LE(bytes.data() + 10);
    (void)bfSize;  // not strictly needed; we trust the buffer we got

    // --- DIB header starts at offset 14 ---
    if (bytes.size() < 18)
        fail("BMP missing DIB header size");
    const std::uint32_t dibSize = readU32LE(bytes.data() + 14);
    if (bytes.size() < 14 + dibSize)
        fail("Truncated DIB header");

    BmpPalette out{};
    std::size_t palette_offset = 14 + dibSize;
    std::uint32_t colors_used = 0;
    std::uint16_t bit_count = 0;
    bool use_rgbquad = true;  // 4-byte palette entries unless OS/2 CORE

    // --- Parse DIB header variants ---
    if (dibSize == 12) {
        // OS/2 BITMAPCOREHEADER
        if (bytes.size() < 26)
            fail("CORE header truncated");
        // width (u16) + height (u16) + planes (u16) + bitcount(u16)
        bit_count = readU16LE(bytes.data() + 24);
        // No colorsUsed field; derive below. Palette entries are 3-byte BGR (no reserved).
        use_rgbquad = false;
        // palette follows immediately after the 12-byte header
        palette_offset = 14 + 12;
    }
    else if (dibSize >= 40) {
        // Windows BITMAPINFOHEADER (40) or V4 (108) or V5 (124)
        if (bytes.size() < 14 + 40)
            fail("INFO header truncated");
        bit_count = readU16LE(bytes.data() + 14 + 14);    // offset to biBitCount within 40-byte header
        colors_used = readU32LE(bytes.data() + 14 + 32);  // biClrUsed at +32
        // If dibSize == 40 and compression is BI_BITFIELDS (3) or ALPHABITFIELDS (6),
        // the 3 or 4 masks immediately follow the 40-byte header and precede the palette.
        if (dibSize == 40) {
            const std::uint32_t compression = readU32LE(bytes.data() + 14 + 16);
            if (compression == 3 /*BI_BITFIELDS*/ || compression == 6 /*BI_ALPHABITFIELDS*/) {
                // 3 masks (RGB) -> +12; ALPHABITFIELDS usually adds an alpha mask -> +16
                // Many encoders still use 3 masks even with 6; be conservative and use 16 if ALPHA.
                palette_offset += (compression == 6 ? 16 : 12);
                if (bytes.size() < palette_offset)
                    fail("Truncated bitfields");
            }
        }
        // For V4/V5, masks/CS are inside the DIB block, so palette still starts at 14 + dibSize.
        use_rgbquad = true;
    }
    else {
        fail("Unsupported DIB header size");
    }

    // --- Determine palette length ---
    std::uint32_t palette_entries = 0;
    if (bit_count <= 8) {
        palette_entries = colors_used ? colors_used : (1u << bit_count);
    }
    else {
        // For high bit depths, palette is optional. If colors_used > 0, we honor it.
        palette_entries = colors_used;
    }
    if (palette_entries == 0)
        detail::fail("No palette in BMP");

    // Guard against bogus offsets
    if (palette_offset > bytes.size())
        fail("Invalid palette offset");
    // Also ensure we don't read past bfOffBits (palette lives before pixel array).
    const std::size_t max_palette_end = (bfOffBits <= bytes.size()) ? bfOffBits : bytes.size();

    Palette result;
    // result.bit_count = bit_count;
    // result.colors_used = colors_used;

    if (use_rgbquad) {
        // 4-byte entries: B, G, R, A/Reserved
        const std::size_t need = static_cast<std::size_t>(palette_entries) * 4;
        if (palette_offset + need > max_palette_end)
            fail("Palette extends past pixel data");
        result.colors.resize(palette_entries);
        const std::uint8_t* p = bytes.data() + palette_offset;
        for (std::uint32_t i = 0; i < palette_entries; ++i) {
            const std::uint8_t b = p[i * 4 + 0];
            const std::uint8_t g = p[i * 4 + 1];
            const std::uint8_t r = p[i * 4 + 2];
            const std::uint8_t a = p[i * 4 + 3];  // often 0 or 0xFF; meaningful in V4/V5
            result.colors[i] = {r, g, b, a};
        }
    }
    else {
        // 3-byte entries (OS/2 CORE): B, G, R
        const std::size_t need = static_cast<std::size_t>(palette_entries) * 3;
        if (palette_offset + need > max_palette_end)
            fail("Palette extends past pixel data");
        result.colors.resize(palette_entries);
        const std::uint8_t* p = bytes.data() + palette_offset;
        for (std::uint32_t i = 0; i < palette_entries; ++i) {
            const std::uint8_t b = p[i * 3 + 0];
            const std::uint8_t g = p[i * 3 + 1];
            const std::uint8_t r = p[i * 3 + 2];
            result.colors[i] = {r, g, b};
        }
    }

    return result;
}

inline Palette extractBmpPalette(const ghc::filesystem::path& path)
{
    ghc::filesystem::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("Failed to open file: " + path.string());
    const auto sz = ghc::filesystem::file_size(path);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
    if (!f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size())))
        throw std::runtime_error("Failed to read file: " + path.string());
    detail::ensure(f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size())), "Failed to read file: " + path.string());
    return extractBmpPalette(std::span<const std::uint8_t>(buf.data(), buf.size()));
}

// Pack/unpack RGBA as 0xAABBGGRR (or any consistent order)
constexpr uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (static_cast<uint32_t>(r) << 24) | (static_cast<uint32_t>(g) << 16) | (static_cast<uint32_t>(b) << 8) | static_cast<uint32_t>(a);
}

constexpr Color unpackRGBA(uint32_t p)
{
    Color c;
    c.a = static_cast<uint8_t>(p & 0xFF);
    c.b = static_cast<uint8_t>((p >> 8) & 0xFF);
    c.g = static_cast<uint8_t>((p >> 16) & 0xFF);
    c.r = static_cast<uint8_t>((p >> 24) & 0xFF);
    return c;
}

// Returns a palette with palette[0] = fully transparent (0,0,0,0),
// followed by up to 255 most common *non-fully-transparent* colors.
inline std::vector<Color> findMostCommonColors(const std::span<const uint8_t> rgba)
{
    std::vector<Color> palette;
    palette.reserve(256);
    palette.push_back(Color{0, 0, 0, 0});  // index 0 reserved for fully transparent

    if (rgba.empty())
        return palette;

    const size_t numPixel = rgba.size() / 4;

    std::unordered_map<uint32_t, uint32_t> hist;
    hist.reserve(numPixel);
    for (size_t i = 0; i < numPixel; ++i) {
        const uint8_t r = rgba[4 * i + 0];
        const uint8_t g = rgba[4 * i + 1];
        const uint8_t b = rgba[4 * i + 2];
        const uint8_t a = rgba[4 * i + 3];
        const uint32_t key = packRGBA(r, g, b, a);
        ++hist[key];
    }

    std::vector<std::pair<uint32_t, uint32_t>> counts;
    counts.reserve(hist.size());
    for (const auto& kv : hist) {
        if ((kv.first & 0xFF) == 0)
            continue;  // ignore fully transparent colors
        counts.push_back(kv);
    }
    std::ranges::sort(counts, [](const auto& a, const auto& b) { return a.second > b.second; });

    const size_t take = std::min<size_t>(counts.size(), 255);
    for (size_t i = 0; i < take; ++i) {
        palette.push_back(unpackRGBA(counts[i].first));
    }

    return palette;
}

}  // namespace img
