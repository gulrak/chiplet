//---------------------------------------------------------------------------------------
// chiplet/tapeaudio.hpp
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

#include <chiplet/filter.hpp>
#include <chiplet/wavfile.hpp>

class TapeAudio
{
public:
    TapeAudio(uint32_t sampleFrequencyHz, bool clean = false)
        : _clipper{1.5}
        , _hpFilter{sampleFrequencyHz, 100}
        , _lpFilter{sampleFrequencyHz, 5000}
        , _isClean{clean}
    {
    }
    ~TapeAudio() = default;

    void open(const std::string& filename, uint32_t sampleRate)
    {
        _wavFile.open(filename, sampleRate, WavStreamWriter::ChannelLayout::Mono);
        appendSamples(0, 50);
    }

    bool isOpen() const { return _wavFile.isOpen(); }

    void appendSample(int16_t sample)
    {
        if (_isClean)
            _wavFile.appendSample(sample);
        else
            _wavFile.appendSample(_lpFilter(_hpFilter(_clipper(sample))));
        _lastSample = sample;
    }

    void appendSamples(int16_t sample, size_t count)
    {
        if (_isClean)
            _wavFile.appendSample(sample, count);
        else {
            for (size_t i = 0; i < count; ++i)
                _wavFile.appendSample(_lpFilter(_hpFilter(_clipper(sample))));
        }
        _lastSample = sample;
    }

    void close()
    {
        if (_wavFile.isOpen()) {
            appendSamples(_lastSample, 150);
            _wavFile.close();
        }
    }

private:
    SoftClipper _clipper;
    HighPassOnePole _hpFilter{44100, 100};
    LowPassButterworthFilter _lpFilter{44100, 5000};
    WavStreamWriter _wavFile;
    int16_t _lastSample{};
    bool _isClean{};
};