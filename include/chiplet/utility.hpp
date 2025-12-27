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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include <fmt/format.h>
#include <chiplet/sha1.hpp>
#include <ghc/fs_fwd.hpp>
#include <tl/expected.hpp>

namespace fs = ghc::filesystem;

namespace ghc {
template <class T, class E>
using expected = tl::expected<T, E>;

using tl::bad_expected_access;
using tl::unexpected;
}  // namespace ghc

template <class T, class U>
concept StaticCastable = requires(U&& u) { static_cast<T>(std::forward<U>(u)); };

template <class T, class U>
    requires StaticCastable<T, U>
[[nodiscard]] constexpr T as(U&& value) noexcept(noexcept(static_cast<T>(std::forward<U>(value))))
{
    return static_cast<T>(std::forward<U>(value));
}

template <typename E, typename = std::enable_if_t<std::is_enum<E>::value, E>>
inline constexpr auto toType(E& e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

inline bool endsWith(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size() && 0 == text.compare(text.size() - suffix.size(), suffix.size(), suffix);
}

inline bool startsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && 0 == text.compare(0, prefix.size(), prefix);
}

inline std::string trimLeft(std::string s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    return s;
}

inline std::string trimRight(std::string s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    return s;
}

inline std::string trim(std::string s)
{
    return trimRight(trimLeft(s));
}

constexpr std::string_view trim(std::string_view str)
{
    const auto start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string_view::npos) {
        return {};
    }
    const auto end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}

inline std::string trimMultipleSpaces(std::string s)
{
    auto result = s;
    std::string::iterator end = std::unique(result.begin(), result.end(), [](char lhs, char rhs) { return (lhs == rhs) && (lhs == ' '); });
    result.erase(end, result.end());
    return result;
}

inline std::string toLower(std::string s)
{
    auto result = s;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
    return result;
}

inline std::string toUpper(std::string s)
{
    auto result = s;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::toupper(c); });
    return result;
}

template <typename OutIter>
inline void split(const std::string& s, char delimiter, OutIter result)
{
    std::istringstream is(s);
    std::string part;
    while (std::getline(is, part, delimiter)) {
        *result++ = part;
    }
}

inline std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> result;
    split(s, delimiter, std::back_inserter(result));
    return result;
}

template <typename Iter>
std::string join(Iter first, Iter last, const std::string& delimiter)
{
    std::ostringstream result;
    for (Iter i = first; i != last; ++i) {
        if (i != first) {
            result << delimiter;
        }
        result << *i;
    }
    return result.str();
}

inline std::string currentTimeIso8601()
{
    auto now = std::chrono::system_clock::now();
    time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);  // Windows
#else
    gmtime_r(&t, &tm);  // POSIX
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%FT%T%H%M%S");
    return oss.str();
}

struct LoadError
{
    std::error_code ec;
    std::string message;
    fs::path path;
};

using Bytes = std::vector<uint8_t>;

inline std::string to_string(const LoadError& e)
{
    std::string s;
    if (!e.path.empty()) {
        s += e.path.string();
        s += ": ";
    }
    if (!e.message.empty()) {
        s += e.message;
        if (e.ec)
            s += " (";
    }
    if (e.ec) {
        s += e.ec.message();
        if (!e.message.empty())
            s += ")";
    }
    return s;
}

inline ghc::expected<Bytes, LoadError> loadFile(const fs::path& path, size_t maxSize = 16 * 1024 * 1024)
{
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        if (ec) {
            return ghc::unexpected(LoadError{ec, "exists() failed", path});
        }
        return ghc::unexpected(LoadError{std::make_error_code(std::errc::no_such_file_or_directory), "file not found", path});
    }
    auto size = fs::file_size(path, ec);
    if (ec) {
        return ghc::unexpected(LoadError{ec, "file_size() failed", path});
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return ghc::unexpected(LoadError{std::make_error_code(std::errc::permission_denied), "failed to open file", path});
    }
    Bytes buf(size);
    if (size != 0) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        if (!in) {
            return ghc::unexpected(LoadError{std::make_error_code(std::errc::io_error), "failed while reading file", path});
        }
    }

    return buf;
}

inline bool writeFile(const std::string& filename, const char* data, size_t size)
{
    fs::ofstream os(filename, std::ios::binary | std::ios::trunc);
    if (os.write(data, size))
        return true;
    return false;
}

inline bool writeFile(const std::string& filename, const uint8_t* data, size_t size)
{
    return writeFile(filename, (const char*)data, size);
}

inline std::string loadTextFile(const fs::path& file)
{
    fs::ifstream is(file, std::ios::binary | std::ios::ate);
    std::streamsize size = is.tellg();
    is.seekg(0, std::ios::beg);

    std::string result(size, '\0');
    if (is.read(result.data(), size)) {
        return result;
    }

    return {};
}

static inline bool isDigit(char32_t c)
{
    return c >= '0' && c <= '9';
}

static inline bool isHexDigit(char32_t c)
{
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/// Calculate sign of a signed integer
/// @returns  +1, 0, or -1
template <typename T>
    requires std::signed_integral<T>
int sign(T v)
{
    return (v > 0) - (v < 0);
}

static inline uint16_t opcodeFromPattern(const std::string& pattern)
{
    uint16_t opcode = 0;
    for (auto c : pattern) {
        opcode <<= 4;
        if (isHexDigit(c))
            opcode += (c >= '0' && c <= '9') ? c - '0' : std::toupper(c) - 'A' + 10;
    }
    return opcode;
}

static inline uint16_t maskFromPattern(const std::string& pattern)
{
    uint16_t opcode = 0;
    for (auto c : pattern) {
        opcode <<= 4;
        if (isHexDigit(c))
            opcode += 15;
    }
    return opcode;
}

static inline bool comparePattern(const std::string& pattern, const std::string& opcode)
{
    int i;
    for (i = 0; i < 4; ++i) {
        if (isHexDigit(pattern[i]) && std::toupper(pattern[i]) != opcode[i])
            break;
    }
    return i == 4;
}

class byte_range
{
public:
    byte_range()
        : _data(nullptr)
        , _size(0)
    {
    }
    byte_range(uint8_t* data, size_t size)
        : _data(data)
        , _size(size)
    {
    }
    byte_range(uint8_t* data, uint8_t* end)
        : _data(data)
        , _size(end - data)
    {
    }

    bool empty() const { return _size == 0; }
    uint8_t* data() { return _data; }
    const uint8_t* data() const { return _data; }
    size_t size() const { return _size; }

    const uint8_t* begin() const { return _data; }
    const uint8_t* end() const { return _data + _size; }

private:
    uint8_t* _data;
    size_t _size;
};

inline std::string formatUnit(double val, const std::string& suffix, int minScale = -1)
{
    static const char* prefix[] = {"n", "u", "m", "", "k", "M", "G", "T"};
    if (std::isnan(val))
        return "";
    bool isNeg = val < 0;
    val = std::abs(val);
    if (val < 0.000000001)
        return "0" + suffix;
    auto scale = (std::max)(int(std::log10(val) - (val < 10.0 ? 4 : 1)) / 3, minScale);
    if (scale >= -3 && scale <= 4) {
        auto scaledVal = val / std::pow(10.0, scale * 3);
        return (isNeg ? "-" : "") + std::to_string(static_cast<int>(scaledVal + 0.5)) + prefix[scale + 3] + suffix;
    }
    return "<err>";
}

inline Sha1::Digest calculateSha1(std::span<const uint8_t> data)
{
    char hex[SHA1_HEX_SIZE];
    Sha1 sum;
    sum.add(data.data(), data.size());
    sum.finalize();
    return static_cast<Sha1::Digest>(sum);
}

inline Sha1::Digest calculateSha1(const std::string& str)
{
    char hex[SHA1_HEX_SIZE];
    Sha1 sum;
    sum.add(str.data(), str.size());
    sum.finalize();
    return static_cast<Sha1::Digest>(sum);
    ;
}

inline bool fuzzyCompare(std::string_view s1, std::string_view s2)
{
    auto iter1 = s1.begin();
    auto iter2 = s2.begin();
    while (iter1 != s1.end() && iter2 != s2.end()) {
        while (iter1 != s1.end() && !std::isalnum(*iter1))
            ++iter1;
        while (iter2 != s2.end() && !std::isalnum(*iter2))
            ++iter2;
        if (iter1 != s1.end() && iter2 != s2.end()) {
            auto c1 = std::tolower(*iter1++);
            auto c2 = std::tolower(*iter2++);
            if (/*std::tolower(*iter1++) != std::tolower(*iter2++)*/ c1 != c2) {
                return false;
            }
        }
    }
    while (iter1 != s1.end() && !std::isalnum(*iter1))
        ++iter1;
    while (iter2 != s2.end() && !std::isalnum(*iter2))
        ++iter2;
    return iter1 == s1.end() && iter2 == s2.end();
}

inline bool fuzzyAnyOf(std::string_view text, std::initializer_list<std::string_view> alternatives)
{
    for (const auto& alt : alternatives) {
        if (fuzzyCompare(text, alt))
            return true;
    }
    return false;
}

inline std::string toOptionName_old(std::string_view text)
{
    std::string result;
    bool gap = false;
    bool wasLower = false;
    for (char c : text) {
        if (std::isalnum(c)) {
            if (gap || (wasLower && std::isupper(c)))
                gap = false, result.push_back('-');
            wasLower = std::islower(c);
            result.push_back(static_cast<char>(std::tolower(c)));
        }
        else
            gap = true;
    }
    return result;
}

inline std::string toOptionName(std::string_view text)
{
    enum class Category { NONE, LOWER, UPPER, DIGIT };
    std::string result;
    auto lastCategory = Category::NONE;
    auto pushHyphen = [&]() {
        if (lastCategory != Category::NONE && result.back() != '-') {
            result.push_back('-');
        }
    };
    for (char c : text) {
        auto uc = static_cast<unsigned char>(c);
        if (std::isupper(uc)) {
            if (lastCategory != Category::UPPER)
                pushHyphen();
            result.push_back(static_cast<char>(std::tolower(uc)));
            lastCategory = Category::UPPER;
        }
        else if (std::islower(uc)) {
            if (lastCategory != Category::LOWER && lastCategory != Category::UPPER)
                pushHyphen();
            result.push_back(static_cast<char>(uc));
            lastCategory = Category::LOWER;
        }
        else if (std::isdigit(uc)) {
            if (lastCategory != Category::DIGIT)
                pushHyphen();
            result.push_back(c);
            lastCategory = Category::DIGIT;
        }
        else {
            pushHyphen();
        }
    }
    if (!result.empty() && result.back() == '-') {
        result.pop_back();
    }
    return result;
}

inline std::string toJsonKey(std::string_view text)
{
    enum class Category { NONE, LOWER, UPPER, DIGIT, OTHER };
    std::string result;
    auto lastCategory = Category::NONE;
    auto pushUppercase = [&]() {
        if (lastCategory != Category::NONE && result.back() != '-') {
            return true;
        }
        return false;
    };
    for (char c : text) {
        auto uc = static_cast<unsigned char>(c);
        bool pushUpper = false;
        if (std::isupper(uc)) {
            if (lastCategory != Category::UPPER)
                pushUpper = pushUppercase();
            result.push_back(static_cast<char>(pushUpper ? std::toupper(uc) : std::tolower(uc)));
            lastCategory = Category::UPPER;
        }
        else if (std::islower(uc)) {
            if (lastCategory != Category::LOWER && lastCategory != Category::UPPER)
                pushUpper = pushUppercase();
            result.push_back(static_cast<char>(pushUpper ? std::toupper(uc) : std::tolower(uc)));
            lastCategory = Category::LOWER;
        }
        else if (std::isdigit(uc)) {
            result.push_back(c);
            lastCategory = Category::DIGIT;
        }
        else {
            if (lastCategory != Category::NONE)
                lastCategory = Category::OTHER;
        }
    }
    return result;
}

template <size_t N, typename ValueType = uint64_t, typename SumType = uint64_t>
class SMA
{
public:
    void reset()
    {
        _fill = _index = 0;
        _sum = 0;
        _min = (std::numeric_limits<ValueType>::max)();
        _max = (std::numeric_limits<ValueType>::min)();
    }
    void add(ValueType nextVal)
    {
        if (_fill < N)
            ++_fill;
        else
            _sum -= _history[_index];
        _sum += nextVal;
        _history[_index] = nextVal;
        if (++_index == N)
            _index = 0;
        if (nextVal < _min)
            _min = nextVal;
        if (nextVal > _max)
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

class Stopwatch
{
public:
    Stopwatch()
        : _start(std::chrono::steady_clock::now())
    {
    }
    void start() { _start = std::chrono::steady_clock::now(); }
    void stop()
    {
        _lastLap = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - _start).count();
        _sma.add(_lastLap);
    }
    [[nodiscard]] std::string getElapsedLapString() const { return formatDuration(_lastLap); }
    [[nodiscard]] std::string getElapsedAvgString() const { return formatDuration(_sma.get()); }

private:
    template <typename T>
    static std::string formatDuration(T duration)
    {
        if (duration > 1000000) {
            return fmt::format("{:.1f}s", static_cast<double>(duration) / 1000000.0);
        }
        if (duration > 150) {
            return fmt::format("{:.1f}ms", static_cast<double>(duration) / 1000.0);
        }
        return fmt::format("{}us", static_cast<unsigned>(duration));
    }
    std::chrono::steady_clock::time_point _start;
    uint64_t _lastLap{0};
    SMA<120> _sma;
};

template <typename T>
class TripleBuffer
{
public:
    using Generation = std::uint64_t;
    // This constructor TAKES OWNERSHIP of a, b, c and will delete them
    // in the destructor. They must be distinct and non-null.
    TripleBuffer(T* a, T* b, T* c, Generation initialGeneration = 0)
        : _workBuffer(a)
        , _outputBuffer(b)
        , _readyBuffer(c)
        , _generation(initialGeneration)
        , _lastConsumedGeneration(initialGeneration)
    {
        assert(a != nullptr && b != nullptr && c != nullptr);
        assert(a != b && a != c && b != c);
    }

    ~TripleBuffer()
    {
        // We assume no concurrent use during destruction (usual C++ rule).
        T* ready = _readyBuffer.load(std::memory_order_relaxed);

        delete _workBuffer;
        delete _outputBuffer;
        delete ready;
    }

    TripleBuffer(const TripleBuffer&) = delete;
    TripleBuffer& operator=(const TripleBuffer&) = delete;
    TripleBuffer(TripleBuffer&&) = delete;
    TripleBuffer& operator=(TripleBuffer&&) = delete;

    // producer - - - - - - - - - - - - - - - - - - - - - - - -
    T& producerAccess() noexcept { return *_workBuffer; }
    void producerCommit() noexcept
    {
        T* previousReady = _readyBuffer.exchange(_workBuffer, std::memory_order_acq_rel);
        _workBuffer = previousReady;

        _generation.fetch_add(1, std::memory_order_release);
    }

    // consumer - - - - - - - - - - - - - - - - - - - - - - - -
    bool trySwap() noexcept
    {
        Generation current = _generation.load(std::memory_order_acquire);
        if (current == _lastConsumedGeneration) {
            return false;
        }
        T* previousReady = _readyBuffer.exchange(_outputBuffer, std::memory_order_acq_rel);
        _outputBuffer = previousReady;

        _lastConsumedGeneration = current;
        return true;
    }
    const T& consumerAccess() const noexcept { return *_outputBuffer; }
    Generation currentGeneration() const noexcept { return _generation.load(std::memory_order_acquire); }

private:
    T* _workBuffer;
    T* _outputBuffer;
    std::atomic<T*> _readyBuffer;
    std::atomic<Generation> _generation;
    Generation _lastConsumedGeneration;
};

class DataBlockFormatter
{
public:
    explicit DataBlockFormatter(std::function<void(std::string_view)> callback, size_t fields = 16)
        : _callback(callback)
        , _fields(fields)
    {
    }
    ~DataBlockFormatter()
    {
        if (_count % _fields != 0)
            _callback("\n");
    }
    void write(const std::string_view text)
    {
        if (_count % _fields == 0)
            _callback("   ");
        else if (!(_fields & 7) && !(_count & 3))
            _callback(" ");
        _callback(text);
        if (++_count % _fields == 0)
            _callback("\n");
    }

private:
    std::function<void(std::string_view)> _callback{};
    size_t _fields{};
    size_t _count{};
};
