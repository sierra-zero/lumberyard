/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#pragma once

#include <AzCore/Math/MathUtils.h>
#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/std/iterator.h>
#include <AzCore/std/hash.h>


namespace AZStd
{
    namespace StringInternal
    {
        template<class CharT, class SizeT, class Traits, SizeT npos>
        constexpr SizeT char_find(const CharT* s, size_t count, CharT ch) noexcept
        {
            const CharT* foundIter = Traits::find(s, count, ch);
            return foundIter ? AZStd::distance(s, foundIter) : npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        constexpr SizeT find(const CharT* data, SizeT size, const CharT* ptr, SizeT offset, SizeT count) noexcept
        {
            if (offset > size || count > size)
            {
                return npos;
            }
            if (count == 0)
            {
                return offset; // count == 0 means that highest index should be returned
            }

            size_t searchRange = size - offset;
            for (size_t searchIndex = offset; searchIndex < size;)
            {
                size_t charFindIndex = char_find<CharT, SizeT, Traits, npos>(&data[searchIndex], searchRange, *ptr);
                if (charFindIndex == npos)
                {
                    return npos;
                }
                size_t foundIndex = searchIndex + charFindIndex;
                if (Traits::compare(&data[foundIndex], ptr, count) == 0)
                {
                    return foundIndex;
                }

                searchRange = size - (foundIndex + 1);
                searchIndex = foundIndex + 1;
            }

            return npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        constexpr SizeT find(const CharT* data, SizeT size, CharT c, SizeT offset) noexcept
        {
            if (offset > size)
            {
                return npos;
            }

            size_t charFindIndex = char_find<CharT, SizeT, Traits, npos>(&data[offset], size - offset, c);
            return charFindIndex != npos ? offset + charFindIndex : npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        constexpr SizeT rfind(const CharT* data, SizeT size, const CharT* ptr, SizeT offset, SizeT count) noexcept
        {
            if (size == 0 || count > size)
            {
                return npos;
            }

            if (count == 0)
            {
                return offset > size ? size : offset; // count == 0 means that highest index should be returned
            }

            // Add one to offset so that for loop condition can check against 0 as the breakout condition
            size_t lastIndex = AZ::GetMin(offset, size - count) + 1;

            for (; lastIndex; --lastIndex)
            {
                if (Traits::eq(*ptr, data[lastIndex - 1]) && Traits::compare(ptr, &data[lastIndex - 1], count) == 0)
                {
                    return lastIndex - 1;
                }
            }

            return npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        constexpr SizeT rfind(const CharT* data, SizeT size, CharT c, SizeT offset) noexcept
        {
            if (size == 0)
            {
                return npos;
            }

            // Add one to offset so that for loop condition can check against 0 as the breakout condition
            size_t lastIndex = offset > size ? size : offset + 1;

            for (; lastIndex; --lastIndex)
            {
                if (Traits::eq(c, data[lastIndex - 1]))
                {
                    return lastIndex - 1;
                }
            }

            return npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        constexpr SizeT find_first_of(const CharT* data, SizeT size, const CharT* ptr, SizeT offset, SizeT count) noexcept
        {
            if (size == 0)
            {
                return npos;
            }

            if (count == 0)
            {
                return npos; // count == 0 means that the set of valid entries is empty
            }

            for (size_t firstIndex = offset; firstIndex < size; ++firstIndex)
            {
                size_t charFindIndex = char_find<CharT, SizeT, Traits, npos>(ptr, count, data[firstIndex]);
                if (charFindIndex != npos)
                {
                    return firstIndex;
                }
            }
            return npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        constexpr SizeT find_last_of(const CharT* data, SizeT size, const CharT* ptr, SizeT offset, SizeT count) noexcept
        {
            if (size == 0)
            {
                return npos;
            }

            if (count == 0)
            {
                return npos; // count == 0 means that the set of valid entries is empty
            }

            // Add one to offset so that for loop condition can against 0 as the breakout condition
            size_t lastIndex = offset > size ? size : offset + 1;
            for (; lastIndex; --lastIndex)
            {
                size_t charFindIndex = char_find<CharT, SizeT, Traits, npos>(ptr, count, data[lastIndex - 1]);
                if (charFindIndex != npos)
                {
                    return lastIndex - 1;
                }
            }

            return npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        constexpr SizeT find_first_not_of(const CharT* data, SizeT size, const CharT* ptr, SizeT offset, SizeT count) noexcept
        {
            if (size == 0)
            {
                return npos;
            }

            if (count == 0)
            {
                return offset; // count == 0 means that the every character is part of the set of valid entries
            }

            for (size_t firstIndex = offset; firstIndex < size; ++firstIndex)
            {
                size_t charFindIndex = char_find<CharT, SizeT, Traits, npos>(ptr, count, data[firstIndex]);
                if (charFindIndex == npos)
                {
                    return firstIndex;
                }
            }
            return npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        constexpr SizeT find_first_not_of(const CharT* data, SizeT size, CharT c, SizeT offset) noexcept
        {
            if (size == 0)
            {
                return npos;
            }

            for (size_t firstIndex = offset; firstIndex < size; ++firstIndex)
            {
                if (!Traits::eq(c, data[firstIndex]))
                {
                    return firstIndex;
                }
            }
            return npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        constexpr SizeT find_last_not_of(const CharT* data, SizeT size, const CharT* ptr, SizeT offset, SizeT count) noexcept
        {
            if (size == 0)
            {
                return npos;
            }

            // Add one to offset so that for loop condition can check against 0 as the breakout condition
            size_t lastIndex = offset > size ? size : offset + 1;
            if (count == 0)
            {
                return lastIndex - 1; // count == 0 means that the every character is part of the set of valid entries
            }

            for (; lastIndex; --lastIndex)
            {
                size_t charFindIndex = char_find<CharT, SizeT, Traits, npos>(ptr, count, data[lastIndex - 1]);
                if (charFindIndex == npos)
                {
                    return lastIndex - 1;
                }
            }

            return npos;
        }

        template<class CharT, class SizeT, class Traits, SizeT npos>
        constexpr SizeT find_last_not_of(const CharT* data, SizeT size, CharT c, SizeT offset) noexcept
        {
            if (size == 0)
            {
                return npos;
            }

            // Add one to offset so that for loop condition can against 0 as the breakout condition
            size_t lastIndex = offset > size ? size : offset + 1;
            for (; lastIndex; --lastIndex)
            {
                if (!Traits::eq(c, data[lastIndex - 1]))
                {
                    return lastIndex - 1;
                }
            }
            return npos;
        }
    }

    template<class Element>
    struct char_traits
    {
        // properties of a string or stream char_type element
        using char_type = Element;
        using int_type = AZStd::conditional_t<AZStd::is_same_v<Element, char>, int,
            AZStd::conditional_t<AZStd::is_same_v<Element, wchar_t>, wint_t, int>
        >;
        using pos_type = std::streampos;
        using off_type = std::streamoff;
        using state_type = mbstate_t;

        static constexpr void assign(char_type& left, const char_type& right) noexcept { left = right; }
        static char_type* assign(char_type* dest, size_t count, char_type ch) noexcept
        {
            AZ_Assert(dest, "Invalid input!");
            if constexpr (AZStd::is_same_v<char_type, char>)
            {
                return static_cast<char_type*>(::memset(dest, ch, count));
            }
            else if constexpr (AZStd::is_same_v<char_type, wchar_t>)
            {
                return static_cast<char_type*>(::wmemset(dest, ch, count));
            }
            else
            {
                for (char* iter = dest; count; --count, ++iter)
                {
                    assign(*iter, ch);
                }
                return dest;
            }
        }
        static constexpr bool eq(char_type left, char_type right) noexcept { return left == right; }
        static constexpr bool lt(char_type left, char_type right) noexcept { return left < right; }
        static constexpr int compare(const char_type* s1, const char_type* s2, size_t count) noexcept
        {
            // Regression in VS2017 15.8 and 15.9 where __builtin_memcmp fails in valid checks in constexpr evaluation
#if !defined(AZ_COMPILER_MSVC) || AZ_COMPILER_MSVC < 1915 || AZ_COMPILER_MSVC > 1916
            if constexpr (AZStd::is_same_v<char_type, char>)
            {
                return __builtin_memcmp(s1, s2, count);
            }
            else if constexpr (AZStd::is_same_v<char_type, wchar_t>)
            {
                return __builtin_wmemcmp(s1, s2, count);
            }
            else
#endif
            {
                for (; count; --count, ++s1, ++s2)
                {
                    if (lt(*s1, *s2))
                    {
                        return -1;
                    }
                    else if (lt(*s2, *s1))
                    {
                        return 1;
                    }
                }
                return 0;
            }
        }
        static constexpr size_t length(const char_type* s) noexcept
        {
            if constexpr (AZStd::is_same_v<char_type, char>)
            {
                return __builtin_strlen(s);
            }
            else if constexpr (AZStd::is_same_v<char_type, wchar_t>)
            {
                return __builtin_wcslen(s);
            }
            else
            {
                size_t strLength{};
                for (; *s; ++s, ++strLength)
                {
                    ;
                }
                return strLength;
            }
        }
        static constexpr const char_type* find(const char_type* s, size_t count, const char_type& ch) noexcept
        {
                // There is a bug with the __builtin_char_memchr intrinsic in Visual Studio 2017 15.8.x and 15.9.x
                // It reads in one more additional character than the value of count.
                // This is probably due to assuming null-termination
#if !defined(AZ_COMPILER_MSVC) || AZ_COMPILER_MSVC < 1915 || AZ_COMPILER_MSVC > 1916
            if constexpr (AZStd::is_same_v<char_type, char>)
            {
                return __builtin_char_memchr(s, ch, count);
            }
            else if constexpr (AZStd::is_same_v<char_type, wchar_t>)
            {
                return __builtin_wmemchr(s, ch, count);

            }
            else
#endif
            {
                for (; count; --count, ++s)
                {
                    if (eq(*s, ch))
                    {
                        return s;
                    }
                }
                return nullptr;
            }
        }
        static char_type* move(char_type* dest, const char_type* src, size_t count) noexcept
        {
            AZ_Assert(dest != nullptr && src != nullptr, "Invalid input!");
            return static_cast<char_type*>(::memmove(dest, src, count * sizeof(char_type)));
        }
        static char_type* copy(char_type* dest, const char_type* src, size_t count)
        {
            AZ_Assert(dest != nullptr && src != nullptr, "Invalid input!");
            AZ_Assert(src < dest || src >= dest + count, "Copying from an memory range where the src overlays the dest is undefined behavior and would modify the src string");
            return static_cast<char_type*>(::memcpy(dest, src, count * sizeof(char_type)));
        }
        static constexpr char_type to_char_type(int_type c) noexcept { return static_cast<char_type>(c); }
        static constexpr int_type to_int_type(char_type c) noexcept
        {
            if constexpr (AZStd::is_same_v<char_type, char>)
            {
                return static_cast<int_type>(static_cast<unsigned char>(c));
            }
            else
            {
                return static_cast<int_type>(c);
            }
        }
        static constexpr bool eq_int_type(int_type left, int_type right) noexcept { return left == right; }
        static constexpr int_type eof() noexcept { return static_cast<int_type>(-1); }
        static constexpr int_type not_eof(int_type c) noexcept { return c != eof() ? c : !eof(); }
    };

    /**
     * Immutable string wrapper based on boost::const_string and std::string_view. When we operate on
     * const char* we don't know if this points to NULL terminated string or just a char array.
     * to have a clear distinction between them we provide this wrapper.
     */
    template <class Element, class Traits = AZStd::char_traits<Element>>
    class basic_string_view
    {
    public:
        using traits_type = Traits;
        using value_type = Element;

        using pointer = value_type*;
        using const_pointer = const value_type*;

        using reference = value_type&;
        using const_reference = const value_type&;

        using size_type = size_t;
        using difference_type = ptrdiff_t;

        static const size_type npos = size_type(-1);

        using iterator = const value_type*;
        using const_iterator = const value_type*;
        using reverse_iterator = AZStd::reverse_iterator<iterator>;
        using const_reverse_iterator = AZStd::reverse_iterator<const_iterator>;

        constexpr basic_string_view() noexcept
            : m_begin(nullptr)
            , m_end(nullptr)
        { }

        constexpr basic_string_view(const_pointer s)
            : m_begin(s)
            , m_end(s ? s + traits_type::length(s) : nullptr)
        { }

        constexpr basic_string_view(const_pointer s, size_type count)
            : m_begin(s)
            , m_end(m_begin + count)
        {}

        constexpr basic_string_view(const_pointer first, const_pointer last)
            : m_begin(first)
            , m_end(last)
        { }

        constexpr basic_string_view(const basic_string_view&) noexcept = default;
        constexpr basic_string_view(basic_string_view&& other)
            : basic_string_view()
        {
            swap(other);
        }

        constexpr const_reference operator[](size_type index) const { return m_begin[index]; }
        /// Returns value, not reference. If index is out of bounds, 0 is returned (can't be reference).
        constexpr value_type at(size_type index) const
        {
            AZ_Assert(index < size(), "pos value is out of range");
            return index >= size()
                ? value_type(0)
                : m_begin[index];
        }

        constexpr const_reference front() const
        {
            AZ_Assert(!empty(), "string_view::front(): string is empty");
            return m_begin[0];
        }

        constexpr const_reference back() const
        {
            AZ_Assert(!empty(), "string_view::back(): string is empty");
            return m_end[-1];
        }

        constexpr const_pointer data() const { return m_begin; }

        constexpr size_type length() const { return m_end - m_begin; }
        constexpr size_type size() const { return m_end - m_begin; }
        constexpr size_type max_size() const { return (std::numeric_limits<size_type>::max)(); } //< Wrapping the numeric_limits<size_type>::max function in parenthesis to get around the issue with windows.h defining max as a macro
        constexpr bool      empty() const { return m_end == m_begin; }

        constexpr void rshorten(size_type shift = 1) { m_end -= shift; if (m_end <= m_begin) erase(); }
        constexpr void lshorten(size_type shift = 1) { m_begin += shift; if (m_end <= m_begin) erase(); }
        constexpr void remove_prefix(size_type n)
        {
            AZ_Assert(n <= size(), "Attempting to remove prefix larger than string size");
            lshorten(n);
        }

        constexpr void remove_suffix(size_type n)
        {
            AZ_Assert(n <= size(), "Attempting to remove suffix larger than string size");
            rshorten(n);
        }

        constexpr basic_string_view& operator=(const basic_string_view& s) noexcept = default;

        constexpr void swap(basic_string_view& s) noexcept
        {
            const_pointer tmp1 = m_begin;
            const_pointer tmp2 = m_end;
            m_begin = s.m_begin;
            m_end = s.m_end;
            s.m_begin = tmp1;
            s.m_end = tmp2;
        }
        friend bool constexpr operator==(basic_string_view s1, basic_string_view s2) noexcept
        {
            return s1.length() == s2.length() && (s1.length() == 0 || traits_type::compare(s1.m_begin, s2.m_begin, s1.length()) == 0);
        }

        friend constexpr bool operator!=(basic_string_view s1, basic_string_view s2) noexcept { return !(s1 == s2); }
        friend constexpr bool operator<(basic_string_view lhs, basic_string_view rhs) noexcept { return lhs.compare(rhs) < 0; }
        friend constexpr bool operator>(basic_string_view lhs, basic_string_view rhs) noexcept { return lhs.compare(rhs) > 0; }
        friend constexpr bool operator<=(basic_string_view lhs, basic_string_view rhs) noexcept { return lhs.compare(rhs) <= 0; }
        friend constexpr bool operator>=(basic_string_view lhs, basic_string_view rhs) noexcept { return lhs.compare(rhs) >= 0; }

        constexpr iterator         begin() const noexcept { return m_begin; }
        constexpr iterator         end() const noexcept { return m_end; }
        constexpr const_iterator   cbegin() const noexcept { return m_begin; }
        constexpr const_iterator   cend() const noexcept { return m_end; }
        constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator(m_end); }
        constexpr reverse_iterator rend() const noexcept { return reverse_iterator(m_begin); }
        constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
        constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

        // [string.view.modifiers], modifiers:
        constexpr size_type copy(pointer dest, size_type count, size_type pos = 0) const
        {
            AZ_Assert(pos <= size(), "Position of bytes to copy to destination is out of string_view range");
            if (pos > size())
            {
                return 0;
            }
            size_type rlen = AZ::GetMin<size_type>(count, size() - pos);
            Traits::copy(dest, data() + pos, rlen);
            return rlen;
        }

        constexpr basic_string_view substr(size_type pos = 0, size_type count = npos) const
        {
            AZ_Assert(pos <= size(), "Cannot create substring where position is larger than size");
            return pos > size() ? basic_string_view() : basic_string_view(data() + pos, AZ::GetMin<size_type>(count, size() - pos));
        }

        constexpr int compare(basic_string_view other) const
        {
            size_t cmpSize = AZ::GetMin<size_type>(size(), other.size());
            int cmpval = cmpSize == 0 ? 0 : Traits::compare(data(), other.data(), cmpSize);
            if (cmpval == 0)
            {
                if (size() == other.size())
                {
                    return 0;
                }
                else if (size() < other.size())
                {
                    return -1;
                }

                return 1;
            }

            return cmpval;
        }

        constexpr int compare(size_type pos1, size_type count1, basic_string_view other) const
        {
            return substr(pos1, count1).compare(other);
        }

        constexpr int compare(size_type pos1, size_type count1, basic_string_view sv, size_type pos2, size_type count2) const
        {
            return substr(pos1, count1).compare(sv.substr(pos2, count2));
        }

        constexpr int compare(const_pointer s) const
        {
            return compare(basic_string_view(s));
        }

        constexpr int compare(size_type pos1, size_type count1, const_pointer s) const
        {
            return substr(pos1, count1).compare(basic_string_view(s));
        }

        constexpr int compare(size_type pos1, size_type count1, const_pointer s, size_type count2) const
        {
            return substr(pos1, count1).compare(basic_string_view(s, count2));
        }

        // starts_with
        constexpr bool starts_with(basic_string_view prefix) const
        {
            return size() >= prefix.size() && compare(0, prefix.size(), prefix) == 0;
        }

        constexpr bool starts_with(value_type prefix) const
        {
            return starts_with(basic_string_view(&prefix, 1));
        }

        constexpr bool starts_with(const_pointer prefix) const
        {
            return starts_with(basic_string_view(prefix));
        }

        // ends_with
        constexpr bool ends_with(basic_string_view suffix) const
        {
            return size() >= suffix.size() && compare(size() - suffix.size(), npos, suffix) == 0;
        }

        constexpr bool ends_with(value_type suffix) const
        {
            return ends_with(basic_string_view(&suffix, 1));
        }

        constexpr bool ends_with(const_pointer suffix) const
        {
            return ends_with(basic_string_view(suffix));
        }

        // find
        constexpr size_type find(basic_string_view other, size_type pos = 0) const
        {
            return StringInternal::find<value_type, size_type, traits_type, npos>(data(), size(), other.data(), pos, other.size());
        }

        constexpr size_type find(value_type c, size_type pos = 0) const
        {
            return StringInternal::find<value_type, size_type, traits_type, npos>(data(), size(), c, pos);
        }

        constexpr size_type find(const_pointer s, size_type pos, size_type count) const
        {
            AZ_Assert(count == 0 || s, "string_view::find(): received nullptr");
            return find(basic_string_view{ s, count }, pos);
        }

        constexpr size_type find(const_pointer s, size_type pos = 0) const
        {
            AZ_Assert(s, "string_view::find(): received nullptr");
            return find(basic_string_view{ s, traits_type::length(s) }, pos);
        }

        // rfind
        constexpr size_type rfind(basic_string_view s, size_type pos = npos) const
        {
            return StringInternal::rfind<value_type, size_type, traits_type, npos>(data(), size(), s.data(), pos, s.size());
        }

        constexpr size_type rfind(value_type c, size_type pos = npos) const
        {
            return StringInternal::rfind<value_type, size_type, traits_type, npos>(data(), size(), c, pos);
        }

        constexpr size_type rfind(const_pointer s, size_type pos, size_type count) const
        {
            AZ_Assert(count == 0 || s, "string_view::rfind(): received nullptr");
            return rfind(basic_string_view{ s, count }, pos);
        }

        constexpr size_type rfind(const_pointer s, size_type pos = npos) const
        {
            AZ_Assert(s, "string_view::rfind(): received nullptr");
            return rfind(basic_string_view{ s, traits_type::length(s) }, pos);
        }

        // find_first_of
        constexpr size_type find_first_of(basic_string_view s, size_type pos = 0) const
        {
            return StringInternal::find_first_of<value_type, size_type, traits_type, npos>(data(), size(), s.data(), pos, s.size());
        }

        constexpr size_type find_first_of(value_type c, size_type pos = 0) const
        {
            return find(c, pos);
        }

        constexpr size_type find_first_of(const_pointer s, size_type pos, size_type count) const
        {
            AZ_Assert(count == 0 || s, "string_view::find_first_of(): received nullptr");
            return find_first_of(basic_string_view{ s, count }, pos);
        }

        constexpr size_type find_first_of(const_pointer s, size_type pos = 0) const
        {
            AZ_Assert(s, "string_view::find_first_of(): received nullptr");
            return find_first_of(basic_string_view{ s, traits_type::length(s) }, pos);
        }

        // find_last_of
        constexpr size_type find_last_of(basic_string_view s, size_type pos = npos) const
        {
            return StringInternal::find_last_of<value_type, size_type, traits_type, npos>(data(), size(), s.data(), pos, s.size());
        }

        constexpr size_type find_last_of(value_type c, size_type pos = npos) const
        {
            return rfind(c, pos);
        }

        constexpr size_type find_last_of(const_pointer s, size_type pos, size_type count) const
        {
            AZ_Assert(count == 0 || s, "string_view::find_last_of(): received nullptr");
            return find_last_of(basic_string_view{ s, count }, pos);
        }

        constexpr size_type find_last_of(const_pointer s, size_type pos = npos) const
        {
            AZ_Assert(s, "string_view::find_last_of(): received nullptr");
            return find_last_of(basic_string_view{ s, traits_type::length(s) }, pos);
        }

        // find_first_not_of
        constexpr size_type find_first_not_of(basic_string_view s, size_type pos = 0) const
        {
            return StringInternal::find_first_not_of<value_type, size_type, traits_type, npos>(data(), size(), s.data(), pos, s.size());
        }

        constexpr size_type find_first_not_of(value_type c, size_type pos = 0) const
        {
            return StringInternal::find_first_not_of<value_type, size_type, traits_type, npos>(data(), size(), c, pos);
        }

        constexpr size_type find_first_not_of(const_pointer s, size_type pos, size_type count) const
        {
            AZ_Assert(count == 0 || s, "string_view::find_first_not_of(): received nullptr");
            return find_first_not_of(basic_string_view{ s, count }, pos);
        }

        constexpr size_type find_first_not_of(const_pointer s, size_type pos = 0) const
        {
            AZ_Assert(s, "string_view::find_first_not_of(): received nullptr");
            return find_first_not_of(basic_string_view{ s, traits_type::length(s) }, pos);
        }

        // find_last_not_of
        constexpr size_type find_last_not_of(basic_string_view s, size_type pos = npos) const
        {
            return StringInternal::find_last_not_of<value_type, size_type, traits_type, npos>(data(), size(), s.data(), pos, s.size());
        }

        constexpr size_type find_last_not_of(value_type c, size_type pos = npos) const
        {
            return StringInternal::find_last_not_of<value_type, size_type, traits_type, npos>(data(), size(), c, pos);
        }

        constexpr size_type find_last_not_of(const_pointer s, size_type pos, size_type count) const
        {
            AZ_Assert(count == 0 || s, "string_view::find_last_not_of(): received nullptr");
            return find_last_not_of(basic_string_view{ s, count }, pos);
        }

        constexpr size_type find_last_not_of(const_pointer s, size_type pos = npos) const
        {
            AZ_Assert(s, "string_view::find_last_not_of(): received nullptr");
            return find_last_not_of(basic_string_view{ s, traits_type::length(s) }, pos);
        }

    private:
        constexpr void erase() { m_begin = m_end = nullptr; }
        constexpr void resize(size_type new_len) { if (m_begin + new_len < m_end) m_end = m_begin + new_len; }

        const_pointer m_begin;
        const_pointer m_end;
    };

    template <class Element, class Traits>
    const typename basic_string_view<Element, Traits>::size_type basic_string_view<Element, Traits>::npos;

    using string_view = basic_string_view<char>;
    using wstring_view = basic_string_view<wchar_t>;

    template<class Element, class Traits = AZStd::char_traits<Element>>
    using basic_const_string = basic_string_view<Element, Traits>;
    using const_string = string_view;
    using const_wstring = wstring_view;

    template <class Element, class Traits = AZStd::char_traits<Element>>
    constexpr typename basic_string_view<Element, Traits>::const_iterator begin(basic_string_view<Element, Traits> sv)
    {
        return sv.begin();
    }

    template <class Element, class Traits = AZStd::char_traits<Element>>
    constexpr typename basic_string_view<Element, Traits>::const_iterator end(basic_string_view<Element, Traits> sv)
    {
        return sv.end();
    }

    inline namespace literals
    {
        inline namespace string_view_literals
        {
            constexpr string_view operator "" _sv(const char* str, size_t len) noexcept
            {
                return string_view{ str, len };
            }

            constexpr wstring_view operator "" _sv(const wchar_t* str, size_t len) noexcept
            {
                return wstring_view{ str, len };
            }
        }
    }

    /// For string hashing we are using FNV-1a algorithm 64 bit version.
    template<class RandomAccessIterator>
    constexpr size_t hash_string(RandomAccessIterator first, size_t length)
    {
        size_t hash = 14695981039346656037ULL;
        constexpr size_t fnvPrime = 1099511628211ULL;

        const RandomAccessIterator last(first + length);
        for (; first != last; ++first)
        {
            hash ^= static_cast<size_t>(*first);
#if AZ_COMPILER_MSVC < 1924
            // Workaround for integer overflow warning for hash function when used in a constexpr context
            // The warning must be disabled at the call site and is a compiler bug that has been fixed
            // with Visual Studio 2019 version 16.4
            // https://developercommunity.visualstudio.com/content/problem/211134/unsigned-integer-overflows-in-constexpr-functionsa.html?childToView=211580#comment-211580
            constexpr size_t fnvPrimeHigh{ 0x100ULL };
            constexpr size_t fnvPrimeLow{ 0x000001b3 };
            const uint64_t hashHigh{ hash >> 32 };
            const uint64_t hashLow{ hash & 0xFFFF'FFFF };
            const uint64_t lowResult{ hashLow * fnvPrimeLow };
            const uint64_t fnvPrimeHighResult{ hashLow * fnvPrimeHigh };
            const uint64_t hashHighResult{ hashHigh * fnvPrimeLow };
            hash = (lowResult & 0xffff'ffff) + (((lowResult >> 32) + (fnvPrimeHighResult & 0xffff'ffff) + (hashHighResult & 0xffff'ffff)) << 32);
#else
            hash *= fnvPrime;
#endif
        }
        return hash;
    }

    template<class Element, class Traits>
    struct hash<basic_string_view<Element, Traits>>
    {
        constexpr size_t operator()(const basic_string_view<Element, Traits>& value) const
        {
            // from the string class
            return hash_string(value.begin(), value.length());
        }
    };

} // namespace AZStd

//! Use this macro to simplify safe printing of a string_view which may not be null-terminated.
//! Example: AZStd::string::format("Safely formatted: %.*s", AZ_STRING_ARG(myString));
#define AZ_STRING_ARG(str) aznumeric_cast<int>(str.size()), str.data()

//! Can be used with AZ_STRING_ARG for convenience, rather than manually including "%.*s" in format strings
//! Example: AZStd::string::format("Safely formatted: " AZ_STRING_FORMAT, AZ_STRING_ARG(myString));
#define AZ_STRING_FORMAT "%.*s"