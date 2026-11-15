//---------------------------------------------------------------------------------------
// chiplet/asacpp.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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

#pragma once

#include <ghc/fs_fwd.hpp>
#include <chiplet/compileresult.hpp>
#include <chiplet/sha1.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = ghc::filesystem;

// The ASAP assembler for the HP Saturn (Alonzo Gariepy's mnemonics), as the HP-48 CHIP-8 interpreters are written: two
// passes with includes, macros, conditionals and defines. The result of preprocess() and compile() is a CompileResult like
// the other assemblers give, its includedFiles are the files read through `include` (absolute). The message of an error
// starts with "file:line: " for every file of the include chain, outermost first, its locations follow the same order.
namespace emu {

class AsaCpp {
public:
    AsaCpp();
    ~AsaCpp();

    AsaCpp(const AsaCpp&) = delete;
    AsaCpp& operator=(const AsaCpp&) = delete;
    AsaCpp(AsaCpp&&) noexcept;
    AsaCpp& operator=(AsaCpp&&) noexcept;

    const CompileResult& preprocess(const fs::path& filename, std::span<const char> source);
    const CompileResult& preprocess(const fs::path& filename);
    const CompileResult& compile(const fs::path& filename, std::span<const char> source);
    const CompileResult& compile(const fs::path& filename);

    void define(std::string name, std::int64_t val = 1);
    void defineExpression(std::string name, std::string expr);
    std::optional<double> definedValue(std::string_view name) const;
    std::optional<std::int32_t> definedInteger(std::string_view name) const;

    const CompileResult& compileResult() const { return _translationResult; }
    bool isError() const { return _translationResult.resultType == CompileResult::eERROR; }

    void setIncludePaths(const std::vector<std::string>& paths);
    // The files the last preprocess() or compile() read through `include` (absolute paths), not the source itself
    const std::vector<std::string>& includedFiles() const;

    std::uint32_t codeSize() const;
    std::span<const std::uint8_t> code() const;
    std::vector<std::uint8_t> placedCode(std::uint32_t nibbleCount) const;
    const Sha1::Digest& sha1() const;

    std::pair<std::uint32_t, std::uint32_t> addrForLine(std::uint32_t line) const;
    std::uint32_t lineForAddr(std::uint32_t addr) const;

    std::string listing() const;
    std::string preprocessedSource() const;
    std::string hpAscii() const;

private:
    struct Impl;
    Impl* _impl{};
    CompileResult _translationResult;
};

}  // namespace emu
