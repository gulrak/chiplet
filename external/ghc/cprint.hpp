//---------------------------------------------------------------------------------------
// cprint.hpp
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
#include <iosfwd>
#include <fmt/format.h>

namespace ghc {

// Simple cross-platform color enum
enum class Color {
    DEFAULT,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    MAGENTA,
    CYAN,
    BOLD
};

namespace detail {
// Implemented in the .cpp to keep system/OS headers out of the public surface.
void emit_colored(std::ostream& os, Color color, std::string_view text);
} // namespace detail

// cprint: first parameter is the Color, then fmt-style format string + args.
// Example: cprint::cprint(cprint::Color::GREEN, "Hello, {}!\n", "world");
template <typename... Args>
inline void cprint(Color color, fmt::format_string<Args...> fmt_str, Args&&... args) {
    auto formatted = fmt::format(fmt_str, std::forward<Args>(args)...);
    detail::emit_colored(std::cout, color, formatted);
}

} // namespace ghc
