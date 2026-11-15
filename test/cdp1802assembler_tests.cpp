#include <doctest/doctest.h>

#include <chiplet/cdp1802assembler.hpp>
#include <chiplet/sha1.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using emu::Cdp1802Assembler;
using emu::CompileResult;

namespace {

std::vector<uint8_t> readBinary(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::vector<uint8_t> codeOf(const Cdp1802Assembler& assembler)
{
    return {assembler.code(), assembler.code() + assembler.codeSize()};
}

const char* const TWO_SEGMENTS = R"(        ORG #0010
START:  LDI #12         .. line 2
        PLO R2
LOOP:   SEQ
        BR LOOP
        ORG #0020
        DC #AA,#BB
)";

}  // namespace

TEST_SUITE("Cdp1802Assembler")
{
    TEST_CASE("A program is its segments with the source line of every byte")
    {
        Cdp1802Assembler assembler;
        const auto& result = assembler.compile("blink.asm", TWO_SEGMENTS);
        REQUIRE_MESSAGE(!assembler.isError(), result.errorMessage);
        CHECK(result.resultType == CompileResult::eOK);
        CHECK(result.errorMessage == "No errors.");

        const auto& segments = assembler.segments();
        REQUIRE(segments.size() == 2);
        CHECK(segments[0].address == 0x10);
        CHECK(segments[0].bytes == std::vector<uint8_t>{0xF8, 0x12, 0xA2, 0x7B, 0x30, 0x13});
        CHECK(segments[0].lines == std::vector<uint32_t>{2, 2, 3, 4, 5, 5});
        CHECK(segments[1].address == 0x20);
        CHECK(segments[1].bytes == std::vector<uint8_t>{0xAA, 0xBB});
        CHECK(segments[1].lines == std::vector<uint32_t>{7, 7});

        // the contiguous code starts at the first ORG, the gap is filled with 0
        CHECK(assembler.startAddress() == 0x10);
        REQUIRE(assembler.codeSize() == 0x12);
        const auto code = codeOf(assembler);
        CHECK(code[0] == 0xF8);
        CHECK(code[6] == 0);
        CHECK(code[0x10] == 0xAA);
        CHECK(code[0x11] == 0xBB);
        Sha1 sum;
        sum.add(code.data(), code.size());
        sum.finalize();
        CHECK(assembler.sha1() == Sha1::Digest(sum));
        CHECK(assembler.listing().empty());

        // addresses and lines
        CHECK(assembler.lineForAddr(0x10) == 2u);
        CHECK(assembler.lineForAddr(0x15) == 5u);
        CHECK_FALSE(assembler.lineForAddr(0x16).has_value());
        CHECK(assembler.lineForAddr(0x21) == 7u);
        CHECK_FALSE(assembler.lineForAddr(0x22).has_value());
        REQUIRE(assembler.addrForLine(2).has_value());
        CHECK(*assembler.addrForLine(2) == std::make_pair(0x10u, 0x11u));
        CHECK(*assembler.addrForLine(7) == std::make_pair(0x20u, 0x21u));
        CHECK_FALSE(assembler.addrForLine(1).has_value());  // ORG has no code
        CHECK_FALSE(assembler.addrForLine(99).has_value());
    }

    TEST_CASE("An error has its location, no program is left")
    {
        Cdp1802Assembler assembler;
        REQUIRE_FALSE(assembler.compile("blink.asm", TWO_SEGMENTS).resultType != CompileResult::eOK);
        const auto& result = assembler.compile("broken.asm", "        ORG #0010\n        LDI #12\n        FOO R2\n");
        CHECK(assembler.isError());
        CHECK(result.resultType == CompileResult::eERROR);
        CHECK_FALSE(result.errorMessage.empty());
        REQUIRE(result.locations.size() == 1);
        CHECK(result.locations.front().file == "broken.asm");
        CHECK(result.locations.front().line == 3);
        CHECK(result.locations.front().column > 0);
        CHECK(assembler.segments().empty());
        CHECK(assembler.codeSize() == 0);
        CHECK_FALSE(assembler.lineForAddr(0x10).has_value());
        // a missing file
        const auto& missing = assembler.compile((std::filesystem::path(CHIPLET_TEST_SOURCE_DIR) / "test" / "no-such-file.asm").string());
        CHECK(missing.resultType == CompileResult::eERROR);
        CHECK(missing.errorMessage.find("no-such-file.asm") != std::string::npos);
    }

    TEST_CASE("A later segment over the same address wins")
    {
        Cdp1802Assembler assembler;
        REQUIRE_FALSE(assembler.compile("over.asm", "        ORG #0010\n        SEQ\n        REQ\n        ORG #0011\n        IDL\n").resultType != CompileResult::eOK);
        CHECK(assembler.lineForAddr(0x10) == 2u);
        CHECK(assembler.lineForAddr(0x11) == 5u);
        CHECK(codeOf(assembler) == std::vector<uint8_t>{0x7B, 0x00});
    }

    TEST_CASE("The reference sources assemble to their binaries")
    {
        const auto folder = std::filesystem::path(CHIPLET_TEST_SOURCE_DIR) / "test" / "cdp1802-tests";
        int sources = 0;
        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.path().extension() != ".asm")
                continue;
            ++sources;
            CAPTURE(entry.path().string());
            Cdp1802Assembler assembler;
            assembler.setListing(true);
            const auto& result = assembler.compile(entry.path().string());
            REQUIRE_MESSAGE(!assembler.isError(), result.errorMessage);
            CHECK(codeOf(assembler) == readBinary(std::filesystem::path(entry.path()).replace_extension(".bin")));
            CHECK_FALSE(assembler.listing().empty());
            // every byte of the program has a line
            for (const auto& segment : assembler.segments())
                CHECK(std::ranges::none_of(segment.lines, [](uint32_t line) { return line == 0; }));
        }
        CHECK(sources >= 3);
    }
}
