//---------------------------------------------------------------------------------------
// chiplet/oscillator.hpp
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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

class AdditiveWavetableOsc
{
public:
    static constexpr std::size_t tableSize = 1024;

    // Envelope times in seconds
    static constexpr double attackTimeSeconds = 0.012;  // 12 ms
    static constexpr double releaseTimeSeconds = 0.05;  // 50 ms

    explicit AdditiveWavetableOsc(double sampleRate)
        : _sampleRate(sampleRate)
        , _frequency(1450.0)
        , _secondaryFrequency(1450.0)
        , _phase(0.0)
        , _ampEnvelope(sampleRate)
        , _freqEnvelope(sampleRate)
    {
        _ampEnvelope.updateEnvelopeIncrements(attackTimeSeconds, releaseTimeSeconds);
        _freqEnvelope.updateEnvelopeIncrements(attackTimeSeconds * 2.0, releaseTimeSeconds);
        generateTable(16384);  // default peak
    }

    // Turn the generator on/off (AR-style)
    void activate(bool isActive)
    {
        _ampEnvelope.activate(isActive);
        _freqEnvelope.activate(isActive);
    }

    // Regenerate wavetable with the additive synthesis, normalized to targetPeak
    void generateTable(int targetPeak)
    {
        // Harmonic weights: strong 1st, 3rd, 4th; weaker 2nd, 5th
        constexpr double a1 = 1.0;
        constexpr double a2 = 0.5;
        constexpr double a3 = 1.0;
        constexpr double a4 = 1.0;
        constexpr double a5 = 0.5;

        constexpr double twoPi = 2.0 * std::numbers::pi_v<double>;
        double temp[tableSize];
        double maxAbs = 0.0;

        // Build unnormalized waveform
        for (std::size_t n = 0; n < tableSize; ++n) {
            double phase = static_cast<double>(n) / static_cast<double>(tableSize);  // 0..1
            double theta1 = twoPi * 1.0 * phase;
            double theta2 = twoPi * 2.0 * phase;
            double theta3 = twoPi * 3.0 * phase;
            double theta4 = twoPi * 4.0 * phase;
            double theta5 = twoPi * 5.0 * phase;

            double value = a1 * std::sin(theta1) + a2 * std::sin(theta2) + a3 * std::sin(theta3) + a4 * std::sin(theta4) + a5 * std::sin(theta5);

            temp[n] = value;
            maxAbs = std::max(maxAbs, std::abs(value));
        }

        if (maxAbs <= 0.0) {
            std::fill_n(_table, tableSize, static_cast<int16_t>(0));
            return;
        }

        double scale = static_cast<double>(targetPeak) / maxAbs;
        constexpr int16_t minS = std::numeric_limits<int16_t>::min();
        constexpr int16_t maxS = std::numeric_limits<int16_t>::max();

        for (std::size_t n = 0; n < tableSize; ++n) {
            double scaled = temp[n] * scale;
            long s = std::lround(scaled);

            if (s < static_cast<long>(minS))
                s = minS;
            if (s > static_cast<long>(maxS))
                s = maxS;

            _table[n] = static_cast<int16_t>(s);
        }
    }

    void setFrequency(double frequency)
    {
        _frequency = frequency;
        // If secondary was equal to old frequency, you might choose
        // to update it too; but here we keep it independent on purpose.
    }

    double getFrequency() const { return _frequency; }

    void setSecondaryFrequency(double frequency) { _secondaryFrequency = frequency; }

    double getSecondaryFrequency() const { return _secondaryFrequency; }

    // One output sample (int16_t, with interpolation + AR envelope + freq sweep)
    int16_t processSample()
    {
        // --- Envelope update ---
        double ampEnvLevel = _ampEnvelope.processSample();
        double freqEnvLevel = _freqEnvelope.processSample();

        // If idle (fully inactive), we can early out to guaranteed silence
        if (_ampEnvelope._envStage == EnvStage::Idle && ampEnvLevel <= 0.0) {
            return 0;
        }

        // --- Frequency envelope: interpolate between secondary and main ---
        // level = 0  -> use _secondaryFrequency
        // level = 1  -> use _frequency
        double currentFrequency = _secondaryFrequency + (_frequency - _secondaryFrequency) * freqEnvLevel;

        double step = (static_cast<double>(tableSize) * currentFrequency) / _sampleRate;

        // --- Oscillator wavetable read (always advances phase) ---
        int index = static_cast<int>(_phase);
        int indexNext = index + 1;
        if (indexNext >= static_cast<int>(tableSize))
            indexNext -= static_cast<int>(tableSize);

        double frac = _phase - static_cast<double>(index);

        double s0 = static_cast<double>(_table[index]);
        double s1 = static_cast<double>(_table[indexNext]);
        double sample = s0 + (s1 - s0) * frac;

        // Advance phase using current (enveloped) frequency
        _phase += step;
        while (_phase >= static_cast<double>(tableSize))
            _phase -= static_cast<double>(tableSize);

        // Apply envelope amplitude
        double output = sample * ampEnvLevel;

        // Clamp to int16_t
        constexpr int16_t minS = std::numeric_limits<int16_t>::min();
        constexpr int16_t maxS = std::numeric_limits<int16_t>::max();
        long s = std::lround(output);

        if (s < static_cast<long>(minS))
            s = minS;
        if (s > static_cast<long>(maxS))
            s = maxS;

        return static_cast<int16_t>(s);
    }

    // Convenience: render a block of samples into a buffer
    void processBlock(int16_t* output, std::size_t numSamples)
    {
        for (std::size_t i = 0; i < numSamples; ++i) {
            output[i] = processSample();
        }
    }

private:
    enum class EnvStage { Idle, Attack, Sustain, Release };

    struct EnvelopeGenerator
    {
        double _sampleRate;
        double _envelopeLevel;
        EnvStage _envStage;
        bool _gate;
        double _attackIncrement;
        double _releaseIncrement;

        EnvelopeGenerator(double sampleRate)
            : _sampleRate(sampleRate)
            , _envelopeLevel(0.0)
            , _envStage(EnvStage::Idle)
            , _gate(false)
            , _attackIncrement(0)
            , _releaseIncrement(0)
        {
        }
        ~EnvelopeGenerator() = default;
        void updateEnvelopeIncrements(double attackTime, double releaseTime)
        {
            constexpr double minTime = 1e-9;

            double atk = attackTime;
            double rel = releaseTime;

            if (atk <= minTime)
                _attackIncrement = 1.0;
            else
                _attackIncrement = 1.0 / (_sampleRate * atk);

            if (rel <= minTime)
                _releaseIncrement = 1.0;
            else
                _releaseIncrement = 1.0 / (_sampleRate * rel);
        }
        void activate(bool isActive)
        {
            _gate = isActive;
            if (_gate) {
                // Turned ON: go into attack from Idle or Release
                if (_envStage == EnvStage::Idle || _envStage == EnvStage::Release) {
                    _envStage = EnvStage::Attack;
                }
            }
            else {
                // Turned OFF: go into release from Attack or Sustain
                if (_envStage == EnvStage::Attack || _envStage == EnvStage::Sustain) {
                    _envStage = EnvStage::Release;
                }
            }
        }
        double processSample()
        {
            double level = _envelopeLevel;
            switch (_envStage) {
                case EnvStage::Idle:
                    level = 0.0;
                    break;

                case EnvStage::Attack:
                    level += _attackIncrement;
                    if (level >= 1.0) {
                        level = 1.0;
                        // If gate still on, go to sustain, else start release
                        _envStage = _gate ? EnvStage::Sustain : EnvStage::Release;
                    }
                    break;

                case EnvStage::Sustain:
                    level = 1.0;
                    break;

                case EnvStage::Release:
                    level -= _releaseIncrement;
                    if (level <= 0.0) {
                        level = 0.0;
                        // If gate was re-opened during release, go back to attack
                        _envStage = _gate ? EnvStage::Attack : EnvStage::Idle;
                    }
                    break;
            }

            // Clamp envelope level to [0, 1]
            if (level < 0.0)
                level = 0.0;
            if (level > 1.0)
                level = 1.0;
            _envelopeLevel = level;
            return level;
        }
    };

    double _sampleRate;
    double _frequency;           // main fundamental
    double _secondaryFrequency;  // "start/end" frequency for attack/release
    double _phase;
    bool _gate;

    // Envelope states
    EnvelopeGenerator _ampEnvelope;
    EnvelopeGenerator _freqEnvelope;

    int16_t _table[tableSize];
};
