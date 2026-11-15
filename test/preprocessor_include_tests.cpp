#include <doctest/doctest.h>

#include <chiplet/octocompiler.hpp>

#include <filesystem>
#include <span>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

void writeFile(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    output << content;
}

std::string normalized(const fs::path& path)
{
    return fs::absolute(path).lexically_normal().string();
}

// A project with a nested include, a binary include and an include found through the include paths, each test case uses
// its own folder, as ctest runs them in parallel
struct Project
{
    fs::path root;
    fs::path main;

    explicit Project(const std::string& name)
        : root(fs::temp_directory_path() / ("chiplet-include-test-" + name))
        , main(root / "src" / "main.8o")
    {
        fs::remove_all(root);
        writeFile(main, ": main\n  :include \"lib/sprites.8o\"\n  :include \"shared.8o\"\n  loop again\n:segment data\n:include \"data.bin\" no-labels\n");
        writeFile(root / "src" / "lib" / "sprites.8o", "  v0 := 1\n  :include \"more.8o\"\n");
        writeFile(root / "src" / "lib" / "more.8o", "  v1 := 2\n");
        writeFile(root / "src" / "data.bin", "\x12\x34");
        writeFile(root / "inc" / "shared.8o", "  v2 := 3\n");
    }

    ~Project() { fs::remove_all(root); }

    std::vector<std::string> expectedIncludes() const
    {
        return {normalized(root / "src" / "lib" / "sprites.8o"), normalized(root / "src" / "lib" / "more.8o"), normalized(root / "inc" / "shared.8o"),
                normalized(root / "src" / "data.bin")};
    }
};

}  // namespace

TEST_SUITE("Preprocessor includes")
{
    TEST_CASE("The compile result lists the included files, also ones found through the include paths")
    {
        Project project{"listed"};
        emu::OctoCompiler compiler;
        compiler.setIncludePaths({(project.root / "inc").string()});
        compiler.compile(project.main.string());
        CHECK_MESSAGE(!compiler.isError(), compiler.compileResult().errorMessage);
        CHECK(compiler.compileResult().includedFiles == project.expectedIncludes());
    }

    TEST_CASE("Compiling a source given as text lists its includes too")
    {
        Project project{"text"};
        std::ifstream input{project.main, std::ios::binary};
        const std::string source{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        emu::OctoCompiler compiler;
        compiler.setIncludePaths({(project.root / "inc").string()});
        compiler.compile(project.main.string(), std::span<const char>(source.data(), source.size()));
        CHECK_MESSAGE(!compiler.isError(), compiler.compileResult().errorMessage);
        CHECK(compiler.compileResult().includedFiles == project.expectedIncludes());
        compiler.reset();
        CHECK(compiler.compileResult().includedFiles.empty());
    }

    TEST_CASE("An error in an included file keeps the files included up to it")
    {
        Project project{"error"};
        writeFile(project.root / "src" / "lib" / "more.8o", "  v1 := \n");
        emu::OctoCompiler compiler;
        compiler.setIncludePaths({(project.root / "inc").string()});
        compiler.compile(project.main.string());
        CHECK(compiler.isError());
        const auto& included = compiler.compileResult().includedFiles;
        REQUIRE(included.size() >= 2);
        CHECK(included[1] == normalized(project.root / "src" / "lib" / "more.8o"));
    }

    TEST_CASE("The listing shows the bytes on their lines in the included files")
    {
        Project project{"listing"};
        emu::OctoCompiler compiler;
        compiler.setIncludePaths({(project.root / "inc").string()});
        compiler.compile(project.main.string());
        REQUIRE_MESSAGE(!compiler.isError(), compiler.compileResult().errorMessage);
        const auto listing = compiler.listing();
        auto has = [&](const std::string& text) {
            INFO(listing);
            CHECK(listing.find(text) != std::string::npos);
        };
        has("; lib/sprites.8o\n0200  60 01                        1    v0 := 1\n");
        has("; lib/more.8o\n0202  61 02                        1    v1 := 2\n");
        has("; ../inc/shared.8o\n0204  62 03                        1    v2 := 3\n");
        has("0206  12 06                        4    loop again\n");
        // the generated data of the binary include in the data segment is on the line of the include
        has("0208  12 34                        6  ");
        // the lines without bytes are there too
        has("                                   1  : main\n");
        compiler.reset();
        CHECK(compiler.listing().empty());
    }

    TEST_CASE("The listing puts the bytes of macros on the line using them and splits long lines")
    {
        const std::string source = ":macro twice X {\n  X += 1\n  X += 1\n}\n: main\n  twice v3\n  v4 := 7\n  twice v5\n"
                                   ": data\n  1 2 3 4 5 6 7 8 9 10\n";
        emu::OctoCompiler compiler;
        compiler.compile("macros.8o", std::span<const char>(source.data(), source.size()));
        REQUIRE_MESSAGE(!compiler.isError(), compiler.compileResult().errorMessage);
        const auto listing = compiler.listing();
        INFO(listing);
        CHECK(listing.find("0200  73 01 73 01                  6    twice v3\n") != std::string::npos);
        CHECK(listing.find("0204  64 07                        7    v4 := 7\n") != std::string::npos);
        CHECK(listing.find("0206  75 01 75 01                  8    twice v5\n") != std::string::npos);
        CHECK(listing.find("020A  01 02 03 04 05 06 07 08     10    1 2 3 4 5 6 7 8 9 10\n0212  09 0A\n") != std::string::npos);
    }

    TEST_CASE("Every row generated by an include belongs to the include line, the source after it keeps its lines")
    {
        Project project{"generated"};
        std::string bytes;
        for (int value = 0; value < 20; ++value)
            bytes += static_cast<char>(value);
        writeFile(project.root / "src" / "big.bin", bytes);
        writeFile(project.main, ": main\n  loop again\n\n:include \"big.bin\"\n  v0 := 1\n");
        emu::OctoCompiler compiler;
        compiler.compile(project.main.string());
        REQUIRE_MESSAGE(!compiler.isError(), compiler.compileResult().errorMessage);
        const auto listing = compiler.listing();
        INFO(listing);
        CHECK(listing.find("0202  00 01 02 03 04 05 06 07      4  : big ") != std::string::npos);
        CHECK(listing.find("0212  10 11 12 13                  4  ") != std::string::npos);
        CHECK(listing.find("0216  60 01                        5    v0 := 1\n") != std::string::npos);
        CHECK(compiler.lineForAddr(0x216) != compiler.lineForAddr(0x212));
    }

    TEST_CASE("The jump to a main that isn't at the start is listed first, with the main line")
    {
        auto listingOf = [](const std::string& source) {
            emu::OctoCompiler compiler;
            compiler.compile("main.8o", std::span<const char>(source.data(), source.size()));
            REQUIRE_MESSAGE(!compiler.isError(), compiler.compileResult().errorMessage);
            return compiler.listing();
        };
        const auto later = listingOf(": helper\n  return\n: main\n  helper\n  loop again\n");
        INFO(later);
        CHECK(later.starts_with("; main.8o\n0200  12 04                        3  (jump to main)\n"));
        CHECK(later.find("0202  00 EE                        2    return\n") != std::string::npos);
        CHECK(later.find("                                   3  : main\n") != std::string::npos);
        // at the start there is no jump, main's own code is there
        const auto first = listingOf(": main\n  v0 := 1\n");
        INFO(first);
        CHECK(first.find("                                   1  : main\n0200  60 01                        2    v0 := 1\n") != std::string::npos);
    }

    TEST_CASE("A missing include is reported at the include")
    {
        Project project{"missing"};
        emu::OctoCompiler compiler;
        compiler.compile(project.main.string());  // without the include path shared.8o is missing
        CHECK(compiler.isError());
        CHECK(compiler.compileResult().errorMessage.find("shared.8o") != std::string::npos);
        REQUIRE(!compiler.compileResult().locations.empty());
        CHECK(compiler.compileResult().locations.back().line == 3);
    }
}
