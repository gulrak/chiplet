//---------------------------------------------------------------------------------------
//
// ghc::span - A minimal C++20-like <span> implementation for C++17
//
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2024, Steffen Schümann <s.schuemann@pobox.com>
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
// WARNING: This implementation is pretty straightforward but not extensively
//          tested, there are more stable ones out there! You've been warned.
//---------------------------------------------------------------------------------------
#pragma once

#if !defined(USE_GHC_SPAN) && __cplusplus >= 202002L && defined __has_include
#  if __has_include (<span>)
#define USE_STD_SPAN
#    include <span>
namespace ghc {
using std::span;
using std::dynamic_extent;
}
#else
#include <array>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace ghc {

// constants
inline constexpr std::size_t dynamic_extent = std::numeric_limits<std::size_t>::max();

// 22.7.3, class template span
template <class ElementType, size_t Extent = dynamic_extent>
class span;

// template<class ElementType, size_t Extent>
// inline constexpr bool ranges::enable_view<span<ElementType, Extent>> = Extent == 0 || Extent == dynamic_extent;
// template<class ElementType, size_t Extent>
// inline constexpr bool ranges::enable_borrowed_range<span<ElementType, Extent>> = true;
//  22.7.3.8, views of object representation
template <class ElementType, size_t Extent>
constexpr span<const std::byte, Extent == dynamic_extent ? dynamic_extent : sizeof(ElementType) * Extent> as_bytes(span<ElementType, Extent> s) noexcept;
template <class ElementType, size_t Extent>
constexpr span<std::byte, Extent == dynamic_extent ? dynamic_extent : sizeof(ElementType) * Extent> as_writable_bytes(span<ElementType, Extent> s) noexcept;

template <class ElementType, size_t Extent /*= dynamic_extent*/>
class span
{
public:
    template <class T>
    struct type_identity
    {
        using type = T;
    };

    // constants and types
    using element_type = ElementType;
    using value_type = std::remove_cv_t<ElementType>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = element_type*;
    using const_pointer = const element_type*;
    using reference = element_type&;
    using const_reference = const element_type&;
    using iterator = ElementType*;  // see 22.7.3.7
    using reverse_iterator = std::reverse_iterator<iterator>;
    static constexpr size_type extent = Extent;

    // 22.7.3.2, constructors, copy, and assignment
    constexpr span() noexcept
        : _size(Extent == dynamic_extent ? 0 : extent)
    {
    }
    template <class It>
    constexpr explicit span(It first, size_type count)
        : _data(&(*first))
        , _size(count)
    {
    }
    template <class It, class End>
    constexpr explicit span(It first, End last)
        : _data(&(*first))
        , _size(last - first)
    {
    }
    template <size_t N>
    constexpr span(typename type_identity<element_type>::type (&arr)[N]) noexcept
        : _data(arr)
        , _size(N)
    {
        static_assert(Extent == N || Extent == dynamic_extent);
    }
    template <class T, size_t N>
    constexpr span(std::array<T, N>& arr) noexcept
        : _data(arr.data())
        , _size(N)
    {
        static_assert(Extent == N || Extent == dynamic_extent);
    }
    template <class T, size_t N>
    constexpr span(const std::array<T, N>& arr) noexcept
        : _data(arr.data())
        , _size(N)
    {
        static_assert(Extent == N || Extent == dynamic_extent);
    }
    template <class R>
    constexpr span(R&& r)
        : _data(r.data())
        , _size(r.size())
    {
        static_assert(Extent == dynamic_extent);
    }
    constexpr span(const span& other) noexcept = default;
    // template<class OtherElementType, size_t OtherExtent>
    // constexpr explicit(see below ) span(const span<OtherElementType, OtherExtent>& s) noexcept;
    ~span() noexcept = default;
    constexpr span& operator=(const span& other) noexcept = default;
    // 22.7.3.4, subviews
    template <size_t Count>
    constexpr span<element_type, Count> first() const
    {
        static_assert(Extent != dynamic_extent && Count <= Extent);
        return {_data, _data + Count};
    }
    template <size_t Count>
    constexpr span<element_type, Count> last() const
    {
        static_assert(Extent != dynamic_extent && Count <= Extent);
        return {_data + _size - Count, _data + _size};
    }
    template <size_t Offset, size_t Count = dynamic_extent>
    constexpr span<element_type, Count != dynamic_extent ? Count : (Extent != dynamic_extent ? Extent - Offset : dynamic_extent)> subspan() const
    {
        static_assert(true);
        return {_data + Offset, Count == dynamic_extent ? Extent - Offset : Count};
    }
    constexpr span<element_type, dynamic_extent> first(size_type count) const { return {_data, _data + count}; }
    constexpr span<element_type, dynamic_extent> last(size_type count) const { return {_data + _size - count, _data + _size}; }
    constexpr span<element_type, dynamic_extent> subspan(size_type offset, size_type count = dynamic_extent) const { return {_data + offset, count == dynamic_extent ? _size - offset : count}; }
    // 22.7.3.5, observers
    constexpr size_type size() const noexcept
    {
        if constexpr (Extent == dynamic_extent)
            return _size;
        else
            return extent;
    }
    constexpr size_type size_bytes() const noexcept { return size() * sizeof(ElementType); }
    [[nodiscard]] constexpr bool empty() const noexcept { return Extent == dynamic_extent ? _size == 0 : extent == 0; }
    // 22.7.3.6, element access
    constexpr reference operator[](size_type idx) const
    {
        static_assert(Extent == dynamic_extent || idx < extent);
        return _data[idx];
    }
    constexpr reference front() const
    {
        static_assert(Extent == dynamic_extent || Extent > 0);
        return *_data;
    }
    constexpr reference back() const
    {
        static_assert(Extent == dynamic_extent || Extent > 0);
        return *(_data + _size - 1);
    }
    constexpr pointer data() const noexcept { return _data; }
    // 22.7.3.7, iterator support
    constexpr iterator begin() const noexcept { return _data; }
    constexpr iterator end() const noexcept { return _data + _size; }
    constexpr reverse_iterator rbegin() const noexcept { return std::reverse_iterator<iterator>(_data + _size); }
    constexpr reverse_iterator rend() const noexcept { return std::reverse_iterator<iterator>(_data); }

private:
    pointer _data{};
    size_type _size{};
};

//---------------------------------------------------------------------------------------
// IMPLEMENTATION
//---------------------------------------------------------------------------------------

template <class ElementType, size_t Extent>
constexpr span<const std::byte, Extent == dynamic_extent ? dynamic_extent : sizeof(ElementType) * Extent> as_bytes(span<ElementType, Extent> s) noexcept
{
    return {reinterpret_cast<const std::byte*>(s.data()), s.size_bytes()};
}

template <class ElementType, size_t Extent>
constexpr span<std::byte, Extent == dynamic_extent ? dynamic_extent : sizeof(ElementType) * Extent> as_writable_bytes(span<ElementType, Extent> s) noexcept
{
    return {reinterpret_cast<std::byte*>(s.data()), s.size_bytes()};
}

}  // namespace ghc

#  endif
#endif
