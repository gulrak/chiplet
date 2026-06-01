//---------------------------------------------------------------------------------------
// cdp1802/assembler_session.cpp
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
#include "assembly_session.hpp"

namespace cdp1802 {

bool AssemblySession::compile(std::string_view text, AssemblerOptions options)
{
    _assembler = std::make_unique<Assembler>(text, options);
    return _assembler->compile();
}

bool AssemblySession::isError() const
{
    return _assembler && _assembler->isError();
}

const std::string& AssemblySession::errorMessage() const
{
    return _assembler ? _assembler->errorMessage() : _empty;
}

int AssemblySession::errorLine() const
{
    return _assembler ? _assembler->errorLine() : 0;
}

int AssemblySession::errorColumn() const
{
    return _assembler ? _assembler->errorColumn() : 0;
}

const std::vector<AssemblySegment>& AssemblySession::segments() const
{
    static const std::vector<AssemblySegment> empty;
    return _assembler ? _assembler->segments() : empty;
}

const std::vector<uint8_t>& AssemblySession::contiguousData() const
{
    static const std::vector<uint8_t> empty;
    return _assembler ? _assembler->contiguousData() : empty;
}

const std::string& AssemblySession::listing() const
{
    return _assembler ? _assembler->listing() : _empty;
}

uint16_t AssemblySession::startAddress() const
{
    return _assembler ? _assembler->startAddress() : 0;
}

} // namespace cdp1802
