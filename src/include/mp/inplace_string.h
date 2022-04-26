// The MIT License (MIT)
//
// Copyright (c) 2016 Mateusz Pusz
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

#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>

namespace mp {

  namespace detail {
    template<typename... Args>
    using Requires = std::enable_if_t<std::conjunction<Args...>::value, bool>;

    template<size_t Size>
    using impl_size_type_helper =
        std::conditional_t<Size == 1, std::uint8_t, std::conditional_t<Size == 2, std::uint16_t, std::uint32_t>>;
  }

  template<typename CharT, std::size_t MaxSize, typename Traits = std::char_traits<std::decay_t<CharT>>>
  class basic_inplace_string {
    using impl_size_type = ::mp::detail::impl_size_type_helper<sizeof(CharT)>;
    static_assert(MaxSize <= std::numeric_limits<impl_size_type>::max(),
                  "impl_size_type type too small to store MaxSize characters");

  public:
    using traits_type = Traits;
    using value_type = CharT;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;

#if defined(_MSC_VER) && (_ITERATOR_DEBUG_LEVEL != 0)
    using iterator = stdext::checked_array_iterator<value_type*>;
    using const_iterator = stdext::checked_array_iterator<const value_type*>;
#else
    using iterator = value_type*;
    using const_iterator = const value_type*;
#endif
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    static constexpr size_type npos = static_cast<size_type>(-1);

    // constructors
    constexpr basic_inplace_string() noexcept { clear(); }
    basic_inplace_string(const basic_inplace_string&) = default;
    template<std::size_t OtherMaxSize>
    constexpr basic_inplace_string(const basic_inplace_string<CharT, OtherMaxSize, Traits>& str, size_type pos)
        : basic_inplace_string{std::basic_string_view<CharT, Traits>{str}.substr(pos)}
    {
    }
    template<std::size_t OtherMaxSize>
    constexpr basic_inplace_string(const basic_inplace_string<CharT, OtherMaxSize, Traits>& str, size_type pos,
                                   size_type n)
        : basic_inplace_string{std::basic_string_view<CharT, Traits>{str}.substr(pos, n)}
    {
    }
    template<
        class T,
        detail::Requires<std::is_convertible<const T&, std::basic_string_view<CharT, Traits>>> = true>
    constexpr basic_inplace_string(const T& t, size_type pos, size_type n)
        : basic_inplace_string{std::basic_string_view<CharT, Traits>{t}.substr(pos, n)}
    {
    }
    constexpr explicit basic_inplace_string(std::basic_string_view<CharT, Traits> sv)
        : basic_inplace_string{sv.data(), sv.size()}
    {
    }
    constexpr basic_inplace_string(const_pointer s, size_type count) noexcept { assign(s, count); }
    constexpr basic_inplace_string(const_pointer s) noexcept : basic_inplace_string{s, traits_type::length(s)} {}
    constexpr basic_inplace_string(size_type n, value_type c) { assign(n, c); }
    template<class InputIterator>
    constexpr basic_inplace_string(InputIterator begin, InputIterator end)
    {
      assign(begin, end);
    }
    constexpr basic_inplace_string(std::initializer_list<CharT> ilist) { assign(ilist.begin(), ilist.size()); }

    // assignment
    basic_inplace_string& operator=(const basic_inplace_string&) = default;
    constexpr basic_inplace_string& operator=(std::basic_string_view<CharT, Traits> sv)
    {
      return assign(sv);
    }
    constexpr basic_inplace_string& operator=(const_pointer s) noexcept { return assign(s); }
    constexpr basic_inplace_string& operator=(value_type c) noexcept { return assign(1, c); }
    constexpr basic_inplace_string& operator=(std::initializer_list<CharT> ilist)
    {
      return assign(ilist.begin(), ilist.size());
    }

// iterators
#if defined(_MSC_VER) && (_ITERATOR_DEBUG_LEVEL != 0)
    constexpr iterator begin() { return iterator{data(), size()}; }
    constexpr const_iterator begin() const { return const_iterator{data(), size()}; }
#else
    constexpr iterator begin() { return data(); }
    constexpr const_iterator begin() const { return data(); }
#endif
    constexpr iterator end() { return begin() + size(); }
    constexpr const_iterator end() const { return begin() + size(); }

    constexpr reverse_iterator rbegin() { return reverse_iterator{end()}; }
    constexpr const_reverse_iterator rbegin() const { return const_reverse_iterator{end()}; }
    constexpr reverse_iterator rend() { return reverse_iterator(begin()); }
    constexpr const_reverse_iterator rend() const { return const_reverse_iterator{begin()}; }

#if defined(_MSC_VER) && (_ITERATOR_DEBUG_LEVEL != 0)
    constexpr const_iterator cbegin() const { return const_iterator{data(), size()}; }
#else
    constexpr const_iterator cbegin() const { return data(); }
#endif
    constexpr const_iterator cend() const { return begin() + size(); }
    constexpr const_reverse_iterator crbegin() const { return const_reverse_iterator{cend()}; }
    constexpr const_reverse_iterator crend() const { return const_reverse_iterator{cbegin()}; }

    // capacity
    constexpr size_type size() const { return max_size() - static_cast<impl_size_type>(chars_.back()); }
    constexpr size_type length() const { return size(); }
    constexpr size_type max_size() const { return MaxSize; }
    constexpr void resize(size_type n, value_type c)
    {
      const auto sz = size();
      size(n);
      if(n > sz)
        traits_type::assign(data() + sz, n - sz, c);
    }
    constexpr void resize(size_type n) { resize(n, value_type{}); }
    constexpr void clear() { size(0); }
    constexpr bool empty() const { return size() == 0; }

    // element access
    constexpr const_reference operator[](size_type pos) const { return *(begin() + pos); }
    constexpr reference operator[](size_type pos) { return *(begin() + pos); }
    constexpr const_reference at(size_type pos) const
    {
      if(pos >= size()) throw std::out_of_range("inplace_string::at: 'pos' out of range");
      return (*this)[pos];
    }
    constexpr reference at(size_type pos)
    {
      if(pos >= size()) throw std::out_of_range("inplace_string::at: 'pos' out of range");
      return (*this)[pos];
    }
    constexpr reference front() { return (*this)[0]; }
    constexpr const_reference front() const { return (*this)[0]; }
    constexpr reference back() { return (*this)[size() - 1]; }
    constexpr const_reference back() const { return (*this)[size() - 1]; }

    // modifiers
    template<std::size_t OtherMaxSize>
    basic_inplace_string& operator+=(const basic_inplace_string<CharT, OtherMaxSize, Traits>& str)
    {
      return append(str);
    }
    basic_inplace_string& operator+=(std::basic_string_view<CharT, Traits> sv) { return append(sv); }
    basic_inplace_string& operator+=(const_pointer s) { return append(s); }
    basic_inplace_string& operator+=(value_type c)
    {
      push_back(c);
      return *this;
    }
    basic_inplace_string& operator+=(std::initializer_list<CharT> il) { return append(il); }

    template<std::size_t OtherMaxSize>
    basic_inplace_string& append(const basic_inplace_string<CharT, OtherMaxSize, Traits>& str) { return append(str.data(), str.size()); }
    template<std::size_t OtherMaxSize>
    basic_inplace_string& append(const basic_inplace_string<CharT, OtherMaxSize, Traits>& str, size_type pos,
                                 size_type n = npos)
    {
      return append(std::basic_string_view<CharT, Traits>{str}.substr(pos, n));
    }
    basic_inplace_string& append(std::basic_string_view<CharT, Traits> sv) { return append(sv.data(), sv.size()); }
    template<class T,
             detail::Requires<std::is_convertible<const T&, std::basic_string_view<CharT, Traits>>,
                              std::negation<std::is_convertible<const T&, const CharT*>>> = true>
    basic_inplace_string& append(const T& t, size_type pos, size_type n = npos) {
      return append(std::basic_string_view<CharT, Traits>{t}.substr(pos, n));
    }
    basic_inplace_string& append(const_pointer s, size_type n)
    {
      const auto sz = size();
      size(sz + n);
      traits_type::copy(data() + sz, s, n);
      return *this;
    }
    basic_inplace_string& append(const_pointer s) { return append(s, traits_type::length(s)); }
    basic_inplace_string& append(size_type n, value_type c) { resize(size() + n, c); return *this; }
    template<class InputIterator>
    basic_inplace_string& append(InputIterator first, InputIterator last)
    {
      const auto sz = size();
      const auto count = std::distance(first, last);
      size(sz + count);
      traits_type::copy(data() + sz, first, count);
      return *this;
    }
    basic_inplace_string& append(std::initializer_list<CharT> il) { return append(il.begin(), il.end()); }
    void push_back(value_type c) { append(static_cast<size_type>(1), c); }

    template<std::size_t OtherMaxSize>
    constexpr basic_inplace_string& assign(const basic_inplace_string<CharT, OtherMaxSize, Traits>& str)
    {
      return assign(str.data(), str.size());
    }
    template<std::size_t OtherMaxSize>
    constexpr basic_inplace_string& assign(const basic_inplace_string<CharT, OtherMaxSize, Traits>& str, size_type pos,
                                           size_type count = npos)
    {
      return assign(std::basic_string_view<CharT, Traits>{str}.substr(pos, count));
    }
    constexpr basic_inplace_string& assign(std::basic_string_view<CharT, Traits> sv)
    {
      return assign(sv.data(), sv.size());
    }
    template<class T,
             detail::Requires<std::is_convertible<const T&, std::basic_string_view<CharT, Traits>>,
                              std::negation<std::is_convertible<const T&, const CharT*>>> = true>
    constexpr basic_inplace_string& assign(const T& t, size_type pos, size_type count = npos)
    {
      return assign(std::basic_string_view<CharT, Traits>{t}.substr(pos, count));
    }
    constexpr basic_inplace_string& assign(const_pointer s, size_type count) noexcept
    {
      assert(count <= MaxSize);
      size(count);
      traits_type::copy(data(), s, count);
      return *this;
    }
    constexpr basic_inplace_string& assign(const_pointer s) noexcept { return assign(s, traits_type::length(s)); };
    constexpr basic_inplace_string& assign(std::initializer_list<CharT> ilist)
    {
      return assign(ilist.begin(), ilist.size());
    }
    constexpr basic_inplace_string& assign(size_type count, CharT ch)
    {
      assert(count < npos);
      assert(count <= MaxSize);
      size(count);
      traits_type::assign(data(), count, ch);
      return *this;
    }
    template<class InputIt, detail::Requires<std::negation<std::is_integral<InputIt>>> = true>
    constexpr basic_inplace_string& assign(InputIt first, InputIt last)
    {
      size(std::distance(first, last));
      traits_type::copy(data(), first, size());
      return *this;
    }
    template<class InputIt, detail::Requires<std::is_integral<InputIt>> = true>
    constexpr basic_inplace_string& assign(InputIt first, InputIt last)
    {
      return assign(static_cast<size_type>(first), static_cast<value_type>(last));
    }

    // string operations
    constexpr const_pointer c_str() const { return data(); }
    constexpr pointer data() { return chars_.data(); }
    constexpr const_pointer data() const { return chars_.data(); }
    constexpr operator std::basic_string_view<CharT, Traits>() const noexcept
    {
      return {data(), size()};
    }

    // modifiers
    constexpr void swap(basic_inplace_string& other) { std::swap(chars_, other.chars_); }

    constexpr void size(size_type s)
    {
      if(s > max_size()) throw std::length_error("mp::basic_inplace_string: size() > max_size()");
      chars_[s] = '\0';
      chars_.back() = static_cast<impl_size_type>(max_size() - s);
    }

  private:
    std::array<value_type, MaxSize + 1> chars_;  // size is stored as max_size() - size() on the last byte    
  };

  // relational operators
  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator==(const basic_inplace_string<CharT, MaxSize, Traits>& lhs,
                            const basic_inplace_string<CharT, MaxSize, Traits>& rhs)
  {
    return std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs), std::end(rhs));
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator!=(const basic_inplace_string<CharT, MaxSize, Traits>& lhs,
                            const basic_inplace_string<CharT, MaxSize, Traits>& rhs)
  {
    return !(lhs == rhs);
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator<(const basic_inplace_string<CharT, MaxSize, Traits>& lhs,
                           const basic_inplace_string<CharT, MaxSize, Traits>& rhs)
  {
    return std::lexicographical_compare(std::begin(lhs), std::end(lhs), std::begin(rhs), std::end(rhs));
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator<=(const basic_inplace_string<CharT, MaxSize, Traits>& lhs,
                            const basic_inplace_string<CharT, MaxSize, Traits>& rhs)
  {
    return !(rhs < lhs);
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator>(const basic_inplace_string<CharT, MaxSize, Traits>& lhs,
                           const basic_inplace_string<CharT, MaxSize, Traits>& rhs)
  {
    return rhs < lhs;
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator>=(const basic_inplace_string<CharT, MaxSize, Traits>& lhs,
                            const basic_inplace_string<CharT, MaxSize, Traits>& rhs)
  {
    return !(lhs < rhs);
  }

  // comparison with c-style string
  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator==(const CharT* lhs, const basic_inplace_string<CharT, MaxSize, Traits>& rhs)
  {
    return std::equal(lhs, lhs + Traits::length(lhs), std::begin(rhs), std::end(rhs));
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator==(const basic_inplace_string<CharT, MaxSize, Traits>& lhs, const CharT* rhs)
  {
    return std::equal(std::begin(lhs), std::end(lhs), rhs, rhs + Traits::length(rhs));
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator!=(const CharT* lhs, const basic_inplace_string<CharT, MaxSize, Traits>& rhs)
  {
    return !(lhs == rhs);
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator!=(const basic_inplace_string<CharT, MaxSize, Traits>& lhs, const CharT* rhs)
  {
    return !(lhs == rhs);
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator<(const CharT* lhs, const basic_inplace_string<CharT, MaxSize, Traits>& rhs)
  {
    return std::lexicographical_compare(lhs, lhs + Traits::length(lhs), std::begin(rhs), std::end(rhs));
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator<(const basic_inplace_string<CharT, MaxSize, Traits>& lhs, const CharT* rhs)
  {
    return std::lexicographical_compare(std::begin(lhs), std::end(lhs), rhs, rhs + Traits::length(rhs));
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator<=(const CharT* lhs, const basic_inplace_string<CharT, MaxSize, Traits>& rhs)
  {
    return !(rhs < lhs);
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator<=(const basic_inplace_string<CharT, MaxSize, Traits>& lhs, const CharT* rhs)
  {
    return !(rhs < lhs);
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator>(const CharT* lhs, const basic_inplace_string<CharT, MaxSize, Traits>& rhs)
  {
    return rhs < lhs;
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator>(const basic_inplace_string<CharT, MaxSize, Traits>& lhs, const CharT* rhs)
  {
    return rhs < lhs;
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator>=(const CharT* lhs, const basic_inplace_string<CharT, MaxSize, Traits>& rhs)
  {
    return !(lhs < rhs);
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  constexpr bool operator>=(const basic_inplace_string<CharT, MaxSize, Traits>& lhs, const CharT* rhs)
  {
    return !(lhs < rhs);
  }

  // input/output
  template<typename CharT, std::size_t MaxSize, class Traits>
  inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                                       const basic_inplace_string<CharT, MaxSize, Traits>& v)
  {
    return os << v.data();
  }

  // conversions
  template<typename CharT, std::size_t MaxSize, class Traits>
  inline std::basic_string<CharT, Traits> to_string(const basic_inplace_string<CharT, MaxSize, Traits>& v)
  {
    return {v.data(), v.size()};
  }

  template<typename CharT, std::size_t MaxSize, class Traits>
  inline std::basic_string_view<CharT, Traits> to_sv(const basic_inplace_string<CharT, MaxSize, Traits>& v)
  {
    return {v.data(), v.size()};
  }

  // aliases
  template<std::size_t MaxSize>
  using inplace_string = basic_inplace_string<char, MaxSize>;
  template<std::size_t MaxSize>
  using inplace_wstring = basic_inplace_string<wchar_t, MaxSize>;
  //  template<std::size_t MaxSize>
  //  using inplace_u16string = basic_inplace_string<char16_t, MaxSize>;
  //  template<std::size_t MaxSize>
  //  using inplace_u32string = basic_inplace_string<char32_t, MaxSize>;
}
