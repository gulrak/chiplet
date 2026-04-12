//---------------------------------------------------------------------------------------
// hex2png.cpp
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

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <nothings/stb_image_write.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static inline std::string trim(std::string s) {
    auto not_space = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

static inline bool is_hex_digit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static bool is_hex_byte_token(std::string_view tok) {
    // Allow trailing comma.
    if (!tok.empty() && tok.back() == ',') tok.remove_suffix(1);

    if (tok.size() != 4) return false;
    if (!(tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X'))) return false;
    return is_hex_digit(tok[2]) && is_hex_digit(tok[3]);
}

static uint8_t parse_hex_byte(std::string tok) {
    if (!tok.empty() && tok.back() == ',') tok.pop_back();
    if (!is_hex_byte_token(tok)) {
        throw std::runtime_error("Expected hex byte like 0x00, got: " + tok);
    }
    // Guaranteed 0xYY
    int value = std::stoi(tok, nullptr, 16);
    if (value < 0 || value > 255) {
        throw std::runtime_error("Hex byte out of range: " + tok);
    }
    return static_cast<uint8_t>(value);
}

struct Parsed {
    std::vector<uint8_t> palette_bytes; // ABGR flat bytes; colors start at index 1
    int width{};
    int height{};
    std::vector<uint8_t> indices;       // size = width * height
};

static Parsed parse_input(std::istream& in) {
    // Read entire file, strip comments starting with '#'
    std::ostringstream oss;
    std::string line;
    while (std::getline(in, line)) {
        auto pos = line.find('#');
        if (pos != std::string::npos) line.erase(pos);
        oss << line << '\n';
    }
    std::string content = oss.str();

    // Tokenize by whitespace
    std::istringstream iss(content);
    std::vector<std::string> tokens;
    for (std::string t; iss >> t; ) {
        if (!t.empty()) tokens.push_back(t);
    }
    if (tokens.empty()) throw std::runtime_error("Input is empty.");

    size_t i = 0;
    const size_t n = tokens.size();

    // 1) Palette hex bytes (multiple of 4, ABGR) until a non-hex-byte token
    std::vector<uint8_t> palette_bytes;
    while (i < n && is_hex_byte_token(tokens[i])) {
        palette_bytes.push_back(parse_hex_byte(tokens[i]));
        ++i;
    }
    if (palette_bytes.size() % 4 != 0) {
        throw std::runtime_error("Palette byte count must be a multiple of 4 (ABGR per color).");
    }

    // 2) width height (decimal)
    if (i + 1 >= n) {
        throw std::runtime_error("Missing width and height after palette.");
    }
    int width = 0, height = 0;
    try {
        width  = std::stoi(tokens[i]);
        height = std::stoi(tokens[i + 1]);
    } catch (...) {
        throw std::runtime_error("Expected decimal width and height, got: '" + tokens[i] + "' and '" + tokens[i+1] + "'");
    }
    i += 2;
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Width and height must be positive integers.");
    }

    // 3) width*height hex bytes = indices
    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (n - i < expected) {
        throw std::runtime_error("Not enough pixel indices: expected " + std::to_string(expected) +
                                 ", found " + std::to_string(n - i));
    }
    std::vector<uint8_t> indices;
    indices.reserve(expected);
    for (size_t k = 0; k < expected; ++k) {
        indices.push_back(parse_hex_byte(tokens[i + k]));
    }

    return Parsed{ std::move(palette_bytes), width, height, std::move(indices) };
}

static std::vector<uint8_t> abgr_palette_to_rgba(const std::vector<uint8_t>& abgr_flat) {
    // Index 0 is implicit transparent black (0,0,0,0)
    std::vector<uint8_t> rgba;
    rgba.reserve((abgr_flat.size() / 4 + 1) * 4);
    // implicit index 0
    rgba.push_back(0); rgba.push_back(0); rgba.push_back(0); rgba.push_back(0);

    for (size_t j = 0; j + 3 < abgr_flat.size(); j += 4) {
        uint8_t A = abgr_flat[j + 0];
        uint8_t R = abgr_flat[j + 1];
        uint8_t G = abgr_flat[j + 2];
        uint8_t B = abgr_flat[j + 3];
        std::cout << " " << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(R) << std::setw(2) << static_cast<int>(G) << std::setw(2) << static_cast<int>(B);
        rgba.push_back(R);
        rgba.push_back(G);
        rgba.push_back(B);
        rgba.push_back(A);
        
    }
    std::cout << std::endl;
    return rgba; // flat RGBA entries for indices 0..N
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.txt> <output.png>\n";
        return 1;
    }
    const std::string in_path = argv[1];
    const std::string out_path = argv[2];

    try {
        std::ifstream fin(in_path);
        if (!fin) {
            std::cerr << "Failed to open input file: " << in_path << "\n";
            return 1;
        }

        Parsed p = parse_input(fin);

        // Convert palette: ABGR -> RGBA (with implicit index 0)
        std::vector<uint8_t> palette_rgba = abgr_palette_to_rgba(p.palette_bytes);
        const int max_index = static_cast<int>(palette_rgba.size() / 4) - 1; // excluding implicit? palette has index 0 included
        // Validate indices
        for (size_t k = 0; k < p.indices.size(); ++k) {
            if (p.indices[k] > max_index) {
                std::ostringstream msg;
                msg << "Pixel index " << static_cast<int>(p.indices[k])
                    << " out of range (0.." << max_index << ") at position " << k << ".";
                throw std::runtime_error(msg.str());
            }
        }

        // Build RGBA pixel buffer
        const size_t pixel_count = static_cast<size_t>(p.width) * static_cast<size_t>(p.height);
        std::vector<uint8_t> pixels;
        pixels.resize(pixel_count * 4);
        for (size_t k = 0; k < pixel_count; ++k) {
            const uint8_t idx = p.indices[k];
            const size_t src = static_cast<size_t>(idx) * 4;
            const size_t dst = k * 4;
            pixels[dst + 0] = palette_rgba[src + 0];
            pixels[dst + 1] = palette_rgba[src + 1];
            pixels[dst + 2] = palette_rgba[src + 2];
            pixels[dst + 3] = palette_rgba[src + 3];
        }

        // Write PNG
        const int stride_in_bytes = p.width * 4;
        if (!stbi_write_png(out_path.c_str(), p.width, p.height, 4, pixels.data(), stride_in_bytes)) {
            std::cerr << "stbi_write_png failed for: " << out_path << "\n";
            return 1;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
