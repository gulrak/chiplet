//---------------------------------------------------------------------------------------
// chiplet/wavfile.hpp
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

#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <libresample.h>
#include "utility.hpp"

template <typename SampleT>
class WavFile {
public:
    using Sample = SampleT;
    static_assert(std::is_integral_v<Sample>);
    explicit WavFile(const std::string& filename, std::optional<uint32_t> sampleFreq = std::nullopt)
    {
        load(filename, sampleFreq);
    }

    // Get mono samples in unsigned 8bit, center at 128.
    const std::vector<Sample>& samples() const noexcept { return _samples; }
    // Get Sample rate in Hz
    uint32_t sampleRate() const noexcept { return _sampleRate; }

    // Write mono PCM WAV from 8-bit or 16-bit PCM vector
    template <typename T>
    requires (std::is_same_v<T, uint8_t> || std::is_same_v<T, int16_t>)
    void writeWav(const std::string& filename, const std::span<T>& pcm, uint32_t rate) const {
        if (pcm.empty()) throw std::runtime_error("No samples to write");
        const uint16_t channels = 1; // mono
        const uint16_t bitsPerSample = static_cast<uint16_t>(sizeof(T) * 8);
        const uint16_t blockAlign = channels * static_cast<uint16_t>(sizeof(T));
        const uint32_t byteRate = rate * blockAlign;
        const uint32_t dataSize = static_cast<uint32_t>(pcm.size() * sizeof(T));
        const uint32_t fmtSize  = 16; // PCM
        const uint32_t riffSize = 4 + (8 + fmtSize) + (8 + dataSize); // "WAVE" + fmt + data

        std::ofstream f(filename, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open output file: " + filename);

        auto w4 = [&](uint32_t v) {
            char b[4] = { char(v & 0xFF), char((v >> 8) & 0xFF), char((v >> 16) & 0xFF), char((v >> 24) & 0xFF) };
            f.write(b, 4);
        };
        auto w2 = [&](uint16_t v) {
            char b[2] = { char(v & 0xFF), char((v >> 8) & 0xFF) };
            f.write(b, 2);
        };
        auto wcc = [&](const char id[4]) { f.write(id, 4); };

        // RIFF header
        wcc("RIFF");
        w4(riffSize);
        wcc("WAVE");

        // fmt  chunk
        wcc("fmt ");
        w4(fmtSize);
        w2(1);  // PCM format tag
        w2(channels);
        w4(rate);
        w4(byteRate);
        w2(blockAlign);
        w2(bitsPerSample);

        // data chunk
        wcc("data");
        w4(dataSize);

        if constexpr (std::is_same_v<T, uint8_t>) {
            // contiguous write
            f.write(reinterpret_cast<const char*>(pcm.data()), static_cast<std::streamsize>(pcm.size()));
        } else {
            // write int16_t little-endian (portable)
            for (auto w : pcm) {
                w2(static_cast<uint16_t>(w));
            }
        }

        if (!f) throw std::runtime_error("Error writing WAV file");
    }

    // Convenience overload that uses the loader's sampleRate()
    template <typename T>
    requires (std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t>)
    void writeWav(const std::string& filename, const std::span<T>& pcm) const {
        writeWav<T>(filename, pcm, _sampleRate);
    }

    std::optional<uint8_t> decodeNextBit(uint32_t zeroFreq,
                                         uint32_t oneFreq,
                                         size_t& sampleIndex) const
    {
        if (sampleIndex + 2 >= _samples.size())
            return std::nullopt;

        if (_amplituteThreshold <= 0)
            return std::nullopt;

        // Measure one full wave period (in samples) starting at sampleIndex.
        auto periodOpt = measurePeriodSamples(sampleIndex, _amplituteThreshold);
        if (!periodOpt)
            return std::nullopt;

        const double measuredPeriod = *periodOpt;

        const double expectedZero = static_cast<double>(_sampleRate) / zeroFreq;
        const double expectedOne  = static_cast<double>(_sampleRate) / oneFreq;

        const double errZero = std::abs(measuredPeriod - expectedZero) / expectedZero;
        const double errOne  = std::abs(measuredPeriod - expectedOne) / expectedOne;

        // Allow quite a bit of wiggle room for analog jitter.
        constexpr double maxRelError = 0.25; // ±25%

        if (errZero < errOne && errZero < maxRelError)
            return static_cast<uint8_t>(0);
        if (errOne < errZero && errOne < maxRelError)
            return static_cast<uint8_t>(1);

        // Too ambiguous to classify.
        return std::nullopt;
    }


    void estimateAmplitudeThreshold()
    {
        for (int16_t s : _samples)
        {
            int32_t a = std::abs(static_cast<int32_t>(s));
            if (a > _maxAmplitude)
                _maxAmplitude = a;
        }
        if (_maxAmplitude == 0)
            return;

        // Take ~20% of peak, but not less than 500 (tweak to taste).
        int32_t thr = std::max<int32_t>(_maxAmplitude / 5, 500);
        if (thr > std::numeric_limits<int16_t>::max())
            thr = std::numeric_limits<int16_t>::max();
        _amplituteThreshold = thr;
    }

private:
    std::vector<Sample> _samples;
    std::vector<float> _floatSamples;
    uint32_t _sampleRate{};
    int32_t _maxAmplitude{};
    int32_t _amplituteThreshold{500};

    static uint32_t read_u32_le(std::ifstream& f) {
        uint8_t b[4];
        if (!f.read(reinterpret_cast<char*>(b), 4)) throw std::runtime_error("Unexpected EOF");
        return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
    }
    static uint16_t read_u16_le(std::ifstream& f) {
        uint8_t b[2];
        if (!f.read(reinterpret_cast<char*>(b), 2)) throw std::runtime_error("Unexpected EOF");
        return static_cast<uint16_t>(b[0]) | (static_cast<uint16_t>(b[1]) << 8);
    }
    static float read_f32_le(std::ifstream& f) {
        uint8_t b[4];
        if (!f.read(reinterpret_cast<char*>(b), 4)) throw std::runtime_error("Unexpected EOF");
        uint32_t u = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
        float v;
        static_assert(sizeof(v) == sizeof(u));
        std::memcpy(&v, &u, sizeof(v)); // strict-aliasing-safe
        return v;
    }
    static void read_exact(std::ifstream& f, char* dst, std::streamsize n) {
        if (!f.read(dst, n)) throw std::runtime_error("Unexpected EOF");
    }
    static void skip_exact(std::ifstream& f, std::streamoff n) {
        if (!f.seekg(n, std::ios::cur)) throw std::runtime_error("Bad seek while skipping chunk");
    }
    static bool fourcc_eq(const char* id, const char* want) {
        return id[0]==want[0] && id[1]==want[1] && id[2]==want[2] && id[3]==want[3];
    }

    void load(const std::string& filename, std::optional<uint32_t> sampleFreq) {
        std::ifstream f(filename, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open file: " + filename);

        char riff[4]; read_exact(f, riff, 4);
        if (!fourcc_eq(riff, "RIFF")) throw std::runtime_error("Not a RIFF file");
        (void)read_u32_le(f);
        char wave[4]; read_exact(f, wave, 4);
        if (!fourcc_eq(wave, "WAVE")) throw std::runtime_error("Not a WAVE file");

        bool haveFmt = false, haveData = false;

        uint16_t audioFormat = 0;
        uint16_t numChannels = 0;
        uint32_t sampleRate  = 0;
        uint16_t bitsPerSample = 0;
        uint32_t dataSize = 0;
        std::streampos dataStartPos{};

        while (f && (!haveFmt || !haveData)) {
            char chunkId[4];
            if (!f.read(chunkId, 4)) break;
            uint32_t chunkSize = read_u32_le(f);

            if (fourcc_eq(chunkId, "fmt ")) {
                audioFormat = read_u16_le(f);
                numChannels = read_u16_le(f);
                sampleRate  = read_u32_le(f);
                (void)read_u32_le(f); // byteRate
                (void)read_u16_le(f); // blockAlign
                bitsPerSample = read_u16_le(f);

                std::streamoff consumed = 16;
                if (chunkSize > consumed) skip_exact(f, chunkSize - consumed);
                haveFmt = true;
            } else if (fourcc_eq(chunkId, "data")) {
                dataSize = chunkSize;
                dataStartPos = f.tellg();
                skip_exact(f, chunkSize);
                haveData = true;
            } else {
                skip_exact(f, chunkSize);
            }
            if (chunkSize & 1u) skip_exact(f, 1);
        }

        if (!haveFmt)  throw std::runtime_error("Missing 'fmt ' chunk");
        if (!haveData) throw std::runtime_error("Missing 'data' chunk");

        // Accept: PCM 8/16-bit (format 1), or IEEE float 32-bit (format 3)
        const bool isPcmInt   = (audioFormat == 1) && (bitsPerSample == 8 || bitsPerSample == 16);
        const bool isPcmFloat = (audioFormat == 3) && (bitsPerSample == 32);

        if (!isPcmInt && !isPcmFloat) {
            throw std::runtime_error("Unsupported format: need PCM 8/16-bit (fmt=1) or 32-bit float (fmt=3)");
        }
        if (numChannels == 0) throw std::runtime_error("Invalid channel count");

        _sampleRate = sampleRate;

        f.clear();
        f.seekg(dataStartPos);
        if (!f) throw std::runtime_error("Failed to seek to audio data");

        const uint32_t bytesPerSample = bitsPerSample / 8u;
        if (bytesPerSample == 0) throw std::runtime_error("Invalid bitsPerSample");
        const uint64_t totalSamples = dataSize / bytesPerSample;
        const uint64_t totalFrames  = totalSamples / numChannels;

        if (numChannels == 1 && (!sampleFreq || sampleFreq.value() != sampleRate)) {
            if (bitsPerSample == 8 && sizeof(Sample) == 1) {
                _samples.clear();
                _samples.resize(totalFrames);
                f.read(reinterpret_cast<std::istream::char_type*>(_samples.data()), totalFrames);
                return;
            }
            if (bitsPerSample == 16 && sizeof(Sample) == 2) {
                if (bitsPerSample == 8) {
                    _samples.clear();
                    _samples.resize(totalFrames);
                    f.read(reinterpret_cast<std::istream::char_type*>(_samples.data()), totalFrames*2);
                    return;
                }
            }
        }
        _floatSamples.clear();
        _floatSamples.reserve(totalFrames);

        if (isPcmFloat) {
            // IEEE 754 float32, little-endian, interleaved
            for (uint64_t i = 0; i < totalFrames; ++i) {
                float acc = 0.0f;
                for (uint16_t ch = 0; ch < numChannels; ++ch) {
                    float v = read_f32_le(f);
                    acc += v;
                }
                _floatSamples.push_back(acc / static_cast<float>(numChannels));
            }
        } else if (bitsPerSample == 8) {
            for (uint64_t i = 0; i < totalFrames; ++i) {
                float acc = 0.0f;
                for (uint16_t ch = 0; ch < numChannels; ++ch) {
                    uint8_t s;
                    if (!f.read(reinterpret_cast<char*>(&s), 1)) throw std::runtime_error("Unexpected EOF in data");
                    float v = (static_cast<int>(s) - 128) / 128.0f; // ≈[-1, +1]
                    acc += v;
                }
                _floatSamples.push_back(acc / static_cast<float>(numChannels));
            }
        } else { // 16-bit
            for (uint64_t i = 0; i < totalFrames; ++i) {
                float acc = 0.0f;
                for (uint16_t ch = 0; ch < numChannels; ++ch) {
                    uint8_t b[2];
                    if (!f.read(reinterpret_cast<char*>(b), 2)) throw std::runtime_error("Unexpected EOF in data");
                    int16_t s = static_cast<int16_t>( (uint16_t)b[0] | ((uint16_t)b[1] << 8) );
                    float v = static_cast<float>(s) / 32768.0f; // ≈[-1, +1)
                    acc += v;
                }
                _floatSamples.push_back(acc / static_cast<float>(numChannels));
            }
        }
        if (sampleFreq && sampleFreq.value() != sampleRate) {
            resampleTo(sampleFreq.value());
        }
        _samples = toPcm<Sample>(); // convert to 8-bit
        _floatSamples.clear();
    }

    // Resample samples_ to dstRate, in-place, using libresample.
    // highQuality=true selects the library's "highQuality" path.
    void resampleTo(uint32_t dstRate, bool highQuality = true) {
        if (dstRate == 0) throw std::runtime_error("dstRate must be > 0");
        if (_floatSamples.empty()) { _sampleRate = dstRate; return; }
        if (dstRate == _sampleRate) return;

        const double factor = static_cast<double>(dstRate) / static_cast<double>(_sampleRate);
        if (factor <= 0.0) throw std::runtime_error("Invalid resampling factor");

        // Open a resampler; since our factor is constant, set min=max=factor.
        void* h = resample_open(highQuality ? 1 : 0, factor, factor);
        if (!h) throw std::runtime_error("resample_open failed");

        const int fw = resample_get_filter_width(h); // for headroom
        // Conservative output capacity estimate:
        const size_t inN = _floatSamples.size();
        size_t estOut = static_cast<size_t>(inN * factor) + static_cast<size_t>(fw * 4 + 64);

        std::vector<float> out;
        out.reserve(estOut);

        const int CHUNK = 4096;                 // process input in chunks
        std::vector<float> outChunk;
        // Output per chunk: approx CHUNK*factor + margin
        const int outChunkCap = static_cast<int>(CHUNK * std::max(1.0, factor) + fw * 4 + 64);
        outChunk.resize(outChunkCap);

        int inPos = 0;
        while (inPos < static_cast<int>(inN)) {
            const int inAvail = static_cast<int>(inN) - inPos;
            const int take = std::min(CHUNK, inAvail);

            int inUsed = 0;
            const int lastFlag = (inPos + take >= static_cast<int>(inN)) ? 1 : 0;

            int produced = resample_process(
                h,
                factor,
                _floatSamples.data() + inPos,
                take,
                lastFlag,
                &inUsed,
                outChunk.data(),
                static_cast<int>(outChunk.size())
            );
            if (inUsed < 0 || produced < 0) {
                resample_close(h);
                throw std::runtime_error("resample_process failed");
            }

            inPos += inUsed;
            if (produced > 0) {
                out.insert(out.end(), outChunk.begin(), outChunk.begin() + produced);
            }

            // If no input consumed AND no output produced, enlarge buffer and try again
            if (inUsed == 0 && produced == 0) {
                outChunk.resize(outChunk.size() * 2 + 256);
            }
        }

        resample_close(h);

        // Replace internal buffer and rate
        _floatSamples.swap(out);
        _sampleRate = dstRate;
    }

    // Convert float mono samples to 8-bit unsigned or 16-bit signed PCM containers
    template <typename T>
    requires (std::is_same_v<T, uint8_t> || std::is_same_v<T, int16_t>)
    std::vector<T> toPcm() const {
        std::vector<T> out;
        out.reserve(_floatSamples.size());

        for (float v : _floatSamples) {
            // clamp to [-1, 1]
            if (v > 1.0f) v = 1.0f;
            else if (v < -1.0f) v = -1.0f;

            if constexpr (std::is_same_v<T, uint8_t>) {
                // 8-bit PCM: unsigned 0..255, ~center at 128
                int s = static_cast<int>(std::lround(v * 127.0f + 128.0f));
                if (s < 0) s = 0; else if (s > 255) s = 255;
                out.push_back(static_cast<uint8_t>(s));
            } else {
                // int16_t target
                int val = static_cast<int>(std::lround(v * 32767.0f));
                if (val < -32768) val = -32768;
                if (val >  32767) val =  32767;
                out.push_back(static_cast<int16_t>(val));
            }
        }
        return out;
    }

    std::optional<double> measurePeriodSamples(size_t& index, int16_t minAmplitude) const
    {
        const size_t n = _samples.size();
        if (index + 2 >= n)
            return std::nullopt;

        size_t firstCross = n;
        bool firstUpward = false;

        // 1) Find the first significant zero crossing from index onwards.
        for (size_t i = index + 1; i < n; ++i)
        {
            int16_t s0 = _samples[i - 1];
            int16_t s1 = _samples[i];

            int sg0 = sign(s0);
            int sg1 = sign(s1);
            if (sg0 == 0 || sg1 == 0)
                continue; // skip exact zeros

            // Actual zero crossing?
            if (sg0 != sg1)
            {
                int16_t maxAround = std::max<int16_t>(std::abs(s0), std::abs(s1));
                if (maxAround < minAmplitude)
                    continue; // too close to zero, probably noise

                firstCross = i;
                firstUpward = (sg0 < sg1); // going from - to + ? => upward
                break;
            }
        }

        if (firstCross == n)
            return std::nullopt; // No crossing found

        // 2) Find the next zero crossing with the *same direction* as the first.
        size_t secondCross = n;
        for (size_t i = firstCross + 1; i < n; ++i)
        {
            int16_t s0 = _samples[i - 1];
            int16_t s1 = _samples[i];

            int sg0 = sign(s0);
            int sg1 = sign(s1);
            if (sg0 == 0 || sg1 == 0)
                continue;

            if (sg0 != sg1)
            {
                int16_t maxAround = std::max<int16_t>(std::abs(s0), std::abs(s1));
                if (maxAround < minAmplitude)
                    continue;

                bool upward = (sg0 < sg1);
                if (upward == firstUpward)
                {
                    secondCross = i;
                    break;
                }
            }
        }

        if (secondCross == n)
            return std::nullopt; // Could not find a full period

        double period = static_cast<double>(secondCross - firstCross);

        // Move index to the start of the next bit (boundary at second crossing).
        index = secondCross;
        return period;
    }

};


class WavStreamWriter {
public:
    enum class ChannelLayout : uint16_t {
        Mono   = 1,
        Stereo = 2
    };

    WavStreamWriter() = default;

    WavStreamWriter(const WavStreamWriter&) = delete;
    WavStreamWriter& operator=(const WavStreamWriter&) = delete;

    ~WavStreamWriter() {
        if (isOpen()) {
            try {
                close();
            } catch (...) {
                // Destructors must not throw
            }
        }
    }

    /// Open a new WAV file for streaming samples.
    /// If another file is already open, it will be closed first.
    ///
    /// @param filename    Path to the output file
    /// @param sampleRate  Samples per second (e.g., 44100)
    /// @param layout      Mono or Stereo (default Stereo)
    void open(const std::string& filename,
              uint32_t sampleRate,
              ChannelLayout layout = ChannelLayout::Stereo)
    {
        if (_out.is_open()) {
            close(); // finish previous file safely
        }

        _out.open(filename, std::ios::binary | std::ios::trunc);
        if (!_out) {
            throw std::runtime_error("Failed to open WAV file for writing: " + filename);
        }

        _sampleRate    = sampleRate;
        _numChannels   = static_cast<uint16_t>(layout);
        _bitsPerSample = 16;
        _dataBytesWritten = 0;

        writeWavHeaderPlaceholder();
        _isOpen = true;
    }

    /// Returns true if a file is currently open.
    bool isOpen() const noexcept {
        return _out.is_open() && _isOpen;
    }

    /// Append a single mono sample (valid only if opened as Mono).
    void appendSample(int16_t sample) {
        ensureOpen();
        if (_numChannels != 1) {
            throw std::runtime_error("appendSample(int16_t) called, but file is not mono.");
        }
        writeInt16LE(sample);
        _dataBytesWritten += sizeof(int16_t);
    }

    /// Append a single stereo sample (L, R) (valid only if opened as Stereo).
    void appendSample(int16_t left, int16_t right) {
        ensureOpen();
        if (_numChannels != 2) {
            throw std::runtime_error("appendSample(int16_t, int16_t) called, but file is not stereo.");
        }
        writeInt16LE(left);
        writeInt16LE(right);
        _dataBytesWritten += 2 * sizeof(int16_t);
    }

    /// Append N copies of the same mono sample.
    void appendSamples(int16_t sample, std::size_t count) {
        ensureOpen();
        if (_numChannels != 1) {
            throw std::runtime_error("appendSamples(int16_t, size_t) called, but file is not mono.");
        }

        for (std::size_t i = 0; i < count; ++i) {
            writeInt16LE(sample);
        }
        _dataBytesWritten += count * sizeof(int16_t);
    }

    /// Append N copies of the same stereo sample (L, R).
    void appendSamples(int16_t left, int16_t right, std::size_t count) {
        ensureOpen();
        if (_numChannels != 2) {
            throw std::runtime_error("appendSamples(int16_t, int16_t, size_t) called, but file is not stereo.");
        }

        for (std::size_t i = 0; i < count; ++i) {
            writeInt16LE(left);
            writeInt16LE(right);
        }
        _dataBytesWritten += count * 2 * sizeof(int16_t);
    }

    /// Finalize the WAV header with the correct sizes and close the file.
    /// After this, the object can be reused with another call to open().
    void close() {
        if (!isOpen()) {
            resetState();
            return;
        }

        try {
            fixupHeader();
        } catch (...) {
            _out.close();
            resetState();
            throw;
        }

        _out.close();
        resetState();
    }

private:
    std::ofstream _out;
    uint32_t _sampleRate = 0;
    uint16_t _numChannels = 0;
    uint16_t _bitsPerSample = 16;
    uint32_t _dataBytesWritten = 0;
    bool _isOpen = false;

    void resetState() noexcept {
        _sampleRate = 0;
        _numChannels = 0;
        _bitsPerSample = 16;
        _dataBytesWritten = 0;
        _isOpen = false;
    }

    void ensureOpen() {
        if (!isOpen()) {
            throw std::runtime_error("WavStreamWriter: no file is currently open.");
        }
    }

    void writeUint32LE(uint32_t value) {
        char buf[4];
        buf[0] = static_cast<char>( value        & 0xFF);
        buf[1] = static_cast<char>((value >> 8)  & 0xFF);
        buf[2] = static_cast<char>((value >> 16) & 0xFF);
        buf[3] = static_cast<char>((value >> 24) & 0xFF);
        _out.write(buf, 4);
        if (!_out) {
            throw std::runtime_error("Failed to write uint32_t to WAV file.");
        }
    }

    void writeUint16LE(uint16_t value) {
        char buf[2];
        buf[0] = static_cast<char>( value       & 0xFF);
        buf[1] = static_cast<char>((value >> 8) & 0xFF);
        _out.write(buf, 2);
        if (!_out) {
            throw std::runtime_error("Failed to write uint16_t to WAV file.");
        }
    }

    void writeInt16LE(int16_t value) {
        writeUint16LE(static_cast<uint16_t>(value));
    }

    void writeWavHeaderPlaceholder() {
        _out.write("RIFF", 4);
        // ChunkSize placeholder (will be 36 + Subchunk2Size)
        writeUint32LE(0);
        _out.write("WAVE", 4);
        _out.write("fmt ", 4);
        writeUint32LE(16);
        writeUint16LE(1);
        writeUint16LE(_numChannels);
        writeUint32LE(_sampleRate);
        uint32_t byteRate = _sampleRate * _numChannels * (_bitsPerSample / 8);
        writeUint32LE(byteRate);
        uint16_t blockAlign = _numChannels * (_bitsPerSample / 8);
        writeUint16LE(blockAlign);
        writeUint16LE(_bitsPerSample);
        _out.write("data", 4);
        writeUint32LE(0);

        if (!_out) {
            throw std::runtime_error("Failed to write WAV header.");
        }
    }

    void fixupHeader() {
        uint32_t subchunk2Size = _dataBytesWritten;
        uint32_t chunkSize = 36 + subchunk2Size;
        _out.seekp(4, std::ios::beg);
        if (!_out) {
            throw std::runtime_error("Failed to seek to ChunkSize in WAV header.");
        }
        writeUint32LE(chunkSize);
        _out.seekp(40, std::ios::beg);
        if (!_out) {
            throw std::runtime_error("Failed to seek to Subchunk2Size in WAV header.");
        }
        writeUint32LE(subchunk2Size);
        _out.seekp(0, std::ios::end);
        if (!_out) {
            throw std::runtime_error("Failed to seek to end after fixing WAV header.");
        }
    }
};

