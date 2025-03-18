//
// Created by Steffen Schümann on 13.03.25.
//

#include <ghc/fs_impl.hpp>

#include <chiplet/utility.hpp>
#include <chiplet/decoder/saturndecoder.hpp>

#include <array>
#include <utility>
#include <set>
#include <vector>

#include <algorithm>
#include <chrono>
#include <iostream>

#include <fmt/format.h>

#include <set>

namespace emu {

class SaturnDisassembler : public SaturnDecoder
{
public:
    constexpr static uint32_t ADDRESS_SPACE = 0x100000;
    constexpr static uint32_t RAM_OFFSET = 0x70000;
    constexpr static uint32_t RAM_END = 0x80000;
    SaturnDisassembler()
        : _ram(ADDRESS_SPACE/2, 0)
    {}
    void loadData(uint32_t address, const std::vector<uint8_t>& data)
    {
        std::copy(data.begin(), data.end(), _ram.begin() + address/2);
    }

    unsigned readNibble(const uint32_t address) const override
    {
        //constexpr char hex[] = "0123456789abcdef";
        const auto nibble = address & 1 ? _ram[address>>1] >> 4 : _ram[address>>1] & 0xf;
        //std::cout << hex[nibble];
        return nibble;
    }

    std::pair<std::string,std::string> disassembleOpcode(uint32_t& address) const
    {
        uint32_t startAddress = address;
        if (const auto decoded = decode(address); decoded.oid != Opc_Invalid) {
            const auto& info = *decoded.info;
            auto opcodeSize = info.opcodeSize;
            switch (info.numVars) {
                case 0:
                    return {fmt::format("{:0{}x}", decoded.opcode, opcodeSize), info.fmt};
                case 1:
                    return {fmt::format("{:0{}x}", decoded.opcode, opcodeSize),
                        fmt::format(fmt::runtime(info.fmt),
                            varToString(decoded, startAddress, 0))};
                case 2:
                    if (info.opcodeSize < 0) {
                        return {fmt::format("{:0{}x}{:0{}x}", decoded.opcode, -opcodeSize, decoded.varArg, (decoded.opcode&0xF) + 1),
                            fmt::format(fmt::runtime(info.fmt),
                                varToString(decoded, startAddress, 0),
                                varToString(decoded, startAddress, 1))};
                    }
                    return {fmt::format("{:0{}x}", decoded.opcode, opcodeSize),
                        fmt::format(fmt::runtime(info.fmt),
                            varToString(decoded, startAddress, 0),
                            varToString(decoded, startAddress, 1))};
                case 3:
                    return {fmt::format("{:0{}x}", decoded.opcode, opcodeSize),
                        fmt::format(fmt::runtime(info.fmt),
                            varToString(decoded, startAddress, 0),
                            varToString(decoded, startAddress, 1),
                            varToString(decoded, startAddress, 2))};
                default:
                    return {fmt::format("{:0{}x}?", decoded.opcode, opcodeSize), info.fmt};
            }
        }
        return {"",""};
    }

protected:
    uint64_t _rA{};
    uint64_t _rB{};
    uint64_t _rC{};
    uint64_t _rD{};
    std::array<uint64_t, 5> _rR{};
    std::array<uint32_t, 8> _rRSTK{};
    uint16_t _rIN{};   // 10 bit
    uint16_t _rOUT{};  // 10 bit
    uint32_t _rPC{};
    uint32_t _rD0{};
    uint32_t _rD1{};
    uint16_t _rST{};
    uint8_t _rP{};
    uint8_t _rHS{};
    bool _rCarry{false};
    std::vector<uint8_t> _ram;
};

}

std::string_view getNextInstruction(std::string_view source, std::string_view::const_iterator& iter)
{
    while (iter != source.cend()) {
        // Find the position of the next newline character
        const auto end = std::find(iter, source.cend(), '\n');
        std::string_view line(&(*iter), std::distance(iter, end) - (end != iter && *(end - 1) == '\r' ? 1 : 0));
        if (const auto pos = line.find(';'); pos != std::string_view::npos) {
            line = line.substr(0, pos);
        }
        if (const auto pos = line.find(':'); pos != std::string_view::npos) {
            line = line.substr(pos + 1);
        }
        line = trim(line);
        iter = end != source.end() ? std::next(end) : end;
        if (!line.empty() && line.find('=') == std::string_view::npos) {
            return line;
        }
    }
    return {"---"};
}

int main()
{
    constexpr std::string_view magic = "HPHP48-";
    auto data = loadFile("/Users/schuemann/Development/c8/cadmium/SCHIP11");
    if (!std::equal(magic.begin(), magic.end(), data.begin(), [](char a, uint8_t b) { return static_cast<uint8_t>(a) == b; })) {
        std::cerr << "Not a hp object file!" << std::endl;
        exit(1);
    }
    auto reference = loadTextFile("/Users/schuemann/Development/c8/cadmium/schip10.asap");
    //std::ranges::transform(data, data.begin(), [](uint8_t byte) { return ((byte & 0x0F) << 4) | ((byte & 0xF0) >> 4); });
    emu::SaturnDisassembler saturn{};
    saturn.loadData(0x71000, data);
    uint32_t address = 0x71010;
//    for (int i = 0; i < 16; ++i)
//        std::cout << fmt::format("{:01x}", saturn.readNibble(index + i));
//    std::cout << std::endl;
    std::cout << fmt::format("Type: {:05x}", saturn.readNibbles<5>(address)) << std::endl;
    std::cout << fmt::format("Size: {:05x}", saturn.readNibbles<5>(address)) << std::endl;
    auto refSV = std::string_view(reference);
    auto refIter = refSV.begin();
    for (int i = 0; i < 1; i++) {
        getNextInstruction(refSV, refIter); // skip data header
    }
    auto startTime = std::chrono::high_resolution_clock::now();
    auto lines = 0;
    auto chars = 0;
    uint64_t opcode = 0;
    unsigned opcodeSize = 0;
    std::set<size_t> seen;
    auto startAddress = address;
    for (int i = 0; i < 1; i++) {
        address = startAddress;
        while (address < 0x73cbc) {
#if 1
            auto refInstruction = getNextInstruction(refSV, refIter);
            auto opcodeAddress = address;
            auto [opcode, disassembly] = saturn.disassembleOpcode(address);
            if (disassembly.empty()) {
                std::cerr << "Reference: " << refInstruction << std::endl;
                break;
            }
            ++lines;
            //auto s = fmt::format("{:5x}: {:21} {:32} - {}", opcodeAddress, opcode, disassembly, refInstruction);
            auto s = fmt::format("{:5x}: {:21} {:32}", opcodeAddress, opcode, disassembly);
            std::cout << s << std::endl;
            chars += opcode.size() + disassembly.size() + 1;
#else
            auto decoded = saturn.decode(address);
            if (decoded.oid != emu::SaturnDecoder::Opc_Invalid) {
                seen.insert(decoded.oid);
                chars += decoded.info->opcodeSize;
                ++lines;
            }
#endif
        }
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    for (auto i : seen) {
        std::cout << fmt::format("{}, ", i);
    }
    std::cout << std::endl;
    std::cout << fmt::format("Duration: {} us, {} lines, {} chars, opcodes seen: {}", duration.count(), lines, chars, seen.size()) << std::endl;
}
