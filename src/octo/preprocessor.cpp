//---------------------------------------------------------------------------------------
//
//  octo/preprocessor.cpp
//
//  A preprocessor for Octo CHIP-8 assembly language, suitable for embedding in other
//  tools and environments. Compared to its original form it depends heavily on C++20
//  standard library.
//
//---------------------------------------------------------------------------------------
//
//  C++ Octo Assembler Version with Extensions:
//
//  The MIT License (MIT)
//
//  Copyright (c) 2023, Steffen Schümann
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
//---------------------------------------------------------------------------------------
#include "preprocessor.hpp"

#include <ghc/bit.hpp>
#include <ghc/fs_impl.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <nothings/stb_image.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <fstream>

#include "chiplet/imagehelper.hpp"
#include "chiplet/utility.hpp"
#include "chiplet/wavfile.hpp"

namespace {

constexpr int hexval(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

std::optional<img::Color> parseHexColor(std::string_view s) {
    if (!s.empty() && s.back() == ',') {
        s.remove_suffix(1);
    }
    if (s.empty()) return std::nullopt;
    img::Color out{};
    if (s.size() == 3) {
        int r = hexval(s[0]);
        int g = hexval(s[1]);
        int b = hexval(s[2]);
        if (r < 0 || g < 0 || b < 0) return std::nullopt;
        out.r = static_cast<uint8_t>(r * 17);
        out.g = static_cast<uint8_t>(g * 17);
        out.b = static_cast<uint8_t>(b * 17);
        return out;
    }
    if (s.size() == 6) {
        int r1 = hexval(s[0]), r2 = hexval(s[1]);
        int g1 = hexval(s[2]), g2 = hexval(s[3]);
        int b1 = hexval(s[4]), b2 = hexval(s[5]);
        if ((r1 | r2 | g1 | g2 | b1 | b2) < 0) return std::nullopt;
        out.r = static_cast<uint8_t>((r1 << 4) | r2);
        out.g = static_cast<uint8_t>((g1 << 4) | g2);
        out.b = static_cast<uint8_t>((b1 << 4) | b2);
        return out;
    }
    return std::nullopt;
}

std::optional<std::pair<int, int>> parseDimension(std::string_view s)
{
    auto pos = s.find('x');
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    std::pair<int, int> result;
    auto [ptr1, ec1] = std::from_chars(s.data(), s.data() + pos, result.first);
    if (ec1 != std::errc()) {
        return std::nullopt;
    }
    auto [ptr2, ec2] = std::from_chars(s.data() + pos + 1, s.data() + s.size(), result.second);
    if (ec2 != std::errc()) {
        return std::nullopt;
    }
    return result;
}

inline std::string loadTextFile(const std::string& file)
{
    std::ifstream is(file, std::ios::binary | std::ios::ate);
    std::streamsize size = is.tellg();
    is.seekg(0, std::ios::beg);

    std::string result(size, '\0');
    if (is.read(result.data(), size)) {
        return result;
    }

    return {};
}

template<class... Ts> struct visitor : Ts... { using Ts::operator()...;  };
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;

enum ImageFilterType { NEAREST, DITHER };

std::vector<uint8_t> processImage(uint8_t* data, size_t width, size_t height, ImageFilterType filter, const std::vector<img::Color>& palette, bool megaChip)
{
    switch(filter) {
        case NEAREST:
            return img::threshold(data, width, height, palette);
        case DITHER:
            return img::dither(data, width, height, palette);
    }
    return {};
}

int whitespaceLinesAtEnd(const std::string& text)
{
    int count = 0;
    for(auto iter = text.rbegin(); iter != text.rend(); ++iter) {
        if(!std::isspace((uint8_t)*iter))
            break;
        if(*iter == '\n')
            ++count;
    }
    return count;
}

int whitespaceLinesAtStart(const std::string& text)
{
    int count = 0;
    for(auto iter = text.begin(); iter != text.end(); ++iter) {
        if(!std::isspace((uint8_t)*iter))
            break;
        if(*iter == '\n')
            ++count;
    }
    return count;
}

} // namespace

namespace octo {

PreprocessScanResult Preprocessor::scan(std::string_view source) const
{
    octo::Lexer lexer{source, {.preprocessorMode = true}};
    PreprocessScanResult result;
    octo::Token token{0, 0};
    do {
        lexer.scanNextToken(token);
        result.tokens.push_back(token);
    } while (token.type != Token::Type::END_OF_FILE);
    return result;
}

void Preprocessor::reset()
{
    _codeSegments.clear();
    _dataSegments.clear();
    _symbols.clear();
    _collect.str("");
    _collect.clear();
    _collectLocationStack.clear();
    _currentSegment = CODE;
    _compileResult.reset();
}

void Preprocessor::setIncludePaths(const std::vector<std::string>& paths)
{
    _includePaths.clear();
    for(const auto& path : paths)
        _includePaths.push_back(path);
}

void Preprocessor::define(std::string name, Value val, SymbolType)
{
    _symbols[name] = {emu::OctoCompiler::eCONST, std::move(val)};
}

bool Preprocessor::isTrue(std::string_view name) const
{
    auto iter = _symbols.find(name);
    if(iter != _symbols.end()) {
        return std::visit(visitor{
            [](const std::monostate&) { return false; },
            [](int val) { return val != 0; },
            [](double val) { return std::fabs(val) > 0.0000001; },
            [](const std::string& val) { return !val.empty(); }
        }, iter->second.value);
    }
    return false;
}

std::optional<double> Preprocessor::definedValue(std::string_view name) const
{
    auto iter = _symbols.find(name);
    if(iter != _symbols.end() && (iter->second.type == emu::OctoCompiler::eCONST || iter->second.type == emu::OctoCompiler::eCALC || iter->second.type == emu::OctoCompiler::eLABEL)) {
        return std::visit(visitor{
            [](const std::monostate&) -> std::optional<double> { return {}; },
            [](int val) -> std::optional<double> { return (double)val; },
            [](double val) -> std::optional<double> { return val; },
            [](const std::string&) -> std::optional<double> { return {}; }
        }, iter->second.value);
    }
    return {};
}

std::optional<int32_t> Preprocessor::definedInteger(std::string_view name) const
{
    auto iter = _symbols.find(name);
    if(iter != _symbols.end() && (iter->second.type == emu::OctoCompiler::eCONST || iter->second.type == emu::OctoCompiler::eCALC || iter->second.type == emu::OctoCompiler::eLABEL)) {
        return std::visit(visitor{
            [](const std::monostate&) -> std::optional<int32_t> { return {}; },
            [](int val) -> std::optional<int32_t> { return val; },
            [](double val) -> std::optional<int32_t> { return (int32_t)val; },
            [](const std::string&) -> std::optional<int32_t> { return {}; }
        }, iter->second.value);
    }
    return {};
}

void Preprocessor::error(std::string msg)
{
    if(!_lexerStack.empty())
        lexer().errorLocation(_compileResult);
    else
        _compileResult.reset();
    _compileResult.errorMessage = std::move(msg);
    _compileResult.resultType = CompileResult::eERROR;
    throw std::runtime_error("");
}

void Preprocessor::warning(std::string msg)
{
    if(!_lexerStack.empty())
        lexer().errorLocation(_compileResult);
    else
        _compileResult.reset();
    _compileResult.errorMessage = std::move(msg);
    _compileResult.resultType = CompileResult::eWARNING;
}

void Preprocessor::info(std::string msg)
{
    if(!_lexerStack.empty())
        lexer().errorLocation(_compileResult);
    else
        _compileResult.reset();
    _compileResult.errorMessage = std::move(msg);
    _compileResult.resultType = CompileResult::eINFO;
}

const Preprocessor::CompileResult& Preprocessor::preprocessFile(const std::string& inputFile, const char* source, const char* end)
{
    if(end - source >= 3 && *source == (char)0xef && *(source+1) == (char)0xbb && *(source+2) == (char)0xbf)
        source += 3;

    try {
        _lexerStack.emplace(inputFile, std::string_view{source, static_cast<size_t>(end - source)}, _lexerStack.empty() ? nullptr : &_lexerStack.top(), LexerOptions{.preprocessorMode = true});
        std::shared_ptr<int> guard(nullptr, [&](int *) { _lexerStack.pop(); });
        auto& lex = lexer();
        //lex.setRange(inputFile, source, end);
        _currentSegment = CODE;
        writeLineMarker();
        try {
            auto token = lex.nextToken();
            while (true) {
                if (token == SourceToken::Type::END_OF_FILE) {
                    writePrefix();
                    break;
                }
                if (token == SourceToken::Type::PREPROCESSOR) {
                    writePrefix();
                    if (lex.expect(":include")) {
                        auto next = lex.nextToken();
                        if (next != SourceToken::Type::STRING)
                            error("Expected string after ':include'.");
                        auto newFile = ghc::filesystem::absolute(inputFile).parent_path() / lex.token().strValue;
                        auto extension = ::toLower(newFile.extension().string());
                        if (isImage(extension)) {
                            token = includeImage(newFile.string());
                        }
                        else if (extension == ".bin" || extension == ".ch8") {
                            token = includeBinary(newFile.string());
                        }
                        else {
                            flushSegment();
                            auto oldSeg = _currentSegment;
                            preprocessFile(newFile.string());
                            _currentSegment = oldSeg;
                            token = lex.nextToken(true);
                        }
                    }
                    else if (lex.expect(":segment")) {
                        auto next = lex.nextToken();
                        if (next != SourceToken::Type::IDENTIFIER || (lex.token().rawValue != "data" && lex.token().rawValue != "code"))
                            error("Expected 'data' or 'code' after ':segment'.");
                        flushSegment();
                        _currentSegment = (lex.token().rawValue == "code" ? CODE : DATA);
                        token = lex.nextToken(true);
                        writeLineMarker();
                    }
                    else if (lex.expect(":if")) {
                        auto option = lex.nextToken();
                        if (option != SourceToken::Type::IDENTIFIER)
                            error("{}: Identifier expected after ':if'.");
                        if (!_emitCode.empty() && _emitCode.top() != ACTIVE) {
                            _emitCode.push(SKIP_ALL);
                        }
                        else {
                            _emitCode.push(isTrue(lex.token().rawValue) ? ACTIVE : INACTIVE);
                        }
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":unless")) {
                        auto option = lex.nextToken();
                        if (option != SourceToken::Type::IDENTIFIER)
                            error("Identifier expected after ':unless'.");
                        if (!_emitCode.empty() && _emitCode.top() != ACTIVE) {
                            _emitCode.push(SKIP_ALL);
                        }
                        else {
                            _emitCode.push(!isTrue(lex.token().rawValue) ? ACTIVE : INACTIVE);
                        }
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":else")) {
                        if (_emitCode.empty())
                            error("Use of ':else' without ':if' or ':unless'.");
                        _emitCode.top() = _emitCode.top() == INACTIVE ? ACTIVE : SKIP_ALL;
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":end")) {
                        if (_emitCode.empty())
                            error("Use of ':end' without ':if' or ':unless'.");
                        _emitCode.pop();
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":dump-options")) {
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":config")) {
                        token = lex.nextToken(true);
                        if (lex.expect("{")) {
                            std::string jsonStr = "{";
                            int braceCnt = 1;
                            token = lex.nextToken(true);
                            while(braceCnt && token != SourceToken::Type::END_OF_FILE) {
                                if(token == SourceToken::Type::LCURLY)
                                    ++braceCnt;
                                else if(token == SourceToken::Type::RCURLY)
                                    --braceCnt;
                                jsonStr += " " + std::string(lex.token().rawValue);
                                token = lex.nextToken(true);
                            }
                            if(braceCnt)
                                error("The ':config' JSON object parameter is not closed.");
                            else {
                                try {
                                    _compileResult.config = std::make_shared<nlohmann::json>(nlohmann::json::parse(jsonStr));
                                }
                                catch(...) {
                                    error("Error parsing the JSON object parameter of ':config'.");
                                }
                            }
                        }
                    }
                }
                else if (token == SourceToken::Type::DIRECTIVE && lex.expect(":const") && (_emitCode.empty() || _emitCode.top() == ACTIVE)) {
                    writePrefix();
                    write(lex.token().rawValue);
                    auto nameToken = lex.nextToken();
                        if (nameToken != SourceToken::Type::IDENTIFIER)
                        error("Identifier expected after ':const'.");
                    auto constName = lex.token().rawValue;
                    writePrefix();
                    write(lex.token().rawValue);
                    auto value = lex.nextToken();
                    if (value != SourceToken::Type::IDENTIFIER && value != SourceToken::Type::NUMBER)
                        error("Number or identifier expected after ':const <name>'.");
                    writePrefix();
                    write(lex.token().rawValue);
                    if (value == SourceToken::Type::NUMBER) {
                        define(std::string(constName), lex.token().numValue);
                    }
                    token = lex.nextToken();
                }
                else {
                    writePrefix();
                    write(lex.token().rawValue);
                    token = lex.nextToken();
                }
            }
            flushSegment();
        }
        catch(Lexer::Exception& le) {
            _compileResult.errorMessage = le.errorMessage;
            _compileResult.resultType = CompileResult::eERROR;
            lex.errorLocation(_compileResult);
            return _compileResult;
        }
    }
    catch(std::exception&)
    {
        return _compileResult;
    }
    return _compileResult;
}

std::string Preprocessor::resolveFile(const ghc::filesystem::path& file)
{
    if(file.is_absolute()) {
        std::error_code ec;
        if(ghc::filesystem::exists(file, ec))
            return file.string();
    }
    if(!_lexerStack.empty() && !lexer().filename().empty()) {
        std::error_code ec;
        auto newPath = ghc::filesystem::absolute(lexer().filename(), ec).parent_path() / file;
        if(!ec && ghc::filesystem::exists(newPath, ec))
            return newPath.string();
    }
    else {
        std::error_code ec;
        if(ghc::filesystem::exists(file, ec))
            return file.string();
    }
    for(const auto& path : _includePaths) {
        std::error_code ec;
        if(ghc::filesystem::exists(path / file, ec))
            return (path / file).string();
    }
    error(fmt::format("File not found: '{}'", file.string()));
    return "";
}

const Preprocessor::CompileResult& Preprocessor::preprocessFile(const std::string& inputFile)
{
    try {
        auto file = resolveFile(inputFile);
        if (_progress)
            _progress(_lexerStack.size() + 1, "preprocessing '" + inputFile + "' ...");
        auto content = loadTextFile(inputFile);
        preprocessFile(inputFile, content.data(), content.data() + content.size());
    }
    catch(std::runtime_error&)
    {
    }
    return _compileResult;
}

const Preprocessor::CompileResult& Preprocessor::preprocessFiles(const std::vector<std::string>& files)
{
    for(const auto& file : files) {
        preprocessFile(file);
        if(_compileResult.resultType != CompileResult::eOK)
            break;
    }
    return _compileResult;
}

void Preprocessor::doWrite(const std::string_view& text, int line)
{
    auto& lex = lexer();
    if(_generateLineInfos && line >= 0 && (_collectLocationStack.empty() || _collectLocationStack.back().first != line || lex.filename() != _collectLocationStack.back().second)) {
        auto locationStack = lex.locationStack();
        locationStack.back().first = line;
        auto iterOld = _collectLocationStack.begin();
        auto iterNew = locationStack.begin();
        while(iterOld != _collectLocationStack.end() && iterNew != locationStack.end() && *iterOld == *iterNew) {
            iterOld++;
            iterNew++;
        }
        if (_emitCode.empty() || _emitCode.top() == ACTIVE) {
            auto depth = iterNew - locationStack.begin();
            _collect << "\n";
            while (iterNew != locationStack.end()) {
                _collect << fmt::format("#@line[{},{},{}]\n", ++depth, iterNew->first, iterNew->second);
                iterNew++;
            }
        }
        std::swap(_collectLocationStack, locationStack);
    }
    if(!_collectLocationStack.empty())
        _collectLocationStack.back().first += std::count(text.begin(), text.end(), '\n');
    if (_emitCode.empty() || _emitCode.top() == ACTIVE)
        _collect << text;
}

void Preprocessor::writePrefix()
{
    if(!_lexerStack.empty() && !lexer().token().prefix.empty()) {
        doWrite(lexer().token().prefix, lexer().token().prefixLine + 1);
    }
}

void Preprocessor::write(const std::string_view& text)
{
    if(!text.empty()) {
        doWrite(text, lexer().token().line + 1);
    }
}

void Preprocessor::writeGenerated(const std::string_view& text)
{
    if(!text.empty()) {
        doWrite(text, -1);
    }
}

void Preprocessor::writeLineMarker()
{
    return;
}

void Preprocessor::flushSegment()
{
    if(_currentSegment == CODE)
        _codeSegments.push_back(_collect.str());
    else
        _dataSegments.push_back(_collect.str());
    _collect.str("");
    _collect.clear();
    _collectLocationStack.clear();
}

bool Preprocessor::isImage(const std::string& extension)
{
    return extension == ".png" || extension == ".gif" || extension == ".bmp" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga";
}

Preprocessor::SourceToken::Type Preprocessor::includeImage(std::string filename)
{
    std::vector<img::Color> palette{{0,0,0}, {255,255,255}};
    int width,height,numChannels;
    int widthHint = -1, heightHint = -1;
    bool megaChip = false;
    ImageFilterType filter = NEAREST;
    bool genLabels = true;
    bool genPalette = false;
    bool debug = false;
    auto& lex = lexer();
    auto token = lex.nextToken(true);
    while(token != SourceToken::Type::END_OF_FILE) {
        if (auto size = parseDimension(lex.token().rawValue); size) {
            widthHint = size.value().first;
            heightHint = size.value().second;
            if (megaChip) {
                if (widthHint == 0 || widthHint > 256 || heightHint == 0 || heightHint > 256)
                    error("Invalid size for mega-chip image.");
            }
            else if (!(widthHint == 8 || widthHint == 16) || heightHint == 0 || heightHint > 64) {
                error("Invalid size for 8xN or 16x16 image.");
            }
        }
        else if (token == SourceToken::Type::IDENTIFIER && lex.token().strValue == "no-labels") {
            genLabels = false;
        }
        else if (token == SourceToken::Type::IDENTIFIER && lex.token().strValue == "debug") {
            debug = true;
        }
        else if(token == SourceToken::Type::IDENTIFIER && lex.token().strValue == "dither") {
            filter = DITHER;
        }
        else if(token == SourceToken::Type::IDENTIFIER && lex.token().strValue == "megachip") {
            megaChip = true;
        }
        else if (token == SourceToken::Type::IDENTIFIER && lex.token().strValue == "palette") {
            genPalette = true;
        }
        else if (token == SourceToken::Type::LSQUARE) {
            std::vector<img::Color> colors;
            if (megaChip) {
                colors.reserve(256);
                colors.push_back({0,0,0,0});
            }
            auto sv = lex.token().rawValue;
            if (sv.length() > 1) {
                sv.remove_prefix(1);
                auto col = parseHexColor(sv);
                if (!col)
                    error(fmt::format("Bad color value for image include: '{}'", sv));
                colors.push_back(*col);
                while (true) {
                    token = lex.nextToken(true);
                    if (token == SourceToken::Type::END_OF_FILE)
                        break;
                    if (token == SourceToken::Type::RSQUARE)
                        break;
                    if (endsWith(lex.token().rawValue, "]")) {
                        auto sv = lex.token().rawValue;
                        sv.remove_suffix(1);
                        col = parseHexColor(sv);
                        if (!col)
                            error(fmt::format("Bad color value for image include: '{}'", sv));
                        colors.push_back(*col);
                        break;
                    }
                    auto sv = lex.token().rawValue;
                    if (sv.length() > 1) {
                        col = parseHexColor(sv);
                        if (!col)
                            error(fmt::format("Bad color value for image include: '{}'", sv));
                        colors.push_back(*col);
                    }
                }
            }
            if (auto n = colors.size(); (megaChip && n > 2 && n < 256) || (n != 0 && (n & (n - 1)) == 0 && n >= 2 && n <= 16))
                palette = colors;
            else
                error(fmt::format("Invalid color palette for image include, must be power of two and between 2 and 16 entries"));
        }
        else {
            break;
        }
        token = lex.nextToken(true);
    }
    auto* data = stbi_load(filename.c_str(), &width, &height, &numChannels, 4);
    if(!data) {
        error(fmt::format("Could not load image: '{}'", filename));
    }
    int spriteWidth, spriteHeight;
    if(widthHint > 0) {
        spriteWidth = widthHint;
        spriteHeight = heightHint;
    }
    else if ( width == 16 && height == 16 ) {
        spriteWidth = spriteHeight = 16;
    } else {
        int numRows = 1;
        while ( height % numRows != 0 || height / numRows >= 16 )
            numRows++;
        spriteWidth = 8;
        spriteHeight = height / numRows;
    }
    auto name = ghc::filesystem::path(filename).filename().stem().string();
    if(width % spriteWidth != 0)
        error(fmt::format("Image needs to be divisible by {}.", spriteWidth));
    auto processed = processImage(data, width, height, filter, palette, megaChip);
    stbi_image_free(data);
    std::string debugStr;
    bool asHex = true;
    if(debug && _progress) _progress(1, fmt::format("\nSprite dimension: {}x{}", spriteWidth, spriteHeight));
    if (!megaChip) {
        auto bitPlanes = ghc::countr_zero(palette.size());
        for (int y = 0; y < height; y += spriteHeight) {
            for (int x = 0; x < width; x += spriteWidth) {
                int index = y * width + x;
                for (int plane = 0; plane < bitPlanes; ++plane) {
                    size_t count = 0;
                    if (bitPlanes == 1) {
                        if(genLabels)
                            writeGenerated(fmt::format(": {}-{}-{}", name, x/8, y/spriteHeight));
                        if(debug && _progress) _progress(1, fmt::format("{} {},{}:", name, x/8, y/spriteHeight));
                    }
                    else {
                        if(genLabels)
                            writeGenerated(fmt::format(": {}-{}-{}-{}", name, plane, x/8, y/spriteHeight));
                        if(debug && _progress) _progress(1, fmt::format("{} {},{},{}:", name, plane, x/8, y/spriteHeight));
                    }
                    for (int rows = 0; rows < spriteHeight; rows++) {
                        for (int cols = 0; cols < spriteWidth / 8; cols++) {
                            uint8_t val = 0;
                            for (uint8_t bit = 0x80, i = 0; bit > 0; bit >>= 1, ++i) {
                                bool inside = x + cols < width && y + rows < height;
                                auto pixel = inside ? processed[index + rows * width + cols * 8 + i] : 0;
                                if (pixel & (1 << plane))
                                    val |= bit;
                                if(debug && _progress) debugStr += (pixel & (1 << plane)) ? "██" : "░░";
                            }
                            if (asHex) {
                                if (count++ % 16 == 0) {
                                    writeGenerated("\n ");
                                }
                                writeGenerated(fmt::format(" 0x{:02x}", val));
                            }
                            else
                                writeGenerated(fmt::format(" 0b{:08b}", val));
                        }
                        if(debug && _progress) {
                            _progress(1, debugStr);
                            debugStr.clear();
                        }
                        if (!asHex)
                            writeGenerated("\n");
                    }
                    if (asHex)
                        writeGenerated("\n");
                }
            }
        }
    }
    else {
        for (int y = 0; y < height; y += spriteHeight) {
            for (int x = 0; x < width; x += spriteWidth) {
                int index = y * width + x;
                if(genLabels)
                    writeGenerated(fmt::format(": {}-{}-{}", name, x/spriteWidth, y/spriteHeight));
                if(debug && _progress) _progress(1, fmt::format("{} {},{}:", name, x/spriteWidth, y/spriteHeight));
                size_t count = 0;
                for (int rows = 0; rows < spriteHeight; rows++) {
                    for (int cols = 0; cols < spriteWidth; cols++) {
                        auto pixel = processed[index + rows * width + cols];
                        if (count++ % 16 == 0) {
                            writeGenerated("\n ");
                        }
                        writeGenerated(fmt::format(" 0x{:02x}", pixel));
                    }
                }
                writeGenerated("\n");
            }
        }
    }
    return token;
}

Preprocessor::SourceToken::Type Preprocessor::includeBinary(std::string filename)
{
    auto data = loadFile(filename);
    if(!data) {
        error(fmt::format("Could not load binary file: '{}'", to_string(data.error())));
    }
    bool genLabels = true;
    auto& lex = lexer();
    auto token = lex.nextToken(true);
    while(token != SourceToken::Type::END_OF_FILE) {
        if(token == SourceToken::Type::IDENTIFIER && lex.token().strValue == "no-labels")
        {
            genLabels = false;
        }
        else {
            break;
        }
        token = lex.nextToken(true);
    }
    auto name = ghc::filesystem::path(filename).filename().stem().string();
    if(genLabels)
        writeGenerated(fmt::format(": {}", name));
    DataBlockFormatter formatter{[this](std::string_view sv){writeGenerated(sv);}};
    for (auto byte : *data) {
        formatter.write(fmt::format(" 0x{:02x}", byte));
    }
    return lexer().nextToken(true);
}

Preprocessor::SourceToken::Type Preprocessor::includeWav(std::string filename)
{
    auto& lex = lexer();
    auto token = lex.nextToken(true);
    std::optional<uint32_t> frequencyOverride{};
    while(token != SourceToken::Type::END_OF_FILE) {
        if(token == SourceToken::Type::NUMBER)
        {
            if (lex.token().numValue <= 0 || lex.token().numValue > 96000)
                error(fmt::format("Invalid frequency value for audio include: '{}'.", lex.token().rawValue));
            frequencyOverride = lex.token().numValue;
        }
        else {
            break;
        }
        token = lex.nextToken(true);
    }
    try {
        WavFile<uint8_t> wav(filename, frequencyOverride);
        DataBlockFormatter formatter{[this](std::string_view sv){writeGenerated(sv);}};
        const auto& samples = wav.samples();
        for (auto sample : samples) {
            formatter.write(fmt::format(" 0x{:02x}", sample));
        }
    }
    catch(std::exception& ex) {
        error(fmt::format("Couldn't read audio file '{}': {}", filename, ex.what()));
    }
    return lexer().nextToken(true);
}

void Preprocessor::dumpSegments(std::ostream& output)
{
    int endingWSLines = 2;
    for(auto& segment : _codeSegments) {
        if(!segment.empty()) {
            if(!_generateLineInfos) {
                auto sepLines = endingWSLines + whitespaceLinesAtStart(segment);
                for (int i = 0; i < 2 - sepLines; ++i)
                    output << '\n';
            }
            output << segment;
            if (segment.back() != '\n')
                output << '\n';
            if(!_generateLineInfos)
                endingWSLines = whitespaceLinesAtEnd(segment);
        }
    }
    for(auto& segment : _dataSegments) {
        if(!segment.empty()) {
            if(!_generateLineInfos) {
                auto sepLines = endingWSLines + whitespaceLinesAtStart(segment);
                for (int i = 0; i < 2 - sepLines; ++i)
                    output << '\n';
            }
            output << segment;
            if(segment.back() != '\n')
                output << '\n';
            if(!_generateLineInfos)
                endingWSLines = whitespaceLinesAtEnd(segment);
        }
    }
}

}
