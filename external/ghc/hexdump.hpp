//---------------------------------------------------------------------------------------
//
// ghc::hexdump - A minimal hexdump helper for C++17
//
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2024, Steffen Schümann <s.schuemann@pobox.com>
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

#include <iomanip>
#include <ostream>
#include <string>
#include <cstdint>

namespace ghc {

inline std::ostream& hexDump(std::ostream& os, const uint8_t* buffer, size_t size, bool withChars = true, uint32_t offset = 0)
{
    auto prevFlags = os.flags();
    auto prevFill = os.fill();
    auto renderAscii = [&os](const char* buffer, size_t num){
        os << "  ";
        for (size_t i = 0; i < num; ++i) {
            os << (std::isprint(buffer[i]) ? buffer[i] : '.');
        }
    };
    os << std::hex;
    os.fill('0');
    size_t i = 0;
    for (; i < size; ++i) {
        if ((i & 0xf) == 8) { os << ' '; }
        if (i % 16 == 0) {
            if (i && withChars) {
                renderAscii(reinterpret_cast<const char*>(&buffer[i] - 16), 16);
                os << std::endl;
            }
            os << std::setw(4) << std::right << unsigned(i + offset) << ' ';
        }
        os << ' ' << std::setw(2) << std::right << unsigned(buffer[i]);
    }
    if (i % 16 != 0 && withChars) {
        for (size_t j = 0; j < 16 - (i % 16); ++j) {
            if(j == 8) os << " ";
            os << "   ";
        }
        renderAscii(reinterpret_cast<const char*>(&buffer[i] - (i % 16)), (i % 16));
        os << std::endl;
    }
    os.fill(prevFill);
    os.flags(prevFlags);
    return os;
}

inline std::ostream& hexCode(std::ostream& os, const uint8_t* buffer, size_t size, int fieldsPerLine = 16)
{
    auto prevFlags = os.flags();
    auto prevFill = os.fill();
    os << std::hex << std::setfill('0') << "   ";
    for(size_t i = 0; i < size; ++i) {
        if(i && (i % fieldsPerLine) == 0)
            os << std::endl << "   ";
        os << " 0x" << std::setw(2) << static_cast<unsigned>(buffer[i]) << (i + 1 == size ? "" : ",");
    }
    os << std::endl;
    os.fill(prevFill);
    os.flags(prevFlags);
    return os;
}

}
