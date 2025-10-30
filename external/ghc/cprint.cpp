///---------------------------------------------------------------------------------------
// cprint.cpp
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

#include "cprint.hpp"

#include <iostream>
#include <mutex>
#include <string_view>

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

namespace ghc {
namespace {

// Map Color to ANSI SGR codes (as string views to avoid allocations)
constexpr std::string_view ansi_code_for(Color c) {
    switch (c) {
        case Color::RED:     return "\x1b[31m";
        case Color::GREEN:   return "\x1b[32m";
        case Color::YELLOW:  return "\x1b[33m";
        case Color::BLUE:    return "\x1b[34m";
        case Color::MAGENTA: return "\x1b[35m";
        case Color::CYAN:    return "\x1b[36m";
        case Color::BOLD:    return "\x1b[1m";
        case Color::DEFAULT: return "";
    }
    return "";
}

constexpr std::string_view ansi_reset() { return "\x1b[0m"; }

#if defined(_WIN32)
// Enable Windows 10+ virtual terminal processing once.
// If enabling VT fails, we’ll fall back to SetConsoleTextAttribute.
bool enable_vt_once() {
    static std::once_flag once;
    static bool vt_ok = false;
    std::call_once(once, [] {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!hOut || hOut == INVALID_HANDLE_VALUE) { vt_ok = false; return; }

        DWORD mode = 0;
        if (!GetConsoleMode(hOut, &mode)) { vt_ok = false; return; }

        DWORD desired = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (!SetConsoleMode(hOut, desired)) {
            // Some terminals (e.g., old cmd) refuse VT. That’s fine; we’ll fallback.
            vt_ok = false;
            return;
        }
        vt_ok = true;
    });
    return vt_ok;
}

WORD win_color_attr_for(Color c, WORD original) {
    switch (c) {
        case Color::RED:     return FOREGROUND_RED   | FOREGROUND_INTENSITY;
        case Color::GREEN:   return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case Color::YELLOW:  return FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case Color::BLUE:    return FOREGROUND_BLUE  | FOREGROUND_INTENSITY;
        case Color::MAGENTA: return FOREGROUND_RED   | FOREGROUND_BLUE  | FOREGROUND_INTENSITY;
        case Color::CYAN:    return FOREGROUND_GREEN | FOREGROUND_BLUE  | FOREGROUND_INTENSITY;
        case Color::BOLD:    return original | FOREGROUND_INTENSITY;
        case Color::DEFAULT: return 0; // We'll restore saved attributes.
    }
    return 0;
}
#endif

// Serialize writes so multi-threaded calls don’t interleave.
std::mutex& out_mutex() {
    static std::mutex m;
    return m;
}

} // namespace

void detail::emit_colored(std::ostream& os, Color color, std::string_view text) {
    std::lock_guard<std::mutex> lock(out_mutex());

#if defined(_WIN32)
    // Try VT first for consistent ANSI handling.
    if (enable_vt_once()) {
        const auto pre = ansi_code_for(color);
        if (!pre.empty()) os << pre;
        os << text;
        if (!pre.empty()) os << ansi_reset();
        return;
    }

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut && hOut != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        WORD original = 0;
        if (GetConsoleScreenBufferInfo(hOut, &info)) {
            original = info.wAttributes;
        }

        WORD apply = win_color_attr_for(color, original);
        if (apply != original) {
            SetConsoleTextAttribute(hOut, apply);
            std::cout << text;
            SetConsoleTextAttribute(hOut, original);
        } else {
            std::cout << text;
        }
        return;
    }

    // If all else fails, just print plain text.
    os << text;
#else
    // POSIX: ANSI sequences are widely supported.
    const auto pre = ansi_code_for(color);
    if (!pre.empty()) os << pre;
    os << text;
    if (!pre.empty()) os << ansi_reset();
#endif
}

} // namespace ghc
