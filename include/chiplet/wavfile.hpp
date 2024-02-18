//---------------------------------------------------------------------------------------
// src/emulation/utility.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2015, Steffen Schümann <s.schuemann@pobox.com>
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

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "utility.hpp"


template <typename SampleType>
class WavFile
{
public:
    WavFile();
    bool read(std::string filename);
    bool write(std::string filename);

private:
    static uint16_t readWordLE(const uint8_t* data)
    {
        return (*data<<8)|*(data+1);
    }

    static uint32_t readLongLE(const uint8_t* data)
    {
        return (*data<<24)|(*(data+1)<<16)|(*(data+2)<<8)|*(data+3);
    }


    std::string _filename;
    std::vector<SampleType> _data;
    uint32_t _sampleRate{};
};

template <typename SampleType>
inline WavFile<SampleType>::WavFile()
{
}

template <typename SampleType>
inline bool WavFile<SampleType>::read(std::string filename)
{
    _filename = std::move(filename);
    auto data = loadFile(filename);
    if (data.size() < 44) {
        return false;
    }

    // check if it is a valid WAV file
    if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F' || data[8] != 'W' || data[9] != 'A' || data[10] != 'V' || data[11] != 'E') {
        return false;
    }

    // check it is 8-bit PCM mono
    if (readWordLE(data.data() + 0x22) != 8 && readWordLE(data.data() + 0x14) != 1 && readWordLE(data.data() + 0x16) != 1) {
        return false;
    }

    _sampleRate = readLongLE(data.data() + 0x18);

    // get the size of the audio data
    uint32_t dataSize = readLongLE(data.data() + 0x28);

    // check for enough data
    if(data.size() < 44 + dataSize) {
        return false;
    }

    // copy the audio samples into a vector<uint8_t>
    _data.resize(dataSize);
    std::memcpy(_data.data(), data.data() + 0x2c);
    return true;
}

