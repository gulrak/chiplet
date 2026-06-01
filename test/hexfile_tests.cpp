//
// Created by Steffen Schümann on 01.06.26.
//
#include <doctest/doctest.h>

#include <chiplet/hexfile.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <vector>

namespace {

std::filesystem::path tempPath(const std::string& name)
{
    return std::filesystem::temp_directory_path() / name;
}

std::string readText(const std::filesystem::path& path)
{
    std::ifstream input{path};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

} // namespace

TEST_SUITE("HexFile")
{
    TEST_CASE("writes and reads multiple continuous ranges")
    {
        const auto path = tempPath("chiplet-hexfile-ranges.hex");
        const std::array<uint8_t, 3> first{0x12, 0x34, 0x56};
        const std::array<uint8_t, 2> adjacent{0x78, 0x9A};
        const std::array<uint8_t, 2> second{0xBC, 0xDE};

        {
            emu::HexFile file{path.string(), emu::HexFile::Mode::WRITE};
            file.write(0x0200, first);
            file.write(0x0203, adjacent);
            file.write(0x0300, second);
            file.close();
        }

        emu::HexFile file{path.string(), emu::HexFile::Mode::READ};
        REQUIRE_EQ(file.ranges().size(), 2);
        CHECK_EQ(file.ranges()[0].first, 0x0200);
        CHECK_EQ(std::vector<uint8_t>{file.ranges()[0].second.begin(), file.ranges()[0].second.end()}, std::vector<uint8_t>{0x12, 0x34, 0x56, 0x78, 0x9A});
        CHECK_EQ(file.ranges()[1].first, 0x0300);
        CHECK_EQ(std::vector<uint8_t>{file.ranges()[1].second.begin(), file.ranges()[1].second.end()}, std::vector<uint8_t>{0xBC, 0xDE});
    }

    TEST_CASE("writes EOF record on destruction")
    {
        const auto path = tempPath("chiplet-hexfile-eof.hex");
        const std::array<uint8_t, 1> data{0x00};

        {
            emu::HexFile file{path.string(), emu::HexFile::Mode::WRITE};
            file.write(0x0000, data);
        }

        CHECK(readText(path).ends_with(":00000001FF\n"));
    }

    TEST_CASE("splits large writes into valid records")
    {
        const auto path = tempPath("chiplet-hexfile-large.hex");
        std::vector<uint8_t> data(300);
        std::iota(data.begin(), data.end(), 0);

        {
            emu::HexFile file{path.string(), emu::HexFile::Mode::WRITE};
            file.write(0x1000, data);
        }

        emu::HexFile file{path.string(), emu::HexFile::Mode::READ};
        REQUIRE_EQ(file.ranges().size(), 1);
        CHECK_EQ(file.ranges()[0].first, 0x1000);
        CHECK_EQ(std::vector<uint8_t>{file.ranges()[0].second.begin(), file.ranges()[0].second.end()}, data);
    }

    TEST_CASE("hashes loaded data as one zero-filled binary span")
    {
        const auto path = tempPath("chiplet-hexfile-sha1.hex");
        const std::array<uint8_t, 1> first{0xAA};
        const std::array<uint8_t, 1> second{0xBB};

        {
            emu::HexFile file{path.string(), emu::HexFile::Mode::WRITE};
            file.write(0x0100, first);
            file.write(0x0103, second);
        }

        emu::HexFile file{path.string(), emu::HexFile::Mode::READ};
        CHECK_EQ(file.sha1Digest(), Sha1::Digest{"d8a87c116f38284cb00fad8d32fb440315226ce0"});
    }

    TEST_CASE("writes records using uppercase Intel HEX formatting")
    {
        const auto path = tempPath("chiplet-hexfile-format.hex");
        const std::array<uint8_t, 3> data{0x0A, 0xBC, 0xF0};

        {
            emu::HexFile file{path.string(), emu::HexFile::Mode::WRITE};
            file.write(0x1234, data);
            file.close();
        }

        CHECK_EQ(readText(path), ":031234000ABCF001\n:00000001FF\n");
    }

    TEST_CASE("rejects unsupported records and checksum errors")
    {
        const auto unsupportedPath = tempPath("chiplet-hexfile-unsupported.hex");
        {
            std::ofstream output{unsupportedPath};
            output << ":020000021234B6\n";
            output << ":00000001FF\n";
        }
        CHECK_THROWS_AS(emu::HexFile(unsupportedPath.string(), emu::HexFile::Mode::READ), std::runtime_error);

        const auto checksumPath = tempPath("chiplet-hexfile-checksum.hex");
        {
            std::ofstream output{checksumPath};
            output << ":0100000000FE\n";
            output << ":00000001FF\n";
        }
        CHECK_THROWS_AS(emu::HexFile(checksumPath.string(), emu::HexFile::Mode::READ), std::runtime_error);
    }
}
