//---------------------------------------------------------------------------------------
// cdp1802/assembler_session.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2025, Steffen Schümann <s.schuemann@pobox.com>
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

#include "assembler.hpp"

#include <memory>
#include <string>

namespace cdp1802 {

class AssemblySession
{
public:
    bool compile(std::string_view text, AssemblerOptions options = {});
    bool isError() const;
    const std::string& errorMessage() const;
    int errorLine() const;
    int errorColumn() const;
    const std::vector<AssemblySegment>& segments() const;
    const std::vector<uint8_t>& contiguousData() const;
    const std::string& listing() const;
    uint16_t startAddress() const;

private:
    std::unique_ptr<Assembler> _assembler;
    std::string _empty;
};

} // namespace cdp1802
