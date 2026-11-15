#include <doctest/doctest.h>

#include <chiplet/asacpp.hpp>

#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace {

struct TempFolder
{
    TempFolder()
    {
        const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        for (int attempt = 0;; ++attempt) {
            path = std::filesystem::temp_directory_path() / ("chiplet-asacpp-test-" + stamp + "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(path))
                break;
        }
    }
    ~TempFolder()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
    std::string write(const std::string& name, const std::string& text) const
    {
        const auto file = (path / name).string();
        std::ofstream(file, std::ios::binary) << text;
        return file;
    }
    std::filesystem::path path;
};

std::vector<uint8_t> codeOf(const emu::AsaCpp& assembler)
{
    const auto code = assembler.code();
    return {code.begin(), code.end()};
}

}  // namespace

TEST_SUITE("AsaCpp")
{
    TEST_CASE("Saturn sources in the ASAP syntax assemble, includes are reported")
    {
        TempFolder folder;
        const auto lib = folder.write("lib.asap", "; a library\n        add.a   5,d0\n");
        const auto main = folder.write("prog.asap", "        data.b  'H'\n        data.b  'P'\n        move.a  @d0,a\n        include \"lib.asap\"\n        if FAST\n        jump.a  @a\n        endif\n");

        emu::AsaCpp assembler;
        assembler.define("FAST", 0);
        const auto& result = assembler.compile(main);
        REQUIRE_MESSAGE(!assembler.isError(), result.errorMessage);
        CHECK(result.resultType == emu::CompileResult::eOK);
        const auto code = codeOf(assembler);
        REQUIRE(code.size() >= 2);
        CHECK(code[0] == 'H');
        CHECK(code[1] == 'P');
        REQUIRE(result.includedFiles.size() == 1);
        CHECK(std::filesystem::path(result.includedFiles.front()) == std::filesystem::absolute(lib).lexically_normal());
        CHECK(assembler.includedFiles() == result.includedFiles);
        CHECK_FALSE(assembler.listing().empty());

        // a define switches the conditional part in
        emu::AsaCpp fast;
        fast.define("FAST", 1);
        REQUIRE_FALSE(fast.compile(main).resultType == emu::CompileResult::eERROR);
        CHECK(codeOf(fast).size() > code.size());
        CHECK(fast.sha1() != assembler.sha1());
    }

    TEST_CASE("An error in an included file has the chain of files")
    {
        TempFolder folder;
        const auto lib = folder.write("lib.asap", "; a library\n        frobnicate.a c\n");
        const auto main = folder.write("prog.asap", "        move.a  @d0,a\n        include \"lib.asap\"\n");
        emu::AsaCpp assembler;
        const auto& result = assembler.compile(main);
        CHECK(assembler.isError());
        CHECK(result.resultType == emu::CompileResult::eERROR);
        CHECK(assembler.code().empty());
        // outermost first, the error is in the last one
        REQUIRE_FALSE(result.locations.empty());
        CHECK(result.locations.back().line == 2);
        CHECK(result.locations.back().file.ends_with("lib.asap"));
        CHECK(result.errorMessage.find("lib.asap:2:") != std::string::npos);
    }

    TEST_CASE("An error location keeps the drive letter of a Windows path")
    {
        emu::AsaCpp assembler;
        const std::string source = "        move.a  @d0,a\n        frobnicate.a c\n";
        const auto& result = assembler.compile("C:\\project\\prog.asap", std::span<const char>(source.data(), source.size()));
        CHECK(assembler.isError());
        REQUIRE(result.locations.size() == 1);
        // the separators are the platform's business, the drive letter must not be lost
        CHECK(result.locations.front().file.starts_with("C:"));
        CHECK(result.locations.front().file.ends_with("prog.asap"));
        CHECK(result.locations.front().line == 2);
    }
}
