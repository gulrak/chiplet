#include <chiplet/chip8decompiler.hpp>
#include <chiplet/octocompiler.hpp>
#include <doctest/doctest.h>

using emu::Chip8Decompiler;

TEST_CASE("Relocation follows code paths and preserves instruction-like data and odd tails")
{
    const std::vector<uint8_t> input{0x12,0x06, 0xA2,0xAA, 0x18,0x18, 0xA2,0x04, 0x22,0x0C, 0x12,0x0A, 0x00,0xEE, 0x1B};
    Chip8Decompiler decoder(input, 0x200);
    const auto result = decoder.relocate(0x200, 0x600);
    const std::vector<uint8_t> expected{0x16,0x06, 0xA2,0xAA, 0x18,0x18, 0xA6,0x04, 0x26,0x0C, 0x16,0x0A, 0x00,0xEE, 0x1B};
    CHECK(result.bytes == expected);
}

TEST_CASE("Relocation traces both conditional paths and call targets")
{
    const std::vector<uint8_t> input{0x30,0x00, 0x12,0x0C, 0x22,0x0E, 0xA2,0x10, 0x12,0x08, 0xAA,0xAA, 0x12,0x0C, 0x00,0xEE, 0x1B};
    Chip8Decompiler decoder(input, 0x200);
    const auto result = decoder.relocate(0x200, 0x600);
    CHECK(result.bytes == std::vector<uint8_t>{0x30,0x00, 0x16,0x0C, 0x26,0x0E, 0xA6,0x10, 0x16,0x08, 0xAA,0xAA, 0x16,0x0C, 0x00,0xEE, 0x1B});
}

TEST_CASE("Relocation preserves system addresses and moves scratch RAM beyond the binary")
{
    const std::vector<uint8_t> input{0xA0,0x50, 0xA3,0x00, 0x12,0x04};
    Chip8Decompiler decoder(input, 0x200);
    CHECK(decoder.relocate(0x200, 0x600).bytes == std::vector<uint8_t>{0xA0,0x50, 0xA7,0x00, 0x16,0x04});
    const std::vector<uint8_t> port{0xA0,0x50, 0xA7,0x00, 0x16,0x04};
    Chip8Decompiler reverse(port, 0x600);
    CHECK(reverse.relocate(0x600, 0x200).bytes == input);
}

TEST_CASE("Relocation handles odd entry targets and an entry after a data prefix")
{
    const std::vector<uint8_t> input{0x12,0x03, 0xAA, 0xA2,0x02, 0x12,0x05, 0x1B};
    Chip8Decompiler decoder(input, 0x200);
    CHECK(decoder.relocate(0x200, 0x600).bytes == std::vector<uint8_t>{0x16,0x03, 0xAA, 0xA6,0x02, 0x16,0x05, 0x1B});
    const std::vector<uint8_t> prefix{0x18,0x18, 0xA2,0x00, 0x12,0x04};
    Chip8Decompiler prefixed(prefix, 0x200);
    CHECK(prefixed.relocate(0x202, 0x602).bytes == std::vector<uint8_t>{0x18,0x18, 0xA6,0x00, 0x16,0x04});
}

TEST_CASE("Relocation handles long XO-CHIP and MegaChip address operands")
{
    const std::vector<uint8_t> xo{0xF0,0x00, 0x23,0x45, 0x12,0x04};
    Chip8Decompiler decoder(xo, 0x200);
    CHECK(decoder.relocate(0x200, 0x600).bytes == std::vector<uint8_t>{0xF0,0x00, 0x27,0x45, 0x16,0x04});
    const std::vector<uint8_t> mega{0x00,0x11, 0x01,0x01, 0x12,0x34, 0x12,0x06};
    Chip8Decompiler megaDecoder(mega, 0x200);
    CHECK(megaDecoder.relocate(0x200, 0x600).bytes == std::vector<uint8_t>{0x00,0x11, 0x01,0x01, 0x16,0x34, 0x16,0x06});
}

TEST_CASE("Relocation traces resolved indexed jumps and warns about their limitations")
{
    const std::vector<uint8_t> input{0x60,0x02, 0xB2,0x04, 0xAA,0xAA, 0xA2,0x04, 0x12,0x08};
    Chip8Decompiler decoder(input, 0x200);
    const auto result = decoder.relocate(0x200, 0x600);
    CHECK(result.bytes == std::vector<uint8_t>{0x60,0x02, 0xB6,0x04, 0xAA,0xAA, 0xA6,0x04, 0x16,0x08});
    CHECK_FALSE(result.warnings.empty());
}

TEST_CASE("Relocation rejects address overflow and truncated instructions")
{
    const std::vector<uint8_t> input{0xAF,0xFF, 0x12,0x02};
    Chip8Decompiler decoder(input, 0x200);
    CHECK_THROWS_AS(decoder.relocate(0x200, 0x600), std::out_of_range);
    const std::vector<uint8_t> truncated{0xF0,0x00,0x12};
    Chip8Decompiler shortDecoder(truncated, 0x200);
    CHECK_THROWS_AS(shortDecoder.relocate(0x200, 0x600), std::runtime_error);
    const std::vector<uint8_t> plain{0x12,0x00};
    Chip8Decompiler outside(plain, 0x200);
    CHECK_THROWS_AS(outside.relocate(0x201, 0x600), std::invalid_argument);
    Chip8Decompiler tooHigh(plain, 0x200);
    CHECK_THROWS_AS(tooHigh.relocate(0x200, 0xFFFF), std::out_of_range);
}

TEST_CASE("Relocation warns about detected self modification and native code")
{
    const std::vector<uint8_t> input{0xA2,0x00, 0xF0,0x55, 0x12,0x04};
    Chip8Decompiler decoder(input, 0x200);
    const auto result = decoder.relocate(0x200, 0x600);
    CHECK_FALSE(result.warnings.empty());
    const std::vector<uint8_t> native{0x03,0x00, 0x12,0x02};
    Chip8Decompiler nativeDecoder(native, 0x200, emu::C8V::CHIP_8_COSMAC_VIP);
    const auto nativeResult = nativeDecoder.relocate(0x200, 0x600);
    CHECK(nativeResult.bytes[0] == 0x03);
    CHECK(nativeResult.bytes[1] == 0x00);
    CHECK_FALSE(nativeResult.warnings.empty());
}

TEST_CASE("Relocation traces original VIP skips with nonzero low nibbles")
{
    const std::vector<uint8_t> input{0x57,0xAD, 0x12,0x06, 0xA2,0x00, 0x12,0x06};
    Chip8Decompiler decoder(input, 0x200);
    const auto result = decoder.relocate(0x200, 0x600);
    CHECK(result.bytes == std::vector<uint8_t>{0x57,0xAD, 0x16,0x06, 0xA6,0x00, 0x16,0x06});
    CHECK_FALSE(result.warnings.empty());
}

TEST_CASE("Relocation rejects incomplete tracing of overlapping instructions")
{
    const std::vector<uint8_t> input{0x30,0x00, 0x12,0x03, 0xA2,0x00, 0x12,0x06};
    Chip8Decompiler decoder(input, 0x200);
    CHECK_THROWS_AS(decoder.relocate(0x200, 0x600), std::runtime_error);
}

TEST_CASE("Decompiler keeps formatting programs that mix variants or contain unknown opcodes")
{
    using emu::C8V;
    // hires (SCHIP/XO/MegaChip), plane 0 (XO-CHIP), 02A0 (MegaChip ldpal or CHIP-8X cycle-bgcol), unknown E123, jump to self
    const std::vector<uint8_t> mixed{0x00,0xFF, 0xF0,0x01, 0x02,0xA0, 0xE1,0x23, 0x12,0x08};
    Chip8Decompiler decoder(mixed, 0x200);
    std::ostringstream os;
    decoder.decompile("mixed.ch8", 0x200, &os);
    const auto source = os.str();
    CHECK(decoder.possibleVariants().is_empty());
    CHECK(decoder.unknownOpcodes() == 1);
    CHECK(decoder.variantCover(C8V::CHIP_8 | C8V::CHIP_8X | C8V::SCHIP_1_1 | C8V::MEGA_CHIP | C8V::XO_CHIP) == (C8V::MEGA_CHIP | C8V::XO_CHIP));
    CHECK(source.find("        hires\n") != std::string::npos);
    CHECK(source.find("        plane 0x0\n") != std::string::npos);
    CHECK(source.find("        ldpal 0xa0\n") != std::string::npos);
    CHECK(source.find("        0xE1 0x23\n") != std::string::npos);
    CHECK(source.find(":macro ldpal nn { :byte 0x02 :byte nn }") != std::string::npos);

    emu::OctoCompiler compiler;
    REQUIRE(compiler.compile("mixed.8o", source, false).resultType == emu::CompileResult::eOK);
    CHECK(std::vector<uint8_t>(compiler.code(), compiler.code() + compiler.codeSize()) == mixed);

    // a single unknown opcode doesn't discard the detected variants
    const std::vector<uint8_t> unknown{0x00,0xE0, 0xE1,0x23, 0x12,0x04};
    Chip8Decompiler unknownDecoder(unknown, 0x200);
    unknownDecoder.decompile("unknown.ch8", 0x200, nullptr, true, true);
    CHECK(unknownDecoder.possibleVariants().contains(C8V::CHIP_8));
    CHECK(unknownDecoder.unknownOpcodes() == 1);
}
