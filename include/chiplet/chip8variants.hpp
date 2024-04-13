//---------------------------------------------------------------------------------------
// src/emulation/chip8variants.hpp
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
#include <type_traits>
#include <initializer_list>
#include <array>

namespace emu {

template<typename EnumT, size_t N = 64>
class EnumSet {
    using ArrayT = std::array<uint64_t,(N+63)/64>;
public:
    using value_type = EnumT;
    constexpr EnumSet(EnumT val) {
        //static_assert(static_cast<size_t>(val) < N);
        set(static_cast<size_t>(val));
    }
    constexpr EnumSet(std::initializer_list<EnumT> init)
    {
        for (const auto i : init) {
            set(static_cast<std::size_t>(i));
        }
    }
    constexpr bool contains(const EnumT e) {
        auto idx = static_cast<size_t>(e);
        return _bits[idx>>6] & (1ull << (idx & 0x3f));
    }
    constexpr EnumSet& operator&=(const EnumSet& other) noexcept
    {
        for(auto [iter, oiter] = std::make_pair(_bits.begin(), other._bits.cbegin()); iter != _bits.end(); ++iter, ++oiter) {
            *iter &= *oiter;
        }
        return *this;
    }
    constexpr EnumSet& operator|=(const EnumSet& other) noexcept
    {
        for(auto [iter, oiter] = std::make_pair(_bits.begin(), other._bits.cbegin()); iter != _bits.end(); ++iter, ++oiter) {
            *iter |= *oiter;
        }
        return *this;
    }
    constexpr EnumSet& operator&=(const EnumT& other) noexcept
    {
        if(contains(other)) {
            reset();
            set(static_cast<size_t>(other));
        }
        else {
            reset();
        }
        return *this;
    }
    constexpr EnumSet& operator|=(const EnumT& other) noexcept
    {
        set(static_cast<size_t>(other));
        return *this;
    }
    friend constexpr EnumSet& operator&(const EnumT e1, const EnumT e2) {
        EnumSet result = e1;
        result &= e2;
        return result;
    }
    friend constexpr EnumSet& operator|(const EnumT e1, const EnumT e2) {
        EnumSet result = e1;
        result |= e2;
        return result;
    }
    constexpr bool is_empty() const {
        for(const auto& v : _bits) {
            if(v)
                return false;
        }
        return true;
    }
    constexpr EnumT first() const {
        int offset = 0;
        for(const auto& v : _bits) {
            if(v) return static_cast<EnumT>(ctz(v) + offset);
            offset += 64;
        }
        return static_cast<EnumT>(offset);
    }
    constexpr uint64_t value() const {
        return _bits.front();
    }
private:
    constexpr void set(size_t idx, bool val = true) {
        if(val)
            _bits[idx>>6] |= 1ull << (idx & 0x3f);
        else
            _bits[idx>>6] &= ~(1ull << (idx & 0x3f));
    }
    constexpr void reset(size_t idx) {
        _bits[idx>>6] &= ~(1ull << (idx & 0x3f));
    }
    constexpr void reset() {
        for(auto& v : _bits) v = 0;
    }
    constexpr static int ctz(uint64_t n) {
        int c = 63;
        n &= ~n + 1;
        if (n & 0x00000000FFFFFFFF) c -= 32;
        if (n & 0x0000FFFF0000FFFF) c -= 16;
        if (n & 0x00FF00FF00FF00FF) c -= 8;
        if (n & 0x0F0F0F0F0F0F0F0F) c -= 4;
        if (n & 0x3333333333333333) c -= 2;
        if (n & 0x5555555555555555) c -= 1;
        return c;
    }
    std::array<uint64_t,(N+63)/64> _bits{};
};

namespace chip8 {
enum class Variant {
    // based on https://chip-8.github.io/extensions
    CHIP_8 = 0x01,             // CHIP-8
    CHIP_8_1_2 = 0x02,         // CHIP-8 1/2
    CHIP_8_I = 0x03,           // CHIP-8I
    CHIP_8_II = 0x04,          // CHIP-8 II aka. Keyboard Kontrol
    CHIP_8_III = 0x05,         // CHIP-8III
    CHIP_8_TPD = 0x06,         // Two-page display for CHIP-8
    CHIP_8C = 0x07,            // CHIP-8C
    CHIP_10 = 0x08,            // CHIP-10
    CHIP_8_SRV = 0x09,         // CHIP-8 modification for saving and restoring variables
    CHIP_8_SRV_I = 0x0A,       // Improved CHIP-8 modification for saving and restoring variables
    CHIP_8_RB = 0x0B,          // CHIP-8 modification with relative branching
    CHIP_8_ARB = 0x0C,         // Another CHIP-8 modification with relative branching
    CHIP_8_FSD = 0x0D,         // CHIP-8 modification with fast, single-dot DXYN
    CHIP_8_IOPD = 0x0E,        // CHIP-8 with I/O port driver routine
    CHIP_8_8BMD = 0x0F,        // CHIP-8 8-bit multiply and divide
    HI_RES_CHIP_8 = 0x10,      // HI-RES CHIP-8 (four-page display)
    HI_RES_CHIP_8_IO = 0x11,   // HI-RES CHIP-8 with I/O
    HI_RES_CHIP_8_PS = 0x12,   // HI-RES CHIP-8 with page switching
    CHIP_8E = 0x13,            // CHIP-8E
    CHIP_8_IBNNN = 0x14,       // CHIP-8 with improved BNNN
    CHIP_8_SCROLL = 0x15,      // CHIP-8 scrolling routine
    CHIP_8X = 0x16,            // CHIP-8X
    CHIP_8X_TPD = 0x17,        // Two-page display for CHIP-8X
    HI_RES_CHIP_8X = 0x18,     // Hi-res CHIP-8X
    CHIP_8Y = 0x19,            // CHIP-8Y
    CHIP_8_CtS = 0x1A,         // CHIP-8 “Copy to Screen”
    CHIP_BETA = 0x1B,          // CHIP-BETA
    CHIP_8M = 0x1C,            // CHIP-8M
    MULTIPLE_NIM = 0x1D,       // Multiple Nim interpreter
    DOUBLE_ARRAY_MOD = 0x1E,   // Double Array Modification
    CHIP_8_D6800 = 0x1F,       // CHIP-8 for DREAM 6800 (CHIPOS)
    CHIP_8_D6800_LOP = 0x20,   // CHIP-8 with logical operators for DREAM 6800 (CHIPOSLO)
    CHIP_8_D6800_JOY = 0x21,   // CHIP-8 for DREAM 6800 with joystick
    C8_2K_CHIPOS_D6800 = 0x22, // 2K CHIPOS for DREAM 6800
    CHIP_8_ETI660 = 0x23,      // CHIP-8 for ETI-660
    CHIP_8_ETI660_COL = 0x24,  // CHIP-8 with color support for ETI-660
    CHIP_8_ETI660_HR = 0x25,   // CHIP-8 for ETI-660 with high resolution
    CHIP_8_COSMAC_ELF = 0x26,  // CHIP-8 for COSMAC ELF
    CHIP_8_ACE_VDU = 0x27,     // CHIP-VDU / CHIP-8 for the ACE VDU
    CHIP_8_AE = 0x28,          // CHIP-8 AE (ACE Extended)
    CHIP_8_DC_V2 = 0x29,       // Dreamcards Extended CHIP-8 V2.0
    CHIP_8_AMIGA = 0x2A,       // Amiga CHIP-8 interpreter
    CHIP_48 = 0x2B,            // CHIP-48
    SCHIP_1_0 = 0x2C,          // SUPER-CHIP 1.0
    SCHIP_1_1 = 0x2D,          // SUPER-CHIP 1.1
    GCHIP = 0x2E,              // GCHIP
    SCHIPC = 0x2F,             // SCHIP Compatibility (SCHPC) and GCHIP Compatibility (GCHPC)
    SCHIPC_GCHIPC = 0x2F,      // SCHIP Compatibility (SCHPC) and GCHIP Compatibility (GCHPC)
    VIP2K_CHIP_8 = 0x30,       // VIP2K CHIP-8
    SCHIP_1_1_SCRUP = 0x31,    // SUPER-CHIP with scroll up
    CHIP8RUN = 0x32,           // chip8run
    MEGA_CHIP = 0x33,          // Mega-Chip
    XO_CHIP = 0x34,            // XO-CHIP
    OCTO = 0x35,               // Octo
    CHIP_8_CL_COL = 0x36,      // CHIP-8 Classic / Color

    COSMAC_VIP = 59,           // A pure COSMAC VIP without CHIP-8
    CHIP_8_COSMAC_VIP = 60,    // CHIP-8 on emulated COSMAC VIP
    CHIP_8_TDP_COSMAC_VIP = 61,// CHIP-8 Two Page Display on emulated COSMAC VIP
    GENERIC_CHIP_8 = 63        // A universal program, don't switch current emulation
};

using VariantSet = EnumSet<Variant>;

constexpr VariantSet operator&(const Variant e1, const Variant e2) {
    VariantSet vs = e1;
    vs |= e2;
    return vs;
}
constexpr VariantSet operator|(const Variant e1, const Variant e2) {
    VariantSet result = e1;
    result |= e2;
    return result;
}

static const VariantSet mix = Variant::CHIP_10 | Variant::CHIP_48;
}


enum class Chip8Variant : uint64_t {
    CHIP_8 = 0x01,                      // CHIP-8
    CHIP_8_1_2 = 0x02,                  // CHIP-8 1/2
    CHIP_8_I = 0x04,                    // CHIP-8I
    CHIP_8_II = 0x08,                   // CHIP-8 II aka. Keyboard Kontrol
    CHIP_8_III = 0x10,                  // CHIP-8III
    CHIP_8_TPD = 0x20,                  // Two-page display for CHIP-8
    CHIP_8C = 0x40,                     // CHIP-8C
    CHIP_10 = 0x80,                     // CHIP-10
    CHIP_8_SRV = 0x100,                 // CHIP-8 modification for saving and restoring variables
    CHIP_8_SRV_I = 0x200,               // Improved CHIP-8 modification for saving and restoring variables
    CHIP_8_RB = 0x400,                  // CHIP-8 modification with relative branching
    CHIP_8_ARB = 0x800,                 // Another CHIP-8 modification with relative branching
    CHIP_8_FSD = 0x1000,                // CHIP-8 modification with fast, single-dot DXYN
    CHIP_8_IOPD = 0x2000,               // CHIP-8 with I/O port driver routine
    CHIP_8_8BMD = 0x4000,               // CHIP-8 8-bit multiply and divide
    HI_RES_CHIP_8 = 0x8000,             // HI-RES CHIP-8 (four-page display)
    HI_RES_CHIP_8_IO = 0x10000,         // HI-RES CHIP-8 with I/O
    HI_RES_CHIP_8_PS = 0x20000,         // HI-RES CHIP-8 with page switching
    CHIP_8E = 0x40000,                  // CHIP-8E
    CHIP_8_IBNNN = 0x80000,             // CHIP-8 with improved BNNN
    CHIP_8_SCROLL = 0x100000,           // CHIP-8 scrolling routine
    CHIP_8X = 0x200000,                 // CHIP-8X
    CHIP_8X_TPD = 0x400000,             // Two-page display for CHIP-8X
    HI_RES_CHIP_8X = 0x800000,          // Hi-res CHIP-8X
    CHIP_8Y = 0x1000000,                // CHIP-8Y
    CHIP_8_CtS = 0x2000000,             // CHIP-8 “Copy to Screen”
    CHIP_BETA = 0x4000000,              // CHIP-BETA
    CHIP_8M = 0x8000000,                // CHIP-8M
    MULTIPLE_NIM = 0x10000000,          // Multiple Nim interpreter
    DOUBLE_ARRAY_MOD = 0x20000000,      // Double Array Modification
    CHIP_8_D6800 = 0x40000000,          // CHIP-8 for DREAM 6800 (CHIPOS)
    CHIP_8_D6800_LOP = 0x80000000,      // CHIP-8 with logical operators for DREAM 6800 (CHIPOSLO)
    CHIP_8_D6800_JOY = 0x100000000,     // CHIP-8 for DREAM 6800 with joystick
    CHIPOS_2K_D6800 = 0x200000000,      // 2K CHIPOS for DREAM 6800
    CHIP_8_ETI660 = 0x400000000,        // CHIP-8 for ETI-660
    CHIP_8_ETI660_COL = 0x800000000,    // CHIP-8 with color support for ETI-660
    CHIP_8_ETI660_HR = 0x1000000000,    // CHIP-8 for ETI-660 with high resolution
    CHIP_8_COSMAC_ELF = 0x2000000000,   // CHIP-8 for COSMAC ELF
    CHIP_8_ACE_VDU = 0x4000000000,      // CHIP-VDU / CHIP-8 for the ACE VDU
    CHIP_8_AE = 0x8000000000,           // CHIP-8 AE (ACE Extended)
    CHIP_8_DC_V2 = 0x10000000000,       // Dreamcards Extended CHIP-8 V2.0
    CHIP_8_AMIGA = 0x20000000000,       // Amiga CHIP-8 interpreter
    CHIP_48 = 0x40000000000,            // CHIP-48
    SCHIP_1_0 = 0x80000000000,          // SUPER-CHIP 1.0
    SCHIP_1_1 = 0x100000000000,         // SUPER-CHIP 1.1
    GCHIP = 0x200000000000,             // GCHIP
    SCHIPC = 0x400000000000,            // SCHIP Compatibility (SCHPC) and GCHIP Compatibility (GCHPC)
    SCHIPC_GCHIPC = 0x400000000000,     // SCHIP Compatibility (SCHPC) and GCHIP Compatibility (GCHPC)
    VIP2K_CHIP_8 = 0x800000000000,      // VIP2K CHIP-8
    SCHIP_1_1_SCRUP = 0x1000000000000,  // SUPER-CHIP with scroll up
    CHIP8RUN = 0x2000000000000,         // chip8run
    MEGA_CHIP = 0x4000000000000,        // Mega-Chip
    XO_CHIP = 0x8000000000000,          // XO-CHIP
    OCTO = 0x10000000000000,            // Octo
    CHIP_8_CL_COL = 0x20000000000000,   // CHIP-8 Classic / Color

    NUM_VARIANTS
};

namespace detail {
template <typename Enum>
using EnableBitmask = typename std::enable_if<std::is_same<Enum, Chip8Variant>::value, Enum>::type;
}  // namespace detail

template <typename Enum>
constexpr detail::EnableBitmask<Enum> operator&(Enum X, Enum Y)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(X) & static_cast<underlying>(Y));
}

template <typename Enum>
constexpr detail::EnableBitmask<Enum> operator|(Enum X, Enum Y)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(X) | static_cast<underlying>(Y));
}

template <typename Enum>
constexpr detail::EnableBitmask<Enum> operator^(Enum X, Enum Y)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(X) ^ static_cast<underlying>(Y));
}

template <typename Enum>
constexpr detail::EnableBitmask<Enum> operator~(Enum X)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(~static_cast<underlying>(X));
}

template <typename Enum>
constexpr detail::EnableBitmask<Enum>& operator&=(Enum& X, Enum Y)
{
    X = X & Y;
    return X;
}

template <typename Enum>
constexpr detail::EnableBitmask<Enum>& operator|=(Enum& X, Enum Y)
{
    X = X | Y;
    return X;
}

template <typename Enum>
constexpr detail::EnableBitmask<Enum>& operator^=(Enum& X, Enum Y)
{
    X = X ^ Y;
    return X;
}

using C8V = Chip8Variant;

static constexpr Chip8Variant C8VG_BASE = static_cast<Chip8Variant>(0x3FFFFFFFFFFFFF) & ~(C8V::CHIP_8_1_2 | C8V::CHIP_8C | C8V::CHIP_8_SCROLL | C8V::MULTIPLE_NIM);
static constexpr Chip8Variant C8VG_D6800 = C8V::CHIP_8_D6800 | C8V::CHIP_8_D6800_LOP | C8V::CHIP_8_D6800_JOY | C8V::CHIPOS_2K_D6800;

inline bool contained(Chip8Variant variants, Chip8Variant subset)
{
    return (variants & subset) == subset;
}

inline bool containedAny(Chip8Variant variants, Chip8Variant subset)
{
    return uint64_t(variants & subset) != 0;
}

} // namespace emu
