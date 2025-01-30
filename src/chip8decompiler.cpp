//---------------------------------------------------------------------------------------
// src/emulation/chip8decompiler.hpp
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

#include <chiplet/chip8decompiler.hpp>


#define CASE_7(base) case base: case base+1: case base+2: case base+3: case base+4: case base+5: case base+6
#define CASE_15(base) case base: case base+1: case base+2: case base+3: case base+4: case base+5: case base+6: case base+7:\
                     case base+8: case base+9: case base+10: case base+11: case base+12: case base+13: case base+14
#define CASE_16(base) case base: case base+1: case base+2: case base+3: case base+4: case base+5: case base+6: case base+7:\
                     case base+8: case base+9: case base+10: case base+11: case base+12: case base+13: case base+14: case base+15

namespace emu {

std::pair<std::string,std::string> Chip8Decompiler::chipVariantName(Chip8Variant cv)
{
    switch(cv)
    {
        case C8V::CHIP_8: return {"chip-8", "CHIP-8"};
        case C8V::CHIP_8_1_2: return {"chio-8.5", "CHIP-8 1/2"};
        case C8V::CHIP_8_I: return {"chip-8i", "CHIP-8I"};
        case C8V::CHIP_8_II: return {"chip-8ii", "CHIP-8 II aka. Keyboard Kontrol"};
        case C8V::CHIP_8_III: return {"chip-8iii", "CHIP-8III"};
        case C8V::CHIP_8_TPD: return {"chip-8-tpd", "Two-page display for CHIP-8"};
        case C8V::CHIP_8C: return {"chip-8c", "CHIP-8C"};
        case C8V::CHIP_10: return {"chip-10", "CHIP-10"};
        case C8V::CHIP_8_SRV: return {"chip-8-srv", "CHIP-8 modification for saving and restoring variables"};
        case C8V::CHIP_8_SRV_I: return {"chip-8-srv-i", "Improved CHIP-8 modification for saving and restoring variables"};
        case C8V::CHIP_8_RB: return {"chip-8-rb", "CHIP-8 modification with relative branching"};
        case C8V::CHIP_8_ARB: return {"chip-8-arb", "Another CHIP-8 modification with relative branching"};
        case C8V::CHIP_8_FSD: return {"chip-8-fsb", "CHIP-8 modification with fast, single-dot DXYN"};
        case C8V::CHIP_8_IOPD: return {"chip-8-iopd", "CHIP-8 with I/O port driver routine"};
        case C8V::CHIP_8_8BMD: return {"chip-8-8bmd", "CHIP-8 8-bit multiply and divide"};
        case C8V::HI_RES_CHIP_8: return {"hires-chip-8","HI-RES CHIP-8 (four-page display)"};
        case C8V::HI_RES_CHIP_8_IO: return {"hires-chip-8-io", "HI-RES CHIP-8 with I/O"};
        case C8V::HI_RES_CHIP_8_PS: return {"hires-chip-8-ps", "HI-RES CHIP-8 with page switching"};
        case C8V::CHIP_8E: return {"chip-8e", "CHIP-8E"};
        case C8V::CHIP_8_IBNNN: return {"chip-8-ibnnn", "CHIP-8 with improved BNNN"};
        case C8V::CHIP_8_SCROLL: return {"chip-8-scroll", "CHIP-8 scrolling routine"};
        case C8V::CHIP_8X: return {"chip-8x", "CHIP-8X"};
        case C8V::CHIP_8X_TPD: return {"chip-8x-tdp", "Two-page display for CHIP-8X"};
        case C8V::HI_RES_CHIP_8X: return {"hires-chip-8x", "Hi-res CHIP-8X"};
        case C8V::CHIP_8Y: return {"chip-8y", "CHIP-8Y"};
        case C8V::CHIP_8_CtS: return {"chip-8-cts", "CHIP-8 “Copy to Screen"};
        case C8V::CHIP_BETA: return {"chip-beta", "CHIP-BETA"};
        case C8V::CHIP_8M: return {"chip-8m", "CHIP-8M"};
        case C8V::MULTIPLE_NIM: return {"multi-nim", "Multiple Nim interpreter"};
        case C8V::DOUBLE_ARRAY_MOD: return {"double-array-mod", "Double Array Modification"};
        case C8V::CHIP_8_D6800: return {"chip-8-d6800", "CHIP-8 for DREAM 6800 (CHIPOS)"};
        case C8V::CHIP_8_D6800_LOP: return {"chip-8-d6800-lop", "CHIP-8 with logical operators for DREAM 6800 (CHIPOSLO)"};
        case C8V::CHIP_8_D6800_JOY: return {"chip-8-d6800-joy", "CHIP-8 for DREAM 6800 with joystick"};
        case C8V::CHIPOS_2K_D6800: return {"chipos-2k-d6800", "2K CHIPOS for DREAM 6800"};
        case C8V::CHIP_8_ETI660: return {"chip-8-eti660", "CHIP-8 for ETI-660"};
        case C8V::CHIP_8_ETI660_COL: return {"chip-8-eti660-col", "CHIP-8 with color support for ETI-660"};
        case C8V::CHIP_8_ETI660_HR: return {"chip-8-eti660-hr", "CHIP-8 for ETI-660 with high resolution"};
        case C8V::CHIP_8_COSMAC_ELF: return {"chip-8-cosmac-elf", "CHIP-8 for COSMAC ELF"};
        case C8V::CHIP_8_ACE_VDU: return {"chip-8-ace-vdu", "CHIP-VDU / CHIP-8 for the ACE VDU"};
        case C8V::CHIP_8_AE: return {"chip-8-ae", "CHIP-8 AE (ACE Extended)"};
        case C8V::CHIP_8_DC_V2: return {"chip-8-dc-v2", "Dreamcards Extended CHIP-8 V2.0"};
        case C8V::CHIP_8_AMIGA: return {"chip-8-amiga", "Amiga CHIP-8 interpreter"};
        case C8V::CHIP_48: return {"chip-48", "CHIP-48"};
        case C8V::SCHIP_1_0: return {"schip-1.0", "SUPER-CHIP 1.0"};
        case C8V::SCHIP_1_1: return {"schip-1.1", "SUPER-CHIP 1.1"};
        case C8V::GCHIP: return {"gchip", "GCHIP"};
        case C8V::SCHIPC_GCHIPC: return {"schipc", "SCHIP Compatibility (SCHPC) and GCHIP Compatibility (GCHPC)"};
        case C8V::VIP2K_CHIP_8: return {"vip2k-chip-8", "VIP2K CHIP-8"};
        case C8V::SCHIP_1_1_SCRUP: return {"schip-1.1-scrup", "SUPER-CHIP with scroll up"};
        case C8V::CHIP8RUN: return {"chip8run", "chip8run"};
        case C8V::MEGA_CHIP: return {"megachip", "Mega-Chip"};
        case C8V::XO_CHIP: return {"xo-chip", "XO-CHIP"};
        case C8V::OCTO: return {"octo", "Octo"};
        case C8V::CHIP_8_CL_COL: return {"chip-8-cl-col", "CHIP-8 Classic / Color"};
        case C8V::SCHIP_MODERN: return {"schip-modern", "SUPER-CHIP Modern"};
        default: return {"", ""};
    }
}


std::pair<int, std::string> Chip8Decompiler::disassemble1802InstructionWithBytes(int32_t pc, const uint8_t* code, const uint8_t* end)
{
    auto addr = pc;
    uint8_t data[3]{};
    for(size_t i = 0; i < 3; ++i) {
        data[i] = (code + i < end ? *code++ : 0);
    }
    auto [size, text] = disassemble1802Instruction(data, data+3);
    switch(size) {
        case 2:  return {size, fmt::format("{:04x}: {:02x} {:02x}  {}", addr, data[0], data[1], text)};
        case 3:  return {size, fmt::format("{:04x}: {:02x} {:02x} {:02x}  {}", addr, data[0], data[1], data[2], text)};
        default: return {size, fmt::format("{:04x}: {:02x}     {}", addr, data[0], text)};
    }
}

std::pair<int, std::string> Chip8Decompiler::disassemble1802Instruction(const uint8_t* code, const uint8_t* end)
{
    auto opcode = *code++;
    auto n = opcode & 0xF;
    switch(opcode) {
        case 0x00: return {1, "IDL"};
            CASE_15(0x01): return {1, fmt::format("LDN R{:X}", n)};
            CASE_16(0x10): return {1, fmt::format("INC R{:X}", n)};
            CASE_16(0x20): return {1, fmt::format("DEC R{:X}", n)};
        case 0x30: return {2, fmt::format("BR 0x{:02X}", *code)};
        case 0x31: return {2, fmt::format("BQ 0x{:02X}", *code)};
        case 0x32: return {2, fmt::format("BZ 0x{:02X}", *code)};
        case 0x33: return {2, fmt::format("BDF 0x{:02X}", *code)};
        case 0x34: return {2, fmt::format("B1 0x{:02X}", *code)};
        case 0x35: return {2, fmt::format("B2 0x{:02X}", *code)};
        case 0x36: return {2, fmt::format("B3 0x{:02X}", *code)};
        case 0x37: return {2, fmt::format("B4 0x{:02X}", *code)};
        case 0x38: return {1, "SKP"};
        case 0x39: return {2, fmt::format("BNQ 0x{:02X}", *code)};
        case 0x3A: return {2, fmt::format("BNZ 0x{:02X}", *code)};
        case 0x3B: return {2, fmt::format("BNF 0x{:02X}", *code)};
        case 0x3C: return {2, fmt::format("BN1 0x{:02X}", *code)};
        case 0x3D: return {2, fmt::format("BN2 0x{:02X}", *code)};
        case 0x3e: return {2, fmt::format("BN3 0x{:02X}", *code)};
        case 0x3f: return {2, fmt::format("BN4 0x{:02X}", *code)};
            CASE_16(0x40): return {1, fmt::format("LDA R{:X}", n)};
            CASE_16(0x50): return {1, fmt::format("STR R{:X}", n)};
        case 0x60: return {1, "IRX"};
            CASE_7(0x61): return {1, fmt::format("OUT {:X}", n)};
            CASE_7(0x69): return {1, fmt::format("INP {:X}", n&7)};
        case 0x70: return {1, "RET"};
        case 0x71: return {1, "DIS"};
        case 0x72: return {1, "LDXA"};
        case 0x73: return {1, "STXD"};
        case 0x74: return {1, "ADC"};
        case 0x75: return {1, "SDB"};
        case 0x76: return {1, "SHRC"};
        case 0x77: return {1, "SMB"};
        case 0x78: return {1, "SAV"};
        case 0x79: return {1, "MARK"};
        case 0x7A: return {1, "REQ"};
        case 0x7B: return {1, "SEQ"};
        case 0x7C: return {2, fmt::format("ADCI #0x{:02X}", *code)};
        case 0x7D: return {2, fmt::format("SDBI #0x{:02X}", *code)};
        case 0x7E: return {1, "SHLC"};
        case 0x7F: return {2, fmt::format("SMBI #0x{:02X}", *code)};
            CASE_16(0x80): return {1, fmt::format("GLO R{:X}", n)};
            CASE_16(0x90): return {1, fmt::format("GHI R{:X}", n)};
            CASE_16(0xA0): return {1, fmt::format("PLO R{:X}", n)};
            CASE_16(0xB0): return {1, fmt::format("PHI R{:X}", n)};
        case 0xC0: return {3, fmt::format("LBR 0x{:04X}", (*code<<8)|*(code+1))};
        case 0xC1: return {3, fmt::format("LBQ 0x{:04X}", (*code<<8)|*(code+1))};
        case 0xC2: return {3, fmt::format("LBZ 0x{:04X}", (*code<<8)|*(code+1))};
        case 0xC3: return {3, fmt::format("LBDF 0x{:04X}", (*code<<8)|*(code+1))};
        case 0xC4: return {1, "NOP"};
        case 0xC5: return {1, "LSNQ"};
        case 0xC6: return {1, "LSNZ"};
        case 0xC7: return {1, "LSNF"};
        case 0xC8: return {1, "LSKP"};
        case 0xC9: return {3, fmt::format("LBNQ 0x{:04X}", (*code<<8)|*(code+1))};
        case 0xCA: return {3, fmt::format("LBNZ 0x{:04X}", (*code<<8)|*(code+1))};
        case 0xCB: return {3, fmt::format("LBNF 0x{:04X}", (*code<<8)|*(code+1))};
        case 0xCC: return {1, "LSIE"};
        case 0xCD: return {1, "LSQ"};
        case 0xCE: return {1, "LSZ"};
        case 0xCF: return {1, "LSDF"};
            CASE_16(0xD0): return {1, fmt::format("SEP R{:X}", n)};
            CASE_16(0xE0): return {1, fmt::format("SEX R{:X}", n)};
        case 0xF0: return {1, "LDX"};
        case 0xF1: return {1, "OR"};
        case 0xF2: return {1, "AND"};
        case 0xF3: return {1, "XOR"};
        case 0xF4: return {1, "ADD"};
        case 0xF5: return {1, "SD"};
        case 0xF6: return {1, "SHR"};
        case 0xF7: return {1, "SM"};
        case 0xF8: return {2, fmt::format("LDI #0x{:02X}", *code)};
        case 0xF9: return {2, fmt::format("ORI #0x{:02X}", *code)};
        case 0xFA: return {2, fmt::format("ANI #0x{:02X}", *code)};
        case 0xFB: return {2, fmt::format("XRI #0x{:02X}", *code)};
        case 0xFC: return {2, fmt::format("ADI #0x{:02X}", *code)};
        case 0xFD: return {2, fmt::format("SDI #0x{:02X}", *code)};
        case 0xFE: return {1, "SHL"};
        case 0xFF: return {2, fmt::format("SMI #0x{:02X}", *code)};
        default:
            return {1, "ILLEGAL"};
    }
}

}