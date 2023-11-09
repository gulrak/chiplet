//---------------------------------------------------------------------------------------
// src/emulation/chip8meta.hpp
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

#include <chiplet/chip8variants.hpp>
#include <fmt/format.h>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <bitset>

namespace emu {

enum OpcodeType { OT_FFFF, OT_FFFn, OT_FFnn, OT_Fnnn, OT_FxyF, OT_FxFF, OT_Fxyn, OT_Fxnn, OT_FFyF, NUM_OPCODE_TYPES};

struct OpcodeInfo {
    OpcodeType type;
    uint16_t opcode;
    int size;
    std::string mnemonic;
    std::string octo;
    Chip8Variant variants;
    std::string description;
};

namespace detail {
// clang-format off
inline static std::array<uint16_t, NUM_OPCODE_TYPES> opcodeMasks = { 0xFFFF, 0xFFF0, 0xFF00, 0xF000, 0xF00F, 0xF0FF, 0xF000, 0xF000, 0xFF0F };
inline static std::vector<OpcodeInfo> opcodes{
    { OT_FFFF, 0x0010, 2, "megaoff", "megaoff", C8V::MEGA_CHIP, "disable megachip mode" },
    { OT_FFFF, 0x0011, 2, "megaon", "megaon", C8V::MEGA_CHIP, "enable megachip mode" },
    { OT_FFFn, 0x00B0, 2, "scru N", "scroll_up N", C8V::SCHIP_1_1_SCRUP|C8V::MEGA_CHIP, "scroll screen content up N pixel [Q: On the HP48 (SCHIP/SCHIPC) scrolling in lores mode only scrolls half the pixels]" },
    { OT_FFFn, 0x00C0, 2, "scd N", "scroll-down N", C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::SCHIPC|C8V::MEGA_CHIP|C8V::XO_CHIP|C8V::OCTO, "scroll screen content down N pixel, in XO-CHIP only selected bit planes are scrolled [Q: On the HP48 (SCHIP/SCHIPC) scrolling in lores mode only scrolls half the pixels][Q: On the HP48 (SCHIP/SCHPC) opcode 00C0, so scrolling zero pixels, is not a valid opcode]" },
    { OT_FFFn, 0x00D0, 2, "scu N", "scroll-up N", C8V::XO_CHIP|C8V::OCTO, "scroll screen content up N hires pixel, in XO-CHIP only selected planes are scrolled" },
    { OT_FFFF, 0x00E0, 2, "cls", "clear", C8VG_BASE, "clear the screen, in XO-CHIP only selected bit planes are cleared, in MegaChip mode it updates the visible screen before clearing the draw buffer" },
    { OT_FFFF, 0x00EE, 2, "ret", "return", C8VG_BASE, "return from subroutine to address pulled from stack" },
    { OT_FFFF, 0x00FB, 2, "scr", "scroll-right", C8V::SCHIP_1_1|C8V::SCHIPC|C8V::MEGA_CHIP|C8V::XO_CHIP|C8V::OCTO, "scroll screen content right four pixel, in XO-CHIP only selected bit planes are scrolled [Q: On the HP48 (SCHIP/SCHIPC) scrolling in lores mode only scrolls half the pixels]" },
    { OT_FFFF, 0x00FC, 2, "scl", "scroll-left", C8V::SCHIP_1_1|C8V::SCHIPC|C8V::MEGA_CHIP|C8V::XO_CHIP|C8V::OCTO, "scroll screen content left four pixel, in XO-CHIP only selected bit planes are scrolled [Q: On the HP48 (SCHIP/SCHIPC) scrolling in lores mode only scrolls half the pixels]" },
    { OT_FFFF, 0x00FD, 2, "exit", "exit", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIPC|C8V::XO_CHIP|C8V::MEGA_CHIP|C8V::OCTO, "exit interpreter" },
    { OT_FFFF, 0x00FE, 2, "low", "lores", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIPC|C8V::MEGA_CHIP|C8V::XO_CHIP|C8V::OCTO, "switch to lores mode (64x32) [Q: The original SCHIP-1.x did not clean the screen, leading to artifacts]" },
    { OT_FFFF, 0x00FF, 2, "high", "hires", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIPC|C8V::MEGA_CHIP|C8V::XO_CHIP|C8V::OCTO, "switch to hires mode (128x64) [Q: The original SCHIP-1.x did not clean the screen, leading to artifacts]" },
    { OT_FFFF, 0x00FF, 2, "dw #00ff", "nop", C8V::CHIP_8_ETI660|Chip8Variant::CHIP_8_ETI660_COL|Chip8Variant::CHIP_8_ETI660_HR, "nop (does nothing)" },
    { OT_FFnn, 0x0100, 4, "ldhi i,NNNNNN", "ldhi NNNNNN", C8V::MEGA_CHIP, "set I to NNNNNN (24 bit)" },
    { OT_FFnn, 0x0200, 2, "ldpal NN", "ldpal NN", C8V::MEGA_CHIP, "load NN colors from I into the palette, colors are in ARGB" },
    { OT_FFFF, 0x02A0, 2, "dw #02A0", "cycle-bgcol", C8V::CHIP_8X, "cycle background color one step between blue, black, green and red"},
    { OT_FFFF, 0x02F0, 2, "dw #02F0", "cycle-bgcol-mp", C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X, "cycle background color one step between blue, black, green and red"},
    { OT_FFnn, 0x0300, 2, "sprw NN", "sprw NN", C8V::MEGA_CHIP, "set sprite width to NN (not used for font sprites)" },
    { OT_FFnn, 0x0400, 2, "sprh NN", "sprh NN", C8V::MEGA_CHIP, "set sprite height to NN (not used for font sprites)" },
    { OT_FFnn, 0x0500, 2, "alpha NN", "alpha NN", C8V::MEGA_CHIP, "set screen alpha to NN" },
    { OT_FFFn, 0x0600, 2, "digisnd N", "digisnd N", C8V::MEGA_CHIP, "play digitized sound at I N=loop/noloop" },
    { OT_FFFF, 0x0700, 2, "stopsnd", "stopsnd", C8V::MEGA_CHIP, "stop digitized sound" },
    { OT_FFFn, 0x0800, 2, "bmode N", "bmode N", C8V::MEGA_CHIP, "set sprite blend mode (0=normal,1=25%,2=50%,3=75%,4=additive,5=multiply)" },
    { OT_FFnn, 0x0900, 2, "ccol NN", "ccol NN", C8V::MEGA_CHIP, "set collision color to index NN" },
    { OT_Fnnn, 0x0000, 2, "dw #0NNN", "0x0N 0xNN", static_cast<Chip8Variant>((int64_t)C8V::MULTIPLE_NIM-1)|C8V::CHIP_8_D6800, "jump to native assembler subroutine at 0xNNN"},
    { OT_Fnnn, 0x1000, 2, "jp NNN", "jump NNN", C8VG_BASE, "jump to address NNN" },
    { OT_Fnnn, 0x2000, 2, "call NNN", ":call NNN", C8VG_BASE, "push return address onto stack and call subroutine at address NNN" },
    { OT_Fxnn, 0x3000, 2, "se vX,NN", "if vX != NN then", C8VG_BASE, "skip next opcode if vX == NN (note: on platforms that have 4 byte opcodes, like F000 on XO-CHIP, this needs to skip four bytes)" },
    { OT_Fxnn, 0x4000, 2, "sne vX,NN", "if vX == NN then", C8VG_BASE, "skip next opcode if vX != NN (note: on platforms that have 4 byte opcodes, like F000 on XO-CHIP, this needs to skip four bytes)" },
    { OT_FxyF, 0x5000, 2, "se vX,vY", "if vX != vY then", C8VG_BASE, "skip next opcode if vX == vY (note: on platforms that have 4 byte opcodes, like F000 on XO-CHIP, this needs to skip four bytes)" },
    { OT_FxyF, 0x5001, 2, "dw #5XY1", "0x5X 0xY1", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X, "A BCD like add opcode that works in octal for normal CHIP-8X and hex on multi-page CHIP-8X, add the nibbles of Vx and Vy separately, and mask the results to keep the nibbles addition from overflowing, and store result in vX"},
    { OT_FxyF, 0x5002, 2, "ld [i],vX-vY", "save vX - vY", C8V::XO_CHIP|C8V::OCTO, "write registers vX to vY to memory pointed to by I" },
    { OT_FxyF, 0x5003, 2, "ld vX-vY,[i]", "load vX - vY", C8V::XO_CHIP|C8V::OCTO, "load registers vX to vY from memory pointed to by I" },
    { OT_Fxnn, 0x6000, 2, "ld vX,NN", "vX := NN", C8VG_BASE, "set vX to NN" },
    { OT_Fxnn, 0x7000, 2, "add vX,NN", "vX += NN", C8VG_BASE, "add NN to vX" },
    { OT_FxyF, 0x8000, 2, "ld vX,vY", "vX := vY", C8VG_BASE, "set vX to the value of vY" },
    { OT_FxyF, 0x8001, 2, "or vX,vY", "vX |= vY", C8VG_BASE, "set vX to the result of bitwise vX OR vY [Q: COSMAC based variants will reset VF]" },
    { OT_FxyF, 0x8002, 2, "and vX,vY", "vX &= vY", C8VG_BASE, "set vX to the result of bitwise vX AND vY [Q: COSMAC based variants will reset VF]" },
    { OT_FxyF, 0x8003, 2, "xor vX,vY", "vX ^= vY", C8VG_BASE & ~(C8V::CHIP_8_D6800), "set vX to the result of bitwise vX XOR vY [Q: COSMAC based variants will reset VF]" },
    { OT_FxyF, 0x8004, 2, "add vX,vY", "vX += vY", C8VG_BASE, "add vY to vX, vF is set to 1 if an overflow happened, to 0 if not, even if X=F!" },
    { OT_FxyF, 0x8005, 2, "sub vX,vY", "vX -= vY", C8VG_BASE, "subtract vY from vX, vF is set to 0 if an underflow happened, to 1 if not, even if X=F!" },
    { OT_FxyF, 0x8006, 2, "shr vX{,vY}", "vX >>= vY", C8VG_BASE & ~(C8V::CHIP_8_D6800), "set vX to vY and shift vX one bit to the right, set vF to the bit shifted out, even if X=F! [Q: CHIP-48/SCHIP-1.x don't set vX to vY, so only shift vX]" },
    { OT_FxyF, 0x8007, 2, "subn vX,vY", "vX =- vY", C8VG_BASE & ~(C8V::CHIP_8_D6800), "set vX to the result of subtracting vX from vY, vF is set to 0 if an underflow happened, to 1 if not, even if X=F!" },
    { OT_FxyF, 0x800e, 2, "shl vX{,vY}", "vX <<= vY", C8VG_BASE & ~(C8V::CHIP_8_D6800), "set vX to vY and shift vX one bit to the left, set vF to the bit shifted out, even if X=F! [Q: CHIP-48/SCHIP-1.x don't set vX to vY, so only shift vX]" },
    { OT_FxyF, 0x9000, 2, "sne vX,vY", "if vX == vY then", C8VG_BASE, "skip next opcode if vX != vY (note: on platforms that have 4 byte opcodes, like F000 on XO-CHIP, this needs to skip four bytes)" },
    { OT_Fnnn, 0xA000, 2, "ld i,NNN", "i := NNN", C8VG_BASE, "set I to NNN" },
    { OT_Fnnn, 0xB000, 2, "jp v0,NNN", "jump0 NNN", C8VG_BASE & ~(C8V::CHIP_8_I|C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X|C8V::CHIP_48|C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP), "jump to address NNN + v0" },
    { OT_Fxnn, 0xB000, 2, "jp vX,NNN", "jump0 NNN + vX", C8V::CHIP_48|C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP, "jump to address XNN + vX" },
    { OT_FFnn, 0xB000, 2, "dw #b0NN", "0xb0 0xNN", C8V::CHIP_8_I, "output NN to port"},
    { OT_FFyF, 0xB100, 2, "dw #b1Y0", "0xb1 0xY0", C8V::CHIP_8_I, "output Vy to port"},
    { OT_FFyF, 0xB101, 2, "dw #b1Y1", "0xb1 0xY1", C8V::CHIP_8_I, "wait for input (EF line is low) and set Vy to data from port"},
    { OT_Fxyn, 0xB000, 2, "dw #bXYN", "col-high X Y N", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X, "set the foreground color of the pixel area where VX is the horizontal coordinate and VX+1 is the vertical, for 8 horizontal pixels (similar to DXYN), to the color defined in VY (N > 0), horizontal coordinates are actually seen as VX&0x38"},
    { OT_FxyF, 0xB000, 2, "dw #bXY0", "col-low X Y", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X, "set the foreground color of the pixel area defined by VX and VX+1 to the color defined in VY (VY <= 7, where values correspond to black, red, blue, violet, green, yellow, aqua and white, respectively); the display is split into 8 x 8 zones (8 x 4 pixels each); the least significant nibble of VX specifies the horizontal position of the left-most zone, and the most significant nibble of VX specifies the extra number of horizontal zones to color (ie. a value of 0 will color one zone); ditto for VX+1, but with vertical zones"},
    { OT_Fxnn, 0xC000, 2, "rnd vX,NN", "vX := random NN", C8VG_BASE, "set vx to a random value masked (bitwise AND) with NN" },
    { OT_Fxyn, 0xD000, 2, "drw vX,vY,N", "sprite vX vY N", C8VG_BASE, "draw 8xN pixel sprite at position vX, vY with data starting at the address in I, I is not changed [Q: XO-CHIP wraps pixels instead of clipping them] [Q: Original COSMAC VIP based systems (like original CHIP-8), and the HP48 based interpreters in 64x32 mode wait for the start of the next frame, the VIP sometimes even needs two screens to finish] [Q: CHIP-10 only has a hires mode] [Q: The original SCHIP-1.1 in hires mode set VF to the number of sprite rows with collisions plus the number of rows clipped at the bottom border]" },
    { OT_FxyF, 0xD000, 2, "drw vX,vY,0", "sprite vX vY 0", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::SCHIPC|C8V::XO_CHIP, "draw 16x16 pixel sprite at position vX, vY with data starting at the address in I, I is not changed [Q: XO-CHIP wraps pixels instead of clipping them][Q: SCHIP-1.x only draws 8x16 on lores] [Q: Original COSMAC VIP based systems (like original CHIP-8), and the HP48 based interpreters in 64x32 mode wait for the start of the next frame, the VIP sometimes even needs two screens to finish] [Q: The original SCHIP-1.1 in hires mode set VF to the number of sprite rows with collisions plus the number of rows clipped at the bottom border]" },
    { OT_FxFF, 0xE09E, 2, "skp vX", "if vX -key then", C8VG_BASE, "skip next opcode if key in vX is pressed (note: on platforms that have 4 byte opcodes, like F000 on XO-CHIP, this needs to skip four bytes)" },
    { OT_FxFF, 0xE0A1, 2, "sknp vX", "if vX key then", C8VG_BASE, "skip next opcode if key in vX is not pressed (note: on platforms that have 4 byte opcodes, like F000 on XO-CHIP, this needs to skip four bytes)" },
    { OT_FxFF, 0xE0F2, 2, "dw #eXf2", "0xeX 0xf2", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X, "skip next opcode if key in vX is pressed on keypad 2" },
    { OT_FxFF, 0xE0F5, 2, "dw #eXf5", "0xeX 0xf5", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X, "skip next opcode if key in vX is not pressed keypad 2" },
    { OT_FFFF, 0xF000, 4, "", "i := long NNNN", C8V::XO_CHIP, "assign next 16 bit word to i, and set PC behind it, this is a four byte instruction (see note on skip instructions)" },
    { OT_FxFF, 0xF001, 2, "", "plane X", C8V::XO_CHIP, "select bit planes to draw on when drawing with Dxy0/Dxyn" },
    { OT_FFFF, 0xF002, 2, "", "audio", C8V::XO_CHIP, "load 16 bytes audio pattern pointed to by I into audio pattern buffer" },
    { OT_FxFF, 0xF007, 2, "", "vX := delay", C8VG_BASE, "set vX to the value of the delay timer" },
    { OT_FxFF, 0xF00A, 2, "", "vX := key", C8VG_BASE, "wait for a key pressed and released and set vX to it, in megachip mode it also updates the screen like clear" },
    { OT_FxFF, 0xF015, 2, "", "delay := vX", C8VG_BASE, "set delay timer to vX" },
    { OT_FxFF, 0xF018, 2, "", "buzzer := vX", C8VG_BASE, "set sound timer to vX, sound is played when sound timer is set greater 1 until it is zero" },
    { OT_FxFF, 0xF01E, 2, "", "i += vX", C8VG_BASE, "add vX to I" },
    { OT_FxFF, 0xF029, 2, "", "i := hex vX", C8VG_BASE, "set I to the hex sprite for the lowest nibble in vX" },
    { OT_FxFF, 0xF030, 2, "", "i := bighex vX", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::SCHIPC|C8V::XO_CHIP|C8V::MEGA_CHIP, "set I to the 10 lines height hex sprite for the lowest nibble in vX" },
    { OT_FxFF, 0xF033, 2, "", "bcd vX", C8VG_BASE, "write the value of vX as BCD value at the addresses I, I+1 and I+2" },
    { OT_FxFF, 0xF03A, 2, "", "pitch := vX", C8V::XO_CHIP, "set audio pitch for a audio pattern playback rate of 4000*2^((vX-64)/48)Hz" },
    { OT_FxFF, 0xF055, 2, "", "save vX", C8VG_BASE, "write the content of v0 to vX at the memory pointed to by I, I is incremented by X+1 [Q: CHIP-48/SCHIP1.0 increment I only by X, SCHIP1.1 not at all]" },
    { OT_FxFF, 0xF065, 2, "", "load vX", C8VG_BASE, "read the bytes from memory pointed to by I into the registers v0 to vX, I is incremented by X+1 [Q: CHIP-48/SCHIP1.0 increment I only by X, SCHIP1.1 not at all]" },
    { OT_FxFF, 0xF075, 2, "", "saveflags vX", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::SCHIPC|C8V::XO_CHIP|C8V::MEGA_CHIP, "store the content of the registers v0 to vX into flags storage (outside of the addressable ram) [Q: SCHIP-1.x and SCHIPC only support v0-v7 on a real HP48]" },
    { OT_FxFF, 0xF085, 2, "", "loadflags vX", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::SCHIPC|C8V::XO_CHIP|C8V::MEGA_CHIP, "load the registers v0 to vX from flags storage (outside the addressable ram) [Q: SCHIP-1.x and SCHIPC only support v0-v7 on a real HP48]" },
    { OT_FxFF, 0xF0F8, 2, "dw #fXf8", "0xfX 0xf8", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X, "output vX to io port" },
    { OT_FxFF, 0xF0FB, 2, "dw #fXfb", "0xfX 0xfb", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X, "wait for input from io and load into vX" }
};
// clang-format on

inline static std::map<std::string, std::string> octoMacros = {
    {"megaoff", ":macro megaoff { :byte 0x00  :byte 0x10 }"},
    {"megaon", ":macro megaon { :byte 0x00 :byte 0x11 }"},
    {"scroll_up", ":macro scroll_up n { :calc BN { 0xB0 + ( n & 0xF ) } :byte 0x00 :byte BN }"},
    {"ldhi", ":macro ldhi nnnn { :byte 0x01 :byte 0x00 :pointer nnnn }"},
    {"ldpal", ":macro ldpal nn { :byte 0x02 :byte nn }"},
    {"sprw", ":macro sprw nn { :byte 0x03 :byte nn }"},
    {"sprh", ":macro sprh nn { :byte 0x04 :byte nn }"},
    {"alpha", ":macro alpha nn { :byte 0x05 :byte nn }"},
    {"digisnd", ":macro digisnd n { :calc ZN { n & 0xF } :byte 0x06 :byte ZN }"},
    {"stopsnd", ":macro stopsnd { :byte 0x07 :byte 0x00 }"},
    {"bmode", ":macro bmode n { :calc ZN { n & 0xF } :byte 0x08 :byte ZN }"},
    {"ccol", ":macro ccol nn { :byte 0x09 :byte nn }"},
    {"cycle-bgcol", ":macro cycle-background { 0x02 0xa0 }"},
    {"cycle-bgcol-mp", ":macro cycle-background { 0x02 0xf0 }"},
    {"col-low", ":macro col-low x y { :calc MSB { 0xB0 + ( x & 0xF ) } :calc LSB { ( y & 0xF ) << 4 } :byte MSB :byte LSB }"},
    {"col-high", ":macro col-high x y n { :calc MSB { 0xB0 + ( x & 0xF ) } :calc LSB { ( ( y & 0xF ) << 4 ) + ( n & 0xF ) } :byte MSB :byte LSB }"}
};

class OpcodeSet
{
public:
    using SymbolResolver = std::function<std::string(uint16_t)>;
    explicit OpcodeSet(Chip8Variant variant, SymbolResolver resolver = {})
    : _variant(variant)
    , _labelOrAddress(std::move(resolver))
    , _mappedInfo(0x10000, 0xff)
    {
        for(const auto& info : opcodes) {
            if(uint64_t(info.variants & variant) != 0) {
                mapOpcode(opcodeMasks[info.type], info.opcode, &info - opcodes.data());
            }
        }
    }
    void formatInvalidAsHex(bool asHex) { _invalidAsHex = asHex; }
    [[nodiscard]] Chip8Variant getVariant() const { return _variant; }
    [[nodiscard]] const OpcodeInfo* getOpcodeInfo(uint16_t opcode) const
    {
        auto index = _mappedInfo[opcode];
        return index == 0xff ? nullptr : &opcodes[index];
    }
    [[nodiscard]] std::tuple<uint16_t, uint16_t, std::string> formatOpcode(uint16_t opcode, uint16_t nnnn = 0) const
    {
        static const char* hex = "0123456789abcdef";
        const auto* info = getOpcodeInfo(opcode);
        if(!info) {
            if(_invalidAsHex)
                return {2, opcode, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
            else
                return {2, opcode, "invalid"};
        }
        unsigned x = (opcode >> 8) & 0xF;
        unsigned y = (opcode >> 4) & 0xF;
        char NNNNNN[8]{};
        size_t ncnt = 0;
        uint32_t addr = ~0;
        switch(info->type) {
            case OT_Fnnn:
                addr = opcode & 0xFFF;
                NNNNNN[ncnt++] = hex[(opcode >> 8) & 0xF];
                [[fallthrough]];
            case OT_FFnn:
            case OT_Fxnn:
                NNNNNN[ncnt++] = hex[(opcode >> 4) & 0xF];
                [[fallthrough]];
            case OT_FFFn:
            case OT_Fxyn:
                NNNNNN[ncnt++] = hex[opcode & 0xF];
                break;
            default:
                break;
        }
        if(info->size == 4) {
            addr = nnnn;
            fmt::format_to(NNNNNN + ncnt, "{:04x}", nnnn);
        }
        std::string result;
        result.reserve(15);
        char prev = 0;
        size_t nidx = 0;
        std::string label;
        for(const auto& c : info->octo) {
            switch(c) {
                case 'X':
                    if(!std::isalnum((unsigned)prev)) result += "0x";
                    result += hex[x];
                    break;
                case 'Y':
                    if(!std::isalnum((unsigned)prev)) result += "0x";
                    result += hex[y];
                    break;
                case 'N':
                    if(!nidx && addr != ~0 && _labelOrAddress && info->octo[0] != '0') {
                        label = _labelOrAddress(addr);
                        if(!label.empty()) {
                            nidx++;
                            result += label;
                        }
                    }
                    if(label.empty()) {
                        if (!std::isalnum((unsigned)prev))
                            result += "0x";
                        result += NNNNNN[nidx++];
                    }
                    break;
                default:
                    result += c;
                    break;
            }
            prev = c;
        }
        return {info->size, info->opcode, result};
    }
private:
    void setIfEmpty(uint16_t index, uint8_t value)
    {
        if(_mappedInfo[index] == 0xff)
            _mappedInfo[index] = value;
    }
    void mapOpcode(uint16_t mask, uint16_t opcode, uint8_t infoIndex)
    {
        uint16_t argMask = ~mask;
        int shift = 0;
        if(argMask) {
            while((argMask & 1) == 0) {
                argMask >>= 1;
                ++shift;
            }
            uint16_t val = 0;
            do {
                setIfEmpty(opcode | ((val & argMask) << shift), infoIndex);
            }
            while(++val & argMask);
        }
        else {
            setIfEmpty(opcode, infoIndex);
        }
    }
    Chip8Variant _variant;
    SymbolResolver _labelOrAddress;
    std::vector<uint8_t> _mappedInfo{};
    bool _invalidAsHex = false;
};


} // namespace detail

} // namespace emu
