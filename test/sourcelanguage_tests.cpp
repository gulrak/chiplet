#include <doctest/doctest.h>

#include <chiplet/sourcelanguage.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using emu::SourceLanguage;

namespace {

std::string readText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::vector<std::filesystem::path> filesWithExtension(const std::filesystem::path& folder, const std::string& extension)
{
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.path().extension() == extension)
            files.push_back(entry.path());
    }
    return files;
}

SourceLanguage languageOf(std::string_view text)
{
    return emu::detectSourceLanguage("program.asm", text).language;
}

}  // namespace

TEST_SUITE("SourceLanguage")
{
    TEST_CASE("The extension decides for .8o and .asap")
    {
        CHECK(emu::sourceLanguageFromExtension("game.8o") == SourceLanguage::OCTO);
        CHECK(emu::sourceLanguageFromExtension("dir.x/GAME.8O") == SourceLanguage::OCTO);
        CHECK(emu::sourceLanguageFromExtension("schip.asap") == SourceLanguage::SATURN);
        CHECK(emu::sourceLanguageFromExtension("C:\\src\\monitor.asm") == SourceLanguage::UNKNOWN);
        CHECK(emu::sourceLanguageFromExtension(".8o") == SourceLanguage::UNKNOWN);
        CHECK(emu::sourceLanguageFromExtension("noextension") == SourceLanguage::UNKNOWN);
        // the extension wins over the text
        const auto guess = emu::detectSourceLanguage("lib.8o", "        LDI #00\n        PLO R2\n");
        CHECK(guess.language == SourceLanguage::OCTO);
        CHECK(guess.byExtension);
    }

    TEST_CASE("Names and ids")
    {
        for (auto language : {SourceLanguage::OCTO, SourceLanguage::CDP1802, SourceLanguage::M6800, SourceLanguage::SATURN}) {
            REQUIRE(emu::sourceLanguageFromId(emu::sourceLanguageId(language)).has_value());
            CHECK(*emu::sourceLanguageFromId(emu::sourceLanguageId(language)) == language);
        }
        CHECK_FALSE(emu::sourceLanguageFromId("auto").has_value());
        CHECK(std::string(emu::sourceLanguageName(SourceLanguage::SATURN)) == "Saturn");
    }

    TEST_CASE("The CDP1802 test sources are CDP1802 assembly")
    {
        const auto files = filesWithExtension(std::filesystem::path(CHIPLET_TEST_SOURCE_DIR) / "test" / "cdp1802-tests", ".asm");
        REQUIRE_FALSE(files.empty());
        for (const auto& file : files) {
            CAPTURE(file.string());
            const auto guess = emu::detectSourceLanguage(file.string(), readText(file));
            CHECK(guess.language == SourceLanguage::CDP1802);
            CHECK_FALSE(guess.byExtension);
            CHECK(guess.statementsFor(SourceLanguage::CDP1802) > 100);
        }
    }

    TEST_CASE("Octo sources are Octo whatever their name is")
    {
        const auto files = filesWithExtension(std::filesystem::path(CHIPLET_TEST_SOURCE_DIR) / "test" / "c-octo-tests", ".8o");
        REQUIRE_FALSE(files.empty());
        for (const auto& file : files) {
            // the error cases are a broken line or two, the empty source says nothing
            const auto name = file.filename().string();
            if (name.starts_with("err_") || name == "empty.8o")
                continue;
            CAPTURE(file.string());
            CHECK(languageOf(readText(file)) == SourceLanguage::OCTO);
        }
        // native code in an Octo program doesn't make it CDP1802 assembly
        CHECK(languageOf(": main\n    v0 := 1\n    native inc_bgcol\n    loop again\n"
                         ": inc_bgcol\n:asm cdp1802 {\n    GLO R2\n    PLO RF\n    SEP R4\n}\n") == SourceLanguage::OCTO);
    }

    TEST_CASE("CDP1802 assembly in the RCA syntax")
    {
        CHECK(languageOf(".. blink Q\nSTART:  SEQ\n        LDI#10 ; PLO R7\nWAIT:   DEC R7\n        GLO R7\n        BNZ WAIT\n"
                         "        REQ\n        BR START\n") == SourceLanguage::CDP1802);
        // lower case, a label in the first column without a colon
        CHECK(languageOf("loop    ghi r1\n        phi rb\n        sex r2\n        out 4\n        dis,#23\n") == SourceLanguage::CDP1802);
    }

    TEST_CASE("Motorola 6800 assembly")
    {
        CHECK(languageOf("; count down\nCOUNT   EQU     $0020\n        ORG     $0200\nSTART   LDAA    #$10\n        STAA    COUNT\n"
                         "LOOP    DEC     COUNT\n        BNE     LOOP\n        JSR     $C000\n        RTS\nTABLE   FCB     1,2,3\n") ==
              SourceLanguage::M6800);
        // the "LDA A" spelling, '*' comment lines
        CHECK(languageOf("* clear the screen\n        LDA A   #0\n        LDX     #$0100\nCLEAR   STA A   0,X\n        INX\n"
                         "        CPX     #$0200\n        BNE     CLEAR\n        RTS\n") == SourceLanguage::M6800);
    }

    TEST_CASE("Saturn assembly in the ASAP syntax")
    {
        CHECK(languageOf("; copy a field\n        o_vars:=0\ncopy:   move.a  @d0,a\n        add.a   5,d0\n        move.a  a,@d1\n"
                         "        brne.a  a,c,copy\n        retclrc\n") == SourceLanguage::SATURN);
        // the same as 6800 or 1802 words without a size suffix are no Saturn statements
        const auto guess = emu::detectSourceLanguageFromText("        ret\n        add\n        and\n");
        CHECK(guess.statementsFor(SourceLanguage::SATURN) == 0);
    }

    TEST_CASE("Unclear sources are UNKNOWN")
    {
        CHECK(languageOf("") == SourceLanguage::UNKNOWN);
        CHECK(languageOf("; just a comment\n\n") == SourceLanguage::UNKNOWN);
        // INC and NOP are 1802 and 6800 instructions
        const auto guess = emu::detectSourceLanguageFromText("        INC R1\n        NOP\n");
        CHECK(guess.language == SourceLanguage::UNKNOWN);
        CHECK(guess.statementsFor(SourceLanguage::CDP1802) == 2);
        CHECK(guess.statementsFor(SourceLanguage::M6800) == 2);
        // one language needs twice the statements of every other one
        CHECK(languageOf("        INC R1\n        NOP\n        GHI R1\n        PLO R2\n") == SourceLanguage::CDP1802);
        CHECK(languageOf("        INC R1\n        NOP\n        GHI R1\n") == SourceLanguage::UNKNOWN);
    }
}
