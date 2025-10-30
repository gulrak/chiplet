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

#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <span>

namespace img {

struct Color {
    uint8_t r{}, g{}, b{}, a{0xff};
};

static inline uint8_t clamp_u8(double v) {
    if (v < 0.0) return 0;
    if (v > 255.0) return 255;
    return static_cast<uint8_t>(v);
}

inline std::pair<size_t, Color> nearestColor(const std::array<double, 3>& test, uint8_t alpha, std::span<const Color> palette) {
    double bestDist = std::numeric_limits<double>::infinity();
    const bool transparence = (!palette.empty() && palette[0].a == 0);
    Color best{};
    size_t idx = 0;
    size_t bestIdx = 0;
    if (transparence) {
        palette = palette.subspan(1);
        ++idx;
        if (alpha == 0) {
            return {0, {0,0,0,0}};
        }
    }
    for (const auto& col : palette) {
        const double rmean = (test[0] + static_cast<double>(col.r)) / 2.0;
        const double r = test[0] - static_cast<double>(col.r);
        const double g = test[1] - static_cast<double>(col.g);
        const double b = test[2] - static_cast<double>(col.b);

        // Approximation from https://stackoverflow.com/questions/2103368/color-logic-algorithm
        const double dist = std::sqrt(
            (((512.0 + rmean) * r * r) / 256.0) +
            (4.0 * g * g) +
            (((767.0 - rmean) * b * b) / 256.0)
        );

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
inline std::vector<uint8_t> threshold(uint8_t* pixels, int width, int height, std::span<const Color> palette) {
    std::vector<uint8_t> result;
    result.reserve(width * height);
    const int stride = width * 4;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int index = y * stride + x * 4;

            auto [newIdx, newColor] = nearestColor(
                { static_cast<double>(pixels[index + 0]),
                  static_cast<double>(pixels[index + 1]),
                  static_cast<double>(pixels[index + 2]) },
                  pixels[index + 3],
                palette
            );

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
inline std::vector<uint8_t> dither(uint8_t* pixels, int width, int height, std::span<const Color> palette) {
    std::vector<uint8_t> result;
    result.reserve(width * height);
    const int stride = width * 4;
    const size_t N = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;

    // Work in doubles to preserve propagated error
    std::vector<double> buf(N);
    for (size_t i = 0; i < N; ++i) buf[i] = static_cast<double>(pixels[i]);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = y * stride + x * 4;

            auto [newIdx, newCol] = nearestColor(
                { buf[index + 0], buf[index + 1], buf[index + 2] },
                buf[index + 3],
                palette
            );
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

            if (y == height - 1) continue;
            index += stride; // move to next row

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
        pixels[i + 0] = clamp_u8(buf[i + 0]);
        pixels[i + 1] = clamp_u8(buf[i + 1]);
        pixels[i + 2] = clamp_u8(buf[i + 2]);
        // keep original alpha from input
        // If you prefer propagated alpha, clamp buf[i+3] instead:
        // pixels[i + 3] = clamp_u8(buf[i + 3]);
    }
    return result;
}

}
