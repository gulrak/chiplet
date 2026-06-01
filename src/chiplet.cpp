//---------------------------------------------------------------------------------------
// src/chiplet.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2023, Steffen Schümann <s.schuemann@pobox.com>
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

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <chiplet/octocompiler.hpp>
#include <chiplet/chip8decompiler.hpp>

#include <chiplet/hexfile.hpp>
#include <chiplet/utility.hpp>
#include <chiplet/sha1.hpp>
#include <chiplet/octocartridge.hpp>

#include "cdp1802/assembly_session.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <../external/nothings/stb_image.h>

#include <ghc/cli.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <set>
#include <stdexcept>

enum WorkMode { ePREPROCESS, eCOMPILE, eDISASSEMBLE, eANALYSE, eSEARCH, eDEEP_ANALYSE };
static std::unordered_map<std::string, std::string> fileMap;
static std::set<std::string> extensionsDetected;
static std::set<std::string> extensionsUndetected;
static std::vector<std::string> opcodesToFind;
static std::string outputFile;
static bool fullPath = false;
static bool withUsage = false;
static bool genListing = false;
static bool deepscan = false;
static int foundFiles = 0;
static bool roundTrip = false;
static int errors = 0;
static int64_t totalSourceLines = 0;
static int64_t totalDecompileTime_us = 0;
static int64_t totalAssembleTime_us = 0;

enum class Architecture { OCTO, CDP1802 };

Architecture architectureFromName(const std::string& name)
{
    if (name == "octo")
        return Architecture::OCTO;
    if (name == "cdp1802")
        return Architecture::CDP1802;
    throw std::runtime_error("Unsupported architecture '" + name + "' (expected 'octo' or 'cdp1802')");
}

bool writeBinaryOutput(const std::string& filename, std::span<const uint8_t> data)
{
    std::ofstream out(filename, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

bool writeHexOutput(const std::string& filename, uint16_t address, std::span<const uint8_t> data)
{
    emu::HexFile hex{filename, emu::HexFile::Mode::WRITE};
    hex.write(address, data);
    hex.close();
    return true;
}

bool writeHexOutput(const std::string& filename, const std::vector<cdp1802::AssemblySegment>& segments)
{
    emu::HexFile hex{filename, emu::HexFile::Mode::WRITE};
    for (const auto& segment : segments)
        hex.write(segment.address, segment.bytes);
    hex.close();
    return true;
}

std::string listingFilenameForOutput(const std::string& filename)
{
    auto path = fs::path(filename);
    path.replace_extension(".lst");
    return path.string();
}

bool isChipRom(const std::string& name)
{
    static std::set<std::string> validExtensions{ ".ch8", ".ch10", ".hc8", ".c8h", ".c8e", ".c8x", ".sc8", ".mc8", ".xo8", ".c8", ".o8" };
    return validExtensions.count(name) > 0;
}

std::string fileOrPath(const std::string& file)
{
    return fullPath ? file : fs::path(file).filename().string();
}

void workFile(WorkMode mode, const std::string& file, const std::vector<uint8_t>& data)
{
    if(data.empty())
        return;

    constexpr auto allowedVariants = (emu::C8V::CHIP_8 | emu::C8V::CHIP_8X | emu::C8V::CHIP_8X_TPD | emu::C8V::HI_RES_CHIP_8X | emu::C8V::CHIP_10 | emu::C8V::CHIP_48 | emu::C8V::SCHIP_1_0 | emu::C8V::SCHIP_1_1 | emu::C8V::MEGA_CHIP | emu::C8V::XO_CHIP);
    uint16_t startAddress = endsWith(file, ".c8x") ? 0x300 : 0x200;
    emu::Chip8Decompiler dec{data, startAddress};
    switch(mode) {
        case eDISASSEMBLE:
            if(roundTrip) {
                std::stringstream os;
                emu::OctoCompiler comp;
                auto decStart = std::chrono::steady_clock::now();
                dec.decompile(file, startAddress, &os, false, true);
                auto decEnd = std::chrono::steady_clock::now();
                if(dec.possibleVariants() == emu::C8V::CHIP_8X || dec.possibleVariants() == emu::C8V::HI_RES_CHIP_8X)
                    startAddress = 0x300;
                else if(dec.possibleVariants() == emu::C8V::CHIP_8X_TPD)
                    startAddress = 0x260;
                else if(dec.possibleVariants() == emu::C8V::HI_RES_CHIP_8)
                    startAddress = 0x244;
                auto source = os.str();
                comp.setStartAddress(startAddress);
                if(comp.compile(file, source, false).resultType == emu::CompileResult::eOK) {
                    auto compEnd = std::chrono::steady_clock::now();
                    if(comp.codeSize() != data.size()) {
                        std::cerr << "    " << fileOrPath(file) << ": Compiled size doesn't match! (" << data.size() << " bytes)" << std::endl;
                        workFile(eANALYSE, file, data);
                        writeFile(fs::path(file).filename().string().c_str(), comp.code(), comp.codeSize());
                        ++errors;
                    }
                    else if(comp.sha1() != calculateSha1(data)) {
                        std::cerr << "    " << fileOrPath(file) << ": Compiled code doesn't match! (" << data.size() << " bytes)" << std::endl;
                        workFile(eANALYSE, file, data);
                        writeFile(fs::path(file).filename().string().c_str(), comp.code(), comp.codeSize());
                        ++errors;
                    }
                    else {
                        totalSourceLines += comp.numSourceLines();
                        auto decompTime = std::chrono::duration_cast<std::chrono::microseconds>(decEnd - decStart).count();
                        auto assemTime = std::chrono::duration_cast<std::chrono::microseconds>(compEnd - decEnd).count();
                        totalDecompileTime_us += decompTime;
                        totalAssembleTime_us += assemTime;
                        std::clog << "    " << fileOrPath(file) << " [" << decompTime << "us/" << assemTime << "us]" << std::endl;
                    }
                }
                else {
                    std::cerr << "    " << fileOrPath(file) << ": Source doesn't compile: " << comp.compileResult().errorMessage << std::endl;
                    workFile(eANALYSE, file, data);
                    ++errors;
                }
            }
            else {
                if(outputFile.empty())
                    dec.decompile(file, startAddress, &std::cout);
                else {
                    std::ofstream out(outputFile);
                    dec.decompile(file, startAddress, &out);
                }
            }
            break;
        case eANALYSE:
            std::cout << "    " << fileOrPath(file);
            dec.decompile(file, startAddress, &std::cout, true);
            if(!dec.possibleVariants().is_empty()) {
                bool first = true;
                for (auto variant : dec.possibleVariants()) {
                    //std::cout << (first ? ", possible variants: " : ", ") << dec.chipVariantName(variant).first;
                    first = false;
                }
                std::cout << std::endl;
            }
            else {
                std::cout << ", doesn't seem to be supported by any know variant." << std::endl;
            }
            if(dec.usesOddPcAddress())
                std::cout << "    Uses odd PC access." << std::endl;
            break;
        case eSEARCH: {
            dec.decompile(file, startAddress, &std::cout, true, true);
            bool found = false;
            for (const auto& pattern : opcodesToFind) {
                for (const auto& [opcode, count] : dec.fullStats()) {
                    std::string hex = fmt::format("{:04X}", opcode);
                    if (comparePattern(pattern, hex)) {
                        if(withUsage) {
                            if(!found)
                                std::cout << fileOrPath(file) << ":" << std::endl;
                            dec.listUsages(opcodeFromPattern(pattern), maskFromPattern(pattern), std::cout);
                            found = true;
                        }
                        else {
                            if (found)
                                std::cout << ", ";
                            std::cout << pattern;
                            found = true;
                            break;
                        }
                    }
                }
            }
            if(found) {
                ++foundFiles;
                if (!withUsage)
                    std::cout << ": " << fileOrPath(file) << std::endl;
            }
            break;
        }
        case eDEEP_ANALYSE: {
            if(data.size() > 4096 - startAddress) {
                if(data.size() <= 65536 - startAddress) {
                    dec.setVariants(emu::chip8::Variant::XO_CHIP | emu::chip8::Variant::MEGA_CHIP, true, true);
                }
                else {
                    dec.setVariants(emu::chip8::Variant::MEGA_CHIP, true, true);
                }
            }
            std::ostringstream msg;
            dec.decompile(file, startAddress, &msg, true);
            if(!dec.possibleVariants().is_empty()) {
                auto possibleVariants = dec.possibleVariants() & allowedVariants;
                bool first = true;
                if(!isChipRom(fs::path(file).extension().string())) {
                    for (auto variant : possibleVariants) {
                        std::cout << (first ? ", possible variants: " : ", ") << dec.chipVariantName(variant).first;
                        first = false;
                    }
                    extensionsDetected.emplace(fs::path(file).extension().string());
                    std::cout << std::endl;
                }
            }
            else {
                if(isChipRom(fs::path(file).extension().string())) {
                    std::cout << "    " << fileOrPath(file);
                    std::cout << ", doesn't seem to be supported by any know variant." << std::endl;
                    extensionsUndetected.emplace(fs::path(file).extension().string());
                }
            }
        }
        case ePREPROCESS:
        case eCOMPILE:
            // not handled here
            break;
    }
}

std::pair<bool, std::string> checkDouble(const std::string& file, const std::vector<uint8_t>& data)
{
    char hex[SHA1_HEX_SIZE];
    Sha1 sum;
    sum.add(data.data(), data.size());
    sum.finalize();
    sum.print_hex(hex);
    std::string digest = hex;
    auto iter = fileMap.find(digest);
    if(iter == fileMap.end()) {
        fileMap[digest] = file;
        return {false,""};
    }
    return {true, iter->second};
}

int disassembleOrAnalyze(bool scan, bool dumpDoubles, std::vector<std::string>& inputList, WorkMode& mode)
{
    auto start= std::chrono::steady_clock::now();
    uint64_t files = 0;
    uint64_t doubles = 0;
    for(const auto& input : inputList) {
        if(!fs::exists(input)) {
            std::cerr << "Couldn't find input file: " << input << std::endl;
            continue;
        }
        if(fs::is_directory(input)) {
            for(const auto& de : fs::recursive_directory_iterator(input, fs::directory_options::skip_permission_denied)) {
                if(de.is_regular_file() && (deepscan || isChipRom(de.path().extension().string()))) {
                    auto file = loadFile(de.path().string());
                    auto [isDouble, firstName] = checkDouble(de.path().string(), file.value_or(Bytes{}));
                    if(isDouble) {
                        ++doubles;
                        if(dumpDoubles)
                            std::clog << "File '" << de.path().string() << "' is identical to '" << firstName << "'" << std::endl;
                    }
                    else {
                        ++files;
                        workFile(mode, de.path().string(), *file);
                    }
                }
            }
        }
        else if(fs::is_regular_file(input) && (deepscan || isChipRom(fs::path(input).extension().string()))) {
            auto file = loadFile(input);
            auto [isDouble, firstName] = checkDouble(input, file.value_or(Bytes{}));
            if(isDouble) {
                ++doubles;
                if(dumpDoubles)
                    std::clog << "File '" << input << "' is identical to '" << firstName << "'" << std::endl;
            }
            else {
                ++files;
                workFile(mode, input, file.value_or(Bytes{}));
            }
        }
    }
    if(scan) {
        std::clog << "Used opcodes:" << std::endl;
        for(const auto& [opcode, num] : emu::Chip8Decompiler::totalStats()) {
            std::clog << fmt::format("{:04X}: {}", opcode, num) << std::endl;
        }
    }
    auto duration= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    std::cerr << std::flush;
    std::cout << std::flush;
    std::clog << "Done scanning/decompiling " << files << " files";
    if(doubles)
        std::clog << ", not counting " << doubles << " redundant copies";
    if(foundFiles)
        std::clog << ", found opcodes in " << foundFiles << " files";
    if(errors)
        std::clog << ", round trip errors: " << errors;
    if(totalSourceLines) {
        std::clog << ", total number of source lines assembled: " << totalSourceLines;
        std::clog << ", (d:" << totalDecompileTime_us/1000 << "ms/a:" << totalAssembleTime_us/1000 << "ms)";
    }
    std::clog << " (" << duration << "ms)" <<std::endl;
    if(deepscan) {
        std::cout << "File extensions of detected files: ";
        for(const auto& ext : extensionsDetected)
            std::cout << ext << " ";
        std::cout << std::endl;
        std::cout << "File extensions of undetected files: ";
        for(const auto& ext : extensionsUndetected)
            std::cout << ext << " ";
        std::cout << std::endl;
    }
    return errors ? 1 : 0;
}

int main(int argc, char* argv[])
{
    ghc::filesystem::u8arguments args(argc, argv);
#ifdef GHC_OS_WINDOWS
    SetConsoleOutputCP(CP_UTF8);
#endif
    using namespace std::chrono;
    using namespace std::chrono_literals;
    ghc::CLI cli(argc, argv);
    bool preprocess = false;
    bool disassemble = false;
    bool noLineInfo = false;
    bool quiet = false;
    bool verbose = false;
    bool version = false;
    bool scan = false;
    bool dumpDoubles = false;
    bool cartridgeBuild = false;
    std::string cartridgeLabel;
    std::string cartridgeImage;
    std::string cartridgeOptions;
    std::string cartridgeVariant;
    std::string architectureName{"octo"};
    int verbosity = 1;
    int rc = 0;
    int64_t startAddress = 0x200;
    bool hexOutput = false;
    std::vector<std::string> includePaths;
    std::vector<std::string> inputList;
    std::vector<std::string> defineList;
    cli.category("Assembler/Preprocessor");
    cli.option({"-a", "--arch"}, architectureName, "select assembler architecture: octo or cdp1802, default octo");
    cli.option({"-P", "--preprocess"}, preprocess, "only preprocess the file and output the result");
    cli.option({"-I", "--include-path"}, includePaths, "add directory to include search path");
    cli.option({"-D", "--define"}, defineList, "add a defined option to the preprocessor");
    cli.option({"-o", "--output"}, outputFile, "name of output file, default stdout for preprocessor, a.out.ch8 for binary");
    cli.option({"--hex"}, hexOutput, "write Intel HEX output instead of raw binary when compiling");
    cli.option({"--start-address"}, startAddress, "the address the program will be loaded to, the ': main' label address, default is 512");
    cli.optionEnable({"--no-line-info"}, noLineInfo, "omit generation of line info comments in the preprocessed output");
    cli.option({"--cartridge-label"}, cartridgeLabel, "generate an Octo compatible cartridge gif with the given text label");
    cli.option({"--cartridge-image"}, cartridgeImage, "generate an Octo compatible cartridge gif with the given image as label");
    cli.option({"--cartridge-options"}, cartridgeOptions, "specifies a JSON file that contains the options to use for the cartridge");
    cli.option({"--cartridge-variant"}, cartridgeVariant, "specifies a CHIP-8 variant that will be used to set the options (chip-8, schip, octo, xo-chip)");

    cli.category("Disassembler/Analyzer");
    cli.option({"-d", "--disassemble"}, disassemble, "disassemble a given file");
    cli.option({"-s", "--scan"}, scan, "scan files or directories for chip roms and analyze them, giving some information");
    cli.option({"--deep-scan"}, deepscan, "scan a directory tree for any files that look like CHIP-8 variant programs and list them, ignoring extensions");
    cli.option({"-f", "--find"}, opcodesToFind, "search for use of opcodes");
    cli.option({"-u", "--opcode-use"}, withUsage, "show usage of found opcodes when using -f");
    cli.option({"-p", "--full-path"}, fullPath, "print file names with path");
    cli.option({"--list-duplicates"}, dumpDoubles, "show found duplicates while scanning directories");
    cli.option({"--round-trip"}, roundTrip, "decompile and assemble and compare the result");
    cli.option({"-l", "--listing"}, genListing, "generate additional listing with addresses");

    cli.category("General");
    cli.option({"-q", "--quiet"}, quiet, "suppress all output during operation");
    cli.option({"-v", "--verbose"}, verbose, "more verbose progress output");
    cli.option({"--version"}, version, "just shows version info and exits");

    cli.positional(inputList, "Files or directories to work on");
    cli.parse();

    auto& logstream = preprocess && outputFile.empty() ? std::clog : std::cout;
    Architecture architecture;
    try {
        architecture = architectureFromName(architectureName);
    }
    catch (const std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << std::endl;
        return 1;
    }

    WorkMode mode = eCOMPILE;
    int modes = 0;
    if(!opcodesToFind.empty()) {
        mode = eSEARCH, modes++;
    }
    if(scan) {
        mode = eANALYSE, modes++;
    }
    if(disassemble) {
        mode = eDISASSEMBLE;
        modes++;
    }
    if(preprocess) {
        mode = ePREPROCESS;
        modes++;
    }
    if(!modes && deepscan) {
        mode = eDEEP_ANALYSE, modes++;
    }
    if(cartridgeLabel.empty() != cartridgeImage.empty()) {
        cartridgeBuild = true;
    }
    else if(!cartridgeLabel.empty() && !cartridgeImage.empty()) {
        std::cerr << "ERROR: Only either --cartridge-label or --cartridge-image, not both are supported!" << std::endl;
        exit(1);
    }
    if(modes > 1) {
        std::cerr << "ERROR: Multiple operation modes selected!" << std::endl;
        exit(1);
    }
    if(architecture != Architecture::OCTO && mode != eCOMPILE) {
        std::cerr << "ERROR: --arch=cdp1802 is currently only supported for compile mode." << std::endl;
        return 1;
    }
    if(architecture != Architecture::OCTO && cartridgeBuild) {
        std::cerr << "ERROR: Cartridge output is only supported for --arch=octo." << std::endl;
        return 1;
    }

    if(quiet)
        verbosity = 0;
    else if(verbose)
        verbosity = 100;

    if(!version && inputList.size() == 1 && fs::path(inputList.front()).extension() == ".gif") {
        emu::OctoCartridge cart(inputList.front());
        cart.loadCartridge();
        std::cout << cart.getJsonString() << std::endl;
        exit(0);
    }

    if(!quiet || version) {
        logstream << "Chiplet v" CHIPLET_VERSION " [" CHIPLET_HASH "], (c) 2023 by Steffen Schümann" << std::endl;
        logstream << "Octo assembler inspired by c-octo from John Earnest" << std::endl;
        logstream << "Preprocessor syntax based on Octopus by Tim Franssen\n" << std::endl;
        if(version)
            return 0;
        else
            logstream << "INFO: Current directory: " << fs::current_path().string() << std::endl;
    }

    if(inputList.empty()) {
        std::cerr << "ERROR: No input files given" << std::endl;
        exit(1);
    }

    if(mode == eANALYSE || mode == eDISASSEMBLE || mode == eSEARCH || mode == eDEEP_ANALYSE)
        rc = disassembleOrAnalyze(scan, dumpDoubles, inputList, mode);
    else if(architecture == Architecture::CDP1802) {
        if(inputList.size() != 1) {
            std::cerr << "ERROR: CDP1802 assembly currently expects exactly one input file." << std::endl;
            return 1;
        }
        if(outputFile.empty()) {
            std::cerr << "ERROR: No output filename given for " << (hexOutput ? "hex" : "binary") << " output (use -o/--output)." << std::endl;
            return 1;
        }
        std::error_code ec;
        if(!fs::exists(inputList.front(), ec) || fs::is_directory(inputList.front(), ec)) {
            std::cerr << "ERROR: Couldn't find input file '" << inputList.front() << "'." << std::endl;
            return 1;
        }
        try {
            const auto source = loadTextFile(inputList.front());
            cdp1802::AssemblySession session;
            if(!session.compile(source, {.listingMode = genListing ? cdp1802::ListingMode::TRADITIONAL : cdp1802::ListingMode::NONE})) {
                std::cerr << inputList.front() << ":" << session.errorLine() << ":";
                if(session.errorColumn())
                    std::cerr << session.errorColumn() << ": ";
                std::cerr << session.errorMessage() << std::endl;
                return 1;
            }
            if(hexOutput) {
                writeHexOutput(outputFile, session.segments());
            }
            else if(!writeBinaryOutput(outputFile, session.contiguousData())) {
                std::cerr << "ERROR: Failed to write output file '" << outputFile << "'." << std::endl;
                return 1;
            }
            if(genListing) {
                const auto listingFile = listingFilenameForOutput(outputFile);
                std::ofstream out(listingFile, std::ios::binary);
                out << session.listing();
                if(!out) {
                    std::cerr << "ERROR: Failed to write listing file '" << listingFile << "'." << std::endl;
                    return 1;
                }
            }
        }
        catch (std::exception& ex) {
            std::cerr << "Internal error: " << ex.what() << std::endl;
            rc = -1;
        }
    }
    else {
        emu::OctoCompiler compiler;
        compiler.setStartAddress(startAddress);
        compiler.generateLineInfos(!noLineInfo);
        compiler.setIncludePaths(includePaths);
        if(!quiet) {
            compiler.setProgressHandler([&](int verbLvl, std::string msg) {
                if (verbLvl <= verbosity) {
                    logstream << std::string(verbLvl * 2 - 2, ' ') << msg << std::endl;
                }
            });
        }

        auto start = steady_clock::now();
        for(const auto& def : defineList) {
            compiler.define(def, 1);
        }
        emu::CompileResult result;
        std::error_code ec;
        if(inputList.size() == 1 &&
            fs::exists(inputList.front(), ec) &&
            fs::is_directory(inputList.front(), ec)) {
            if(fs::exists(fs::path(inputList.front()) / "index.8o", ec)) {
                inputList = {(fs::path(inputList.front()) / "index.8o").string()};
                if (!quiet)
                    logstream << "Directory detected, using " << inputList.front() << std::endl;
            }
            else {
                std::cerr << "ERROR: Directory as input, but no 'index.8o' found inside." << std::endl;
                return 1;
            }
        }
        try {
            if (preprocess || cartridgeBuild) {
                result = compiler.preprocessFiles(inputList);
                if (result.resultType == emu::CompileResult::eOK) {
                    std::ostringstream os;
                    if(cartridgeBuild) {
                        compiler.dumpSegments(os);
                    }
                    if (outputFile.empty())
                        compiler.dumpSegments(std::cout);
                    else {
                        std::ofstream out(outputFile);
                        compiler.dumpSegments(out);
                    }
                    if (outputFile.empty()) {
                        std::cerr << "ERROR: No output filename given for cartridge output (use -o/--output)." << std::endl;
                        return 1;
                    }
                    if (cartridgeBuild) {
                        emu::OctoCartridge cart(outputFile);
                        if(!cartridgeOptions.empty()) {
                            if(!fs::exists(cartridgeOptions) || fs::is_directory(cartridgeOptions)) {
                                std::cerr << "ERROR: Couldn't find JSON file '" << cartridgeOptions << "' with cartridge options." << std::endl;
                                return 1;
                            }
                            else {
                                auto optionsStr = loadTextFile(cartridgeOptions);
                                try {
                                    auto json = nlohmann::json::parse(optionsStr);
                                    cart.setOptions(json);
                                }
                                catch(...) {
                                    std::cerr << "ERROR: Couldn't parse cartridge option file '" << cartridgeOptions << "'." << std::endl;
                                    return 1;
                                }
                            }
                        }
                        else if(!cartridgeVariant.empty()) {

                        }
                        else if(result.config) {
                            cart.setOptions(result.config->at("options"));
                        }
                        cart.saveCartridge(os.str(), cartridgeLabel, {});
                        // TODO: Finish cartridge generation
                        return -1;
                    }
                    return 0;
                }
            }
            else {

                result = compiler.compile(inputList);
                if (result.resultType == emu::CompileResult::eOK) {
                    if (outputFile.empty()) {
                        std::cerr << "ERROR: No output filename given for " << (hexOutput ? "hex" : "binary") << " output (use -o/--output)." << std::endl;
                        return 1;
                    }
                    if(hexOutput) {
                        if(startAddress < 0 || startAddress > 0xFFFF) {
                            std::cerr << "ERROR: --start-address is out of range for Intel HEX output." << std::endl;
                            return 1;
                        }
                        writeHexOutput(outputFile, static_cast<uint16_t>(startAddress), std::span<const uint8_t>{compiler.code(), compiler.codeSize()});
                    }
                    else if(!writeBinaryOutput(outputFile, std::span<const uint8_t>{compiler.code(), compiler.codeSize()})) {
                        std::cerr << "ERROR: Failed to write output file '" << outputFile << "'." << std::endl;
                        return 1;
                    }
                }
            }
            if (result.resultType != emu::CompileResult::eOK) {
                if (result.locations.empty()) {
                    std::cerr << "ERROR: " << result.errorMessage << std::endl;
                }
                else {
                    for (auto iter = result.locations.rbegin(); iter != result.locations.rend(); ++iter) {
                        switch (iter->type) {
                            case emu::CompileResult::Location::eINCLUDED:
                                std::cerr << "In file included from " << iter->file << ":" << iter->line << ":" << std::endl;
                                break;
                            case emu::CompileResult::Location::eINSTANTIATED:
                                std::cerr << "Instantiated at " << iter->file << ":" << iter->line << ":" << std::endl;
                                break;
                            default:
                                std::cerr << iter->file << ":" << iter->line << ":";
                                if (iter->column)
                                    std::cerr << iter->column << ": ";
                                std::cerr << result.errorMessage << "\n" << std::endl;
                                break;
                        }
                    }
                }
                rc = -1;
            }
        }
        catch (std::exception& ex) {
            std::cerr << "Internal error: " << ex.what() << std::endl;
            rc = -1;
        }
        auto duration = duration_cast<milliseconds>(steady_clock::now() - start).count();
        if (!quiet)
            logstream << "Duration: " << duration << "ms\n" << std::endl;
    }
    return rc;
}
