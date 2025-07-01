//----------------------------------------------------------------------------
// C++ "port" by Steffen Schümann on 22.03.25, released under CC0
//----------------------------------------------------------------------------
// Based on xoshiro256plus.c
// Written in 2018 by David Blackman and Sebastiano Vigna (vigna@acm.org)
//
// To the extent possible under law, the author has dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
// IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//----------------------------------------------------------------------------
#pragma once

#include <chrono>
#include <cstdint>
#include <thread>

class Xoschiro256Plus
{
public:
    Xoschiro256Plus()
    {
        // This is not cryptographic, just meant to be video game quality random and per-instance and per-thread different
        reseed(std::chrono::high_resolution_clock::now().time_since_epoch().count() ^ static_cast<uint64_t>(std::hash<std::thread::id>()(std::this_thread::get_id())) ^ reinterpret_cast<uint64_t>(this));
    }
    explicit Xoschiro256Plus(uint64_t seed) { reseed(seed); }

    uint64_t operator()() { return next(); }

    void reseed(uint64_t seed)
    {
        s[0] = splitMix64(seed);
        s[1] = splitMix64(seed);
        s[2] = splitMix64(seed);
        s[3] = splitMix64(seed);
    }

private:
    static uint64_t rotl(const uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
    uint64_t next(void) {
        const uint64_t result = s[0] + s[3];

        const uint64_t t = s[1] << 17;

        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];

        s[2] ^= t;

        s[3] = rotl(s[3], 45);

        return result;
    }
    uint64_t splitMix64(uint64_t& x)
    {
        uint64_t z = (x += 0x9e3779b97f4a7c15);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        return z ^ (z >> 31);
    }
    uint64_t s[4]{};
};