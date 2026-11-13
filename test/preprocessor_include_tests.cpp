#include <doctest/doctest.h>

#include <chiplet/octocompiler.hpp>

#include <filesystem>
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
