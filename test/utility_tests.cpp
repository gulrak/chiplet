//
// Created by schuemann on 24.01.25.
//
#include <doctest/doctest.h>

#include <chiplet/utility.hpp>
#include <chiplet/chip8meta.hpp>
#include <chiplet/chip8variants.hpp>
#include <ghc/cli.hpp>

#include <string>
#include <vector>

TEST_CASE("toOptionName - category transitions and kebab-case format")
{
    SUBCASE("Empty string remains empty") {
        CHECK(toOptionName("") == "");
    }

    SUBCASE("Pure lowercase is unchanged") {
        CHECK(toOptionName("simple") == "simple");
    }

    SUBCASE("Lower to upper: insert hyphen") {
        CHECK(toOptionName("myVar") == "my-var");
    }

    SUBCASE("Upper to lower: no direct hyphen, but next is lower anyway") {
        CHECK(toOptionName("UserID") == "user-id");
    }

    SUBCASE("Digit transitions: letter->digit and digit->letter") {
        CHECK(toOptionName("size10") == "size-10");
        CHECK(toOptionName("10Size") == "10-size");
        CHECK(toOptionName("size123other") == "size-123-other");
    }

    SUBCASE("Non-alphanumeric replaced by hyphens, collapsed if multiple") {
        CHECK(toOptionName("my_var__99Test") == "my-var-99-test");
        CHECK(toOptionName("my--special**string") == "my-special-string");
    }

    SUBCASE("Trim leading/trailing hyphens") {
        CHECK(toOptionName("-leading-") == "leading");
        CHECK(toOptionName("trailing-") == "trailing");
    }

    SUBCASE("Composite example") {
        CHECK(toOptionName("someMixedNumb3rCase__42 andMore")
              == "some-mixed-numb-3-r-case-42-and-more");
    }
}

TEST_CASE("toJsonKey - category transitions and camelCase format")
{
    SUBCASE("Empty string remains empty") {
        CHECK(toJsonKey("") == "");
    }

    SUBCASE("Pure lowercase is unchanged") {
        CHECK(toJsonKey("simple") == "simple");
    }

    SUBCASE("Lower to upper: insert upper") {
        CHECK(toJsonKey("myVar") == "myVar");
    }

    SUBCASE("Upper to lower: insert lower") {
        CHECK(toJsonKey("UserID") == "userId");
    }

    SUBCASE("Digit transitions: letter->digit and digit->letter") {
        CHECK(toJsonKey("size10") == "size10");
        CHECK(toJsonKey("10Size") == "10Size");
        CHECK(toJsonKey("size123other") == "size123Other");
    }

    SUBCASE("Non-alphanumeric replaced by following be an upper if letter, collapsed if multiple") {
        CHECK(toJsonKey("my_var__99Test") == "myVar99Test");
        CHECK(toJsonKey("my--special**string") == "mySpecialString");
    }

    SUBCASE("Trim leading/trailing special characters") {
        CHECK(toJsonKey("-leading-") == "leading");
        CHECK(toJsonKey("trailing-") == "trailing");
    }

    SUBCASE("Composite example") {
        CHECK(toJsonKey("someMixedNumb3rCase__42 andMore")
              == "someMixedNumb3RCase42AndMore");
    }
}

TEST_CASE("VariantSet - iteration visits exactly the set variants")
{
    using emu::chip8::Variant;
    using emu::chip8::VariantSet;

    SUBCASE("Empty set yields no elements") {
        VariantSet vs{};
        CHECK(vs.begin() == vs.end());
    }

    SUBCASE("Variants across both words are visited in ascending order") {
        VariantSet vs = Variant::CHIP_8 | Variant::CHIP_48 | Variant::MEGA_CHIP | Variant::COSMAC_VIP | Variant::CHIP_8_TPD_TS_COSMAC_VIP | Variant::GENERIC_CHIP_8;
        std::vector<Variant> visited(vs.begin(), vs.end());
        std::vector<Variant> expected{Variant::CHIP_8, Variant::CHIP_48, Variant::MEGA_CHIP, Variant::COSMAC_VIP, Variant::CHIP_8_TPD_TS_COSMAC_VIP, Variant::GENERIC_CHIP_8};
        CHECK(visited == expected);
    }

    SUBCASE("Only upper word populated") {
        VariantSet vs = Variant::CHIP_8_COSMAC_VIP | Variant::GENERIC_CHIP_8;
        std::vector<Variant> visited;
        for (auto v : vs) {
            visited.push_back(v);
        }
        CHECK(visited == std::vector<Variant>{Variant::CHIP_8_COSMAC_VIP, Variant::GENERIC_CHIP_8});
        CHECK(vs.lowestVariant() == Variant::CHIP_8_COSMAC_VIP);
    }

    SUBCASE("Iteration agrees with contains() for all variants") {
        auto vs = emu::chip8::ALL_VARIANTS;
        std::size_t count = 0;
        for (auto v : vs) {
            CHECK(vs.contains(v));
            ++count;
        }
        std::size_t expectedCount = 0;
        for (std::size_t i = 0; i < 128; ++i) {
            if (vs.contains(static_cast<Variant>(i))) {
                ++expectedCount;
            }
        }
        CHECK(count == expectedCount);
    }
}

TEST_CASE("OpcodeSet formats Bxnn with its full address")
{
    using emu::chip8::Variant;
    for (const auto variant : {Variant::CHIP_48, Variant::SCHIP_1_0, Variant::SCHIP_1_1, Variant::SCHIP_1_1_SCRUP, Variant::SCHIP_MODERN}) {
        const emu::detail::OpcodeSet opcodes{emu::chip8::VariantSet(variant)};
        const auto [size, opcode, text] = opcodes.formatOpcode(0xB216);
        CHECK(text == "jump0 0x216 + v2");
    }
}

TEST_CASE("CLI bool options follow their --no- form")
{
    std::vector<std::string> args{"test", "--plain", "--negated", "--no-set", "--no-unset", "--explicit", "false", "--no-callback", "--no-enabled"};
    std::vector<char*> argv;
    for (auto& arg : args)
        argv.push_back(arg.data());
    bool plain = false, negated = false, set = true, unset = false, isExplicit = true, enabled = false, callback = true;
    ghc::CLI cli(static_cast<int>(argv.size()), argv.data());
    cli.option({"--plain"}, plain);
    cli.option({"--negated"}, negated);
    cli.option({"--set"}, set);
    cli.option({"--unset"}, unset);
    cli.option({"--explicit"}, isExplicit);
    cli.option<bool>({"--callback"}, [&callback](const std::string&, bool& value) { callback = value; });
    cli.optionEnable({"--no-enabled"}, enabled);
    cli.parse();
    CHECK(plain);
    CHECK(negated);
    CHECK_FALSE(set);
    CHECK_FALSE(unset);
    CHECK_FALSE(isExplicit);
    CHECK_FALSE(callback);
    CHECK(enabled);
}
