//---------------------------------------------------------------------------------------
// fpscount.hpp
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
#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>

namespace ghc {

namespace detail {
template <size_t N, typename ValueType = uint64_t, typename SumType = uint64_t>
class SMA
{
public:
    void reset()
    {
        _fill = _index = 0;
        _sum = 0;
        _min = std::numeric_limits<ValueType>::max();
        _max = std::numeric_limits<ValueType>::min();
    }
    void add(ValueType nextVal)
    {
        if (_fill < N) [[unlikely]]
            ++_fill;
        else
            _sum -= _history[_index];
        _sum += nextVal;
        _history[_index] = nextVal;
        if (++_index == N) [[unlikely]]
            _index = 0;
        if (nextVal < _min) [[unlikely]]
            _min = nextVal;
        if (nextVal > _max) [[unlikely]]
            _max = nextVal;
    }
    double get() const { return _fill ? double(_sum) / _fill : 0.0; }
    ValueType getMin() const { return _min; }
    ValueType getMax() const { return _max; }

private:
    size_t _fill{0};
    size_t _index{0};
    ValueType _history[N]{};
    SumType _sum{0};
    ValueType _min{0};
    ValueType _max{0};
};
}  // namespace detail

class FPSCounter
{
public:
    constexpr static unsigned WARMUP_FRAMES = 60;
    explicit FPSCounter(unsigned updateRate = 60)
        : _buffer(256, '\0')
        , _updateRate(updateRate)
    {
    }

    ~FPSCounter()
    {
    }

    /// Call this once per frame. Returns true when displayed stats should be updated, if current number of instructiuons is
    /// provided, it will also allow calculation of Mips.
    bool frameTick(uint64_t instructions = 0)
    {
        bool result = false;
        _frameCount++;
        if (!_totalFrames) [[unlikely]] {
            if (_frameCount < WARMUP_FRAMES)
                return false;
            _totalFrames = WARMUP_FRAMES;
            _frameCount = 0;
            _startCycles = _lastFrameCycles = _lastStatsCycles = instructions;
            _startTime = _lastFrameTime = _lastStatsTime = std::chrono::steady_clock::now();
            return false;
        }

        auto currentTime = std::chrono::steady_clock::now();
        _averageFrameMicroseconds.add(std::chrono::duration_cast<std::chrono::microseconds>(currentTime - _lastFrameTime).count());
        _lastFrameCycles = instructions;

        if (_frameCount == 60) [[unlikely]] {
            _statsTimeInMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - _lastStatsTime).count();
            _statsCycles = instructions - _lastStatsCycles;
            _lastStatsTime = currentTime;
            _lastStatsCycles = instructions;
            if (_statsTimeInMicroseconds) {
                _averageMips.add(static_cast<double>(_statsCycles) / _statsTimeInMicroseconds);
            }
            _lastFrameTime = currentTime;
            _totalFrames += _frameCount;
            _frameCount = 0;
            result = true;
        }
        _lastFrameTime = currentTime;
        return result;
    }

    /// Returns the 120 frame average FPS
    double getFPS() const { return 1000000.0 / _averageFrameMicroseconds.get(); }

    /// Returns the 120 frame average frame time in microseconds
    double getFrameTime_us() const { return _averageFrameMicroseconds.get(); }

    /// Returns a further smoothed Mips counter _if_ the number of instructions where provided to frameTick() or 0
    double getMips() const { return _averageMips.get(); }

    /// Returns the stats as a string, e.g. "<prefix> [59.92fps,123.45Mips]" (Mips only if available)
    const char* getStatsString(const std::string& prefix) const
    {
        if (_statsTimeInMicroseconds) {
            if (_statsCycles) {
                auto [value, unit] = withUnitPrefix(_averageMips.get() * 1'000'000.0);
                std::snprintf(_buffer.data(), _buffer.size() - 1, "%s [%.2ffps,%.2f%sips]", prefix.c_str(), getFPS(), value, unit);
            }
            else {
                std::snprintf(_buffer.data(), _buffer.size() - 1, "%s [%.2ffps]", prefix.c_str(), getFPS());
            }
            return _buffer.data();
        }
        return prefix.c_str();
    }

    void dumpTotalStats() const
    {
        auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(_lastStatsTime - _startTime).count();
        auto totalMips = static_cast<double>(_lastStatsCycles - _startCycles) / totalTime;
        auto totalFps = static_cast<double>(_totalFrames - WARMUP_FRAMES) / totalTime * 1000000.0;
        std::snprintf(_buffer.data(), _buffer.size() - 1, "Overall averages: %.2fps, %.2fMips", totalFps, totalMips);
        printf("%s\n", _buffer.data());
    }

private:
    static std::pair<double, const char*> withUnitPrefix(double value) {
        constexpr double thousand = 1'000.0;
        constexpr double million  = 1'000'000.0;
        constexpr double billion  = 1'000'000'000.0;
        double absVal = std::fabs(value);
        if (absVal >= billion) {
            return { value / billion, "G" };
        }
        if (absVal >= million) {
            return { value / million, "M" };
        }
        if (absVal >= thousand) {
            return { value / thousand, "k" };
        }
        if (absVal < 1.0) {
            return { value * 1000.0, "m" };
        }
        return { value, "" }; // no prefix
    }
    mutable std::vector<char> _buffer;
    const unsigned _updateRate{60};
    detail::SMA<120> _averageFrameMicroseconds;
    detail::SMA<5, double, double> _averageMips;
    std::chrono::steady_clock::time_point _startTime{};
    std::chrono::steady_clock::time_point _lastFrameTime{};
    std::chrono::steady_clock::time_point _lastStatsTime{};
    uint64_t _statsTimeInMicroseconds{};
    uint64_t _startCycles{};
    uint64_t _statsCycles{};
    uint64_t _lastFrameCycles{};
    uint64_t _lastStatsCycles{};
    uint64_t _totalFrames{};
    int _frameCount{};
};

}  // namespace ghc
