//---------------------------------------------------------------------------------------
// viptape.cpp
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

#include <iostream>

#include <ghc/fs_impl.hpp>
#include <chiplet/filter.hpp>
#include <chiplet/wavfile.hpp>
#include <ghc/cli.hpp>

namespace fs = ghc::filesystem;

class Rectifier
{
public:
    // thresholdAbs: minimum |peak| (0..32767) for a half-wave to be considered "real"
    // outputLevel: fixed output amplitude (e.g. 16384 = 50% of int16 range)
    explicit Rectifier(int16_t thresholdAbs, int16_t outputLevel = 16384)
        : _threshold(std::clamp<int>(std::abs(thresholdAbs), 0, 32767)),
          _outputLevel(outputLevel)
    {}

    // Feed one sample. Returns {0,0} unless a half wave just finished.
    std::pair<std::size_t, int16_t> operator()(int16_t sample)
    {
        // -1, 0, or +1
        const int sRaw = (sample > 0) - (sample < 0);

        if (!_inWave) {
            // Start first wave
            _inWave = true;
            _currentSign = (sRaw == 0 ? +1 : sRaw);  // default sign if first is zero
            _waveLength = 1;
            _peakAbs = std::abs(static_cast<int>(sample));
            return {0u, int16_t{0}};
        }

        // Treat zeros as part of the current wave, not as sign changes
        int effectiveSign = (sRaw == 0) ? _currentSign : sRaw;

        if (effectiveSign == _currentSign) {
            // Still in same half wave
            ++_waveLength;
            int absVal = std::abs(static_cast<int>(sample));
            if (absVal > _peakAbs) {
                _peakAbs = absVal;
            }
            return {0u, int16_t{0}};
        }

        // Sign changed: finish previous half-wave, then start a new one
        auto result = finalizeCurrentWave();

        // Start new wave with this sample
        _currentSign = (sRaw == 0 ? _currentSign : sRaw); // here sRaw should be non-zero
        _waveLength = 1;
        _peakAbs = std::abs(static_cast<int>(sample));
        _inWave = true;

        return result;
    }

    // Call at the end of the stream to flush the last half wave, if any.
    std::pair<std::size_t, int16_t> flush()
    {
        if (!_inWave || _waveLength == 0) {
            return {0u, int16_t{0}};
        }

        auto result = finalizeCurrentWave();
        _inWave = false;
        _currentSign = 0;
        _waveLength = 0;
        _peakAbs = 0;
        return result;
    }

    // Optional: reset without emitting anything (e.g. between tracks)
    void reset()
    {
        _inWave = false;
        _currentSign = 0;
        _waveLength = 0;
        _peakAbs = 0;
    }

private:
    std::pair<std::size_t, int16_t> finalizeCurrentWave()
    {
        int16_t level = 0;

        if (_peakAbs >= _threshold && _currentSign != 0) {
            level = (_currentSign > 0)
                ? _outputLevel
                : static_cast<int16_t>(-_outputLevel);
        }

        std::size_t length = _waveLength;

        // Don't clear _inWave here; caller decides (either starts new wave or flush resets)
        _waveLength = 0;
        _peakAbs = 0;

        return {length, level};
    }

    // Parameters
    int _threshold;           // |peak| threshold for "real" half waves
    int16_t _outputLevel;     // e.g. 16384

    // Current half-wave state
    bool _inWave = false;
    int _currentSign = 0;     // -1 or +1
    std::size_t _waveLength = 0;
    int _peakAbs = 0;         // store as int to avoid abs(int16_t) UB on -32768
};


class TapeDecoder
{
public:
    enum class State {
        Init,
        Start,
        Bit0,
        Bit1,
        Gap,
        End
    };
    explicit TapeDecoder(const WavFile<int16_t>& wav, uint32_t zeroFreq, uint32_t oneFreq)
    : _hpFilter(wav.sampleRate(), (std::min)(zeroFreq, oneFreq))
    , _lpFilter(wav.sampleRate(), (std::max)(zeroFreq, oneFreq))
    , _rectifier(6000)
    , _sampleRate(wav.sampleRate())
    , _samples(wav.samples())
    {
        if (zeroFreq > oneFreq) {
            _zeroLowFreq = 1200; //(zeroFreq + oneFreq) / 2;
            _oneLowFreq = oneFreq * 3 / 4;
            _zeroHighFreq = zeroFreq * 4 / 3;
            _oneHighFreq = _zeroLowFreq;
        }
        else {
            _zeroLowFreq = zeroFreq * 3 / 4;
            _oneLowFreq = 1200; //(zeroFreq + oneFreq) / 2;
            _zeroHighFreq = oneFreq * 4 / 3;
            _oneHighFreq = zeroFreq;
        }
    }
    TapeDecoder() = delete;
    ~TapeDecoder() = default;
    size_t scanForStartMarker()
    {
        size_t length{};
        do {
            do {
                _state = decodeNextBit();
            }
            while (_state != State::Bit0 && _state != State::End);
            while (_state == State::Bit0) {
                ++length;
                _state = decodeNextBit();
            }
        }
        while (length < 1000 && _state != State::End);
        return _state == State::End ? 0 : length;
    }
    static bool isBit(State state) { return state == State::Bit0 || state == State::Bit1; }
    std::optional<uint8_t> decodeByte()
    {
        if (_state == State::Start) {
            _state = decodeNextBit();
        }
        if (_state != State::Bit1) {
            return {};
        }
        _state = decodeNextBit();
        if (_state != State::Bit1) {
            return {};
        }
        uint8_t resultByte = 0;
        uint8_t parity = 0;
        for (int i = 0; i < 8; ++i) {
            const auto bit = decodeNextBit();
            _state = decodeNextBit(); // get second halfwave
            if (!isBit(bit) || !isBit(_state) || bit != _state) {
                return {};
            }
            if (bit == State::Bit1) {
                resultByte |= (1 << (7 - i));
                parity ^= 1;
            }
        }
        const auto decodedParity = decodeNextBit();
        _state = decodeNextBit();
        if (decodedParity != _state || (decodedParity != State::Bit1 && decodedParity != State::Bit0)
            || (parity != (decodedParity == State::Bit1))) {
            return {};
        }
        _state = State::Start;
        return resultByte;
    }
private:
    State decodeNextBit()
    {
        while (_idx < _samples.size()) {
            auto sample = _samples[_idx++];
            auto [len, lvl] = _rectifier(sample/*_hpFilter(_lpFilter(sample))*/);
            if (len > 0) {
                if (lvl != 0) {
                    auto f = _sampleRate / len / 2;
                    if (len <= 19 /*f >= _zeroLowFreq && f < _zeroHighFreq*/) {
                        std::clog << "0: " << len << ", " << lvl << std::endl;
                        return State::Bit0;
                    }
                    if (len > 19 /*_oneLowFreq && f < _oneHighFreq*/) {
                        std::clog << "1: " << len << ", " << lvl << std::endl;
                        return State::Bit1;
                    }
                }
                std::clog << "GAP: " << len << ", " << lvl << std::endl;
                return State::Gap;
            }
        }
        return State::End;
    }
    HighPassButterworthFilter _hpFilter;
    LowPassButterworthFilter _lpFilter;
    Rectifier _rectifier;
    State _state{State::Init};
    uint32_t _sampleRate{};
    std::span<const int16_t> _samples;
    size_t _idx{};
    uint32_t _zeroLowFreq{};
    uint32_t _oneLowFreq{};
    uint32_t _zeroHighFreq{};
    uint32_t _oneHighFreq{};
};

int main(int argc, char *argv[])
{
    fs::u8arguments args(argc, argv);
    std::vector<std::string> inputList;

    if (!args.valid()) {
        std::cerr << "WARNING: Unsupported non-UTF8 locale." << std::endl;
    }

    ghc::CLI cli(argc, argv);
    cli.positional(inputList, "Files to work on");
    cli.parse();


if (false && !inputList.empty()) {
    auto wav = WavFile<int16_t>(inputList[0]);
    constexpr uint32_t zeroFreq = 2000;
    constexpr uint32_t oneFreq  = 800;
    const uint32_t zeroHalfwave = wav.sampleRate() / 2 / zeroFreq;
    const uint32_t oneHalfwave  = wav.sampleRate() / 2 / oneFreq;
    HighPassButterworthFilter hp{wav.sampleRate(), (std::min)(zeroFreq, oneFreq)};
    LowPassButterworthFilter lp{wav.sampleRate(), (std::max)(zeroFreq, oneFreq)};
    Rectifier rectifier{6000};
    std::vector<int16_t> filteredSamples;
    filteredSamples.reserve(wav.samples().size());
    for (const auto& sample : wav.samples()) {
        auto [len, lvl]= rectifier(lp(hp(sample)));
        if (len > 0) {
            auto f = wav.sampleRate() / len / 2;
            if (f > 1400 && f < 2500) {
                // 0-bit
                filteredSamples.insert(filteredSamples.end(), zeroHalfwave, lvl);
            }
            else if (f > 500 && f < 1400) {
                filteredSamples.insert(filteredSamples.end(), oneHalfwave, lvl);
            }
            else {
                filteredSamples.insert(filteredSamples.end(), len, 0);
            }
        }
    }
    wav.writeWav<int16_t>("filtered_output.wav", filteredSamples, wav.sampleRate());
}

    if (!inputList.empty()) {
        auto wav = WavFile<int16_t>(inputList[0]);
        constexpr uint32_t zeroFreq = 2000;
        constexpr uint32_t oneFreq  = 800;
        TapeDecoder tape{wav, zeroFreq, oneFreq};
        while (true) {
            auto startMarker = tape.scanForStartMarker();
            if (startMarker > 0) {
                std::cout << "Start marker found, length " << startMarker << std::endl;
                DataBlockFormatter out{[](std::string_view s) { std::cout << s;}};
                std::optional<uint8_t> nextByte;
                do {
                    nextByte = tape.decodeByte();
                    if (nextByte.has_value()) {
                        out.write(fmt::format(" {:02X}", *nextByte));
                    }
                }
                while (nextByte.has_value());
            }
            else {
                std::cout << "Start marker not found!" << std::endl;
                break;
            }
        }
    }
#if 0
    for (const auto& input : inputList) {
        std::cout << "File: " << input << std::endl;
        WavFile<int16_t> wav(input);
        wav.estimateAmplitudeThreshold();

        const uint32_t zeroFreq = 2000; // 0-bit frequency
        const uint32_t oneFreq  =  800; // 1-bit frequency

        size_t idx = 0;
        size_t numSamples = wav.samples().size();
        std::vector<uint8_t> bits;

        while (idx < wav.samples().size())
        {
            auto bit = wav.decodeNextBit(zeroFreq, oneFreq, idx);
            //if (!bit) break; // no more or undecodable
            if (bit.has_value()) {
                bits.push_back(*bit);
                std::cout << (int)*bit << " ";
                if (bits.size() % 16 == 0) std::cout << std::endl;
            }
            if (idx & 0x3ff == 0) {
                std::cout << idx << "/" << numSamples << std::endl;
            }
        }
    }
#endif
    return 0;
}