#pragma once

#include "mn/Exports.h"
#include "mn/Rune.h"
#include "mn/Buf.h"
#include "mn/Map.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <utility>
#include <assert.h>

namespace mn
{
	// a string is just a `Buf<char>` so it's suitable to use the buf functions with the string
	// but use with caution since the null terminator should be maintained all the time
	using Str = Buf<char>;

	// creates a new string
	MN_EXPORT Str
	str_new();

	// creates a new string with the given allocator
	MN_EXPORT Str
	str_with_allocator(Allocator allocator);

	// creates a new string from the given c string
	MN_EXPORT Str
	str_from_c(const char* str, Allocator allocator = allocator_top());

	// creates a new string from the given sub string
	MN_EXPORT Str
	str_from_substr(const char* begin, const char* end, Allocator allocator = allocator_top());

	// wraps a string literal into a string (does not allocate)
	MN_EXPORT Str
	str_lit(const char* lit);

	// creates a new temporary string
	inline static Str
	str_tmp(const char* str = nullptr)
	{
		return str_from_c(str, memory::tmp());
	}

	// frees the given string
	MN_EXPORT void
	str_free(Str& self);

	// destruct overload for str free
	inline static void
	destruct(Str& self)
	{
		str_free(self);
	}

	// returns the rune count of the given string
	inline static size_t
	str_rune_count(const Str& self)
	{
		if(self.count)
			return rune_count(self.ptr);
		return 0;
	}

	// pushes the second string into the first one
	MN_EXPORT void
	str_push(Str& self, const char* str);

	// pushes the second string into the first one
	inline static void
	str_push(Str& self, const Str& str)
	{
		str_push(self, str.ptr);
	}

	// pushes the given block of bytes into the string
	MN_EXPORT void
	str_block_push(Str& self, Block block);

	// pushes a rune into the back of the string
	MN_EXPORT void
	str_push(Str& self, Rune r);

	// ensures that string is null terminated
	MN_EXPORT void
	str_null_terminate(Str& self);

	// searches for the given target in the given string starting from the given index, returns the index of target
	// or SIZE_MAX if it doesn't exists
	MN_EXPORT size_t
	str_find(const Str& self, const Str& target, size_t start);

	// searches for the given target in the given string starting from the given index, returns the index of target
	// or SIZE_MAX if it doesn't exists
	inline static size_t
	str_find(const Str& self, const char* target, size_t start)
	{
		return str_find(self, str_lit(target), start);
	}

	// searches for the given target in the given string starting from the given index, returns the index of target
	// or SIZE_MAX if it doesn't exists
	inline static size_t
	str_find(const char* self, const Str& target, size_t start)
	{
		return str_find(str_lit(self), target, start);
	}

	// searches for the given target in the given string starting from the given index, returns the index of target
	// or SIZE_MAX if it doesn't exists
	inline static size_t
	str_find(const char* self, const char* target, size_t start)
	{
		return str_find(str_lit(self), str_lit(target), start);
	}

	// searches for the given target in the given string starting from the given index and moving backwards, returns the index of target
	// or SIZE_MAX if it doesn't exists
	// note: to search the entire string backwards `str_find_last(self, target, self.count)`
	MN_EXPORT size_t
	str_find_last(const Str& self, const Str& target, size_t index);

	// searches for the given target in the given string starting from the given index and moving backwards, returns the index of target
	// or SIZE_MAX if it doesn't exists
	// note: to search the entire string backwards `str_find_last(self, target, self.count)`
	inline static size_t
	str_find_last(const Str& self, const char* target, size_t index)
	{
		return str_find_last(self, str_lit(target), index);
	}

	// searches for the given target in the given string starting from the given index and moving backwards, returns the index of target
	// or SIZE_MAX if it doesn't exists
	// note: to search the entire string backwards `str_find_last(self, target, self.count)`
	inline static size_t
	str_find_last(const char* self, const Str& target, size_t index)
	{
		return str_find_last(str_lit(self), target, index);
	}

	// searches for the given target in the given string starting from the given index and moving backwards, returns the index of target
	// or SIZE_MAX if it doesn't exists
	// note: to search the entire string backwards `str_find_last(self, target, self.count)`
	inline static size_t
	str_find_last(const char* self, const char* target, size_t index)
	{
		return str_find_last(str_lit(self), str_lit(target), index);
	}

	// searches for the given rune in the given string starting from the given index, returns the index of target
	// or SIZE_MAX if it doesn't exists
	MN_EXPORT size_t
	str_find(const Str& self, Rune r, size_t start_in_bytes);

	// searches for the given rune in the given string starting from the given index, returns the index of target
	// or SIZE_MAX if it doesn't exists
	inline static size_t
	str_find(const char* self, Rune r, size_t start_in_bytes)
	{
		return str_find(str_lit(self), r, start_in_bytes);
	}

	// replaces a single char in the given string
	MN_EXPORT void
	str_replace(Str& self, char to_remove, char to_add);

	// replaces the search string with the replace one in the given string
	MN_EXPORT void
	str_replace(Str& self, const Str& search, const Str& replace);

	// replaces the search string with the replace one in the given string
	inline static void
	str_replace(Str& self, const Str& search, const char* replace)
	{
		str_replace(self, search, str_lit(replace));
	}

	// replaces the search string with the replace one in the given string
	inline static void
	str_replace(Str& self, const char* search, const Str& replace)
	{
		str_replace(self, str_lit(search), replace);
	}

	// replaces the search string with the replace one in the given string
	inline static void
	str_replace(Str& self, const char* search, const char* replace)
	{
		str_replace(self, str_lit(search), str_lit(replace));
	}

	// splits the string with the given delimiter, and returns a buf of sub strings allocated using the given allocator
	// skip_empty option allows you to skip empty substrings if it's set to true
	MN_EXPORT Buf<Str>
	str_split(const Str& self, const Str& delim, bool skip_empty, Allocator allocator = memory::tmp());

	// splits the string with the given delimiter, and returns a buf of sub strings allocated using the given allocator
	// skip_empty option allows you to skip empty substrings if it's set to true
	inline static Buf<Str>
	str_split(const Str& self, const char* delim, bool skip_empty, Allocator allocator = memory::tmp())
	{
		return str_split(self, str_lit(delim), skip_empty, allocator);
	}

	// splits the string with the given delimiter, and returns a buf of sub strings allocated using the given allocator
	// skip_empty option allows you to skip empty substrings if it's set to true
	inline static Buf<Str>
	str_split(const char* self, const Str& delim, bool skip_empty, Allocator allocator = memory::tmp())
	{
		return str_split(str_lit(self), delim, skip_empty, allocator);
	}

	// splits the string with the given delimiter, and returns a buf of sub strings allocated using the given allocator
	// skip_empty option allows you to skip empty substrings if it's set to true
	inline static Buf<Str>
	str_split(const char* self, const char* delim, bool skip_empty, Allocator allocator = memory::tmp())
	{
		return str_split(str_lit(self), str_lit(delim), skip_empty, allocator);
	}

	// returns whether the string is starting with the given prefix
	MN_EXPORT bool
	str_prefix(const Str& self, const Str& prefix);

	// returns whether the string is starting with the given prefix
	inline static bool
	str_prefix(const Str& self, const char* prefix)
	{
		return str_prefix(self, str_lit(prefix));
	}

	// returns whether the string is starting with the given prefix
	inline static bool
	str_prefix(const char* self, const Str& prefix)
	{
		return str_prefix(str_lit(self), prefix);
	}

	// returns whether the string is starting with the given prefix
	inline static bool
	str_prefix(const char* self, const char* prefix)
	{
		return str_prefix(str_lit(self), str_lit(prefix));
	}

	// returns whether the string is ending with the given suffix
	MN_EXPORT bool
	str_suffix(const Str& self, const Str& suffix);

	// returns whether the string is ending with the given suffix
	inline static bool
	str_suffix(const Str& self, const char* suffix)
	{
		return str_suffix(self, str_lit(suffix));
	}

	// returns whether the string is ending with the given suffix
	inline static bool
	str_suffix(const char* self, const Str& suffix)
	{
		return str_suffix(str_lit(self), suffix);
	}

	// returns whether the string is ending with the given suffix
	inline static bool
	str_suffix(const char* self, const char* suffix)
	{
		return str_suffix(str_lit(self), str_lit(suffix));
	}

	// resizes the string to the given size in bytes
	MN_EXPORT void
	str_resize(Str& self, size_t size);

	// ensures the given string has the capacity to hold the specified size
	MN_EXPORT void
	str_reserve(Str& self, size_t size);

	// clears the string
	MN_EXPORT void
	str_clear(Str& self);

	// clones the string using the given allocator
	MN_EXPORT Str
	str_clone(const Str& other, Allocator allocator = allocator_top());

	// trims string from the left, it removes all the runes which when passed to the given function it will return true
	template<typename TFunc>
	inline static void
	str_trim_left_pred(Str& self, TFunc&& f)
	{
		if (self.count == 0)
			return;

		const char* it = begin(self);
		for(; it != end(self); it = rune_next(it))
		{
			Rune c = rune_read(it);
			if(f(c) == false)
				break;
		}

		size_t s = size_t(it - self.ptr);
		::memmove(self.ptr, it, self.count - s);
		self.count -= s;
		str_null_terminate(self);
	}

	// trims the string from the left by removing any rune in the cutset
	inline static void
	str_trim_left(Str& self, const Str& cutset)
	{
		str_trim_left_pred(self, [&cutset](Rune r) { return str_find(cutset, r, 0) != size_t(-1); });
	}

	// trims the string from the left by removing any rune in the cutset
	inline static void
	str_trim_left(Str& self, const char* cutset)
	{
		str_trim_left(self, str_lit(cutset));
	}

	// removes whitespaces from the left of the string
	inline static void
	str_trim_left(Str& self)
	{
		str_trim_left(self, str_lit("\n\t\r\v "));
	}

	// trims string from the right, it removes all the runes which when passed to the given function it will return true
	template<typename TFunc>
	inline static void
	str_trim_right_pred(Str& self, TFunc&& f)
	{
		if (self.count == 0)
			return;

		auto it = rune_prev(end(self));
		auto c = rune_read(it);
		if (f(c) == false)
			return;
		it = rune_prev(it);

		for(; it != begin(self); it = rune_prev(it))
		{
			c = rune_read(it);
			if(f(c) == false)
			{
				//then ignore this rune
				it = rune_next(it);
				break;
			}
		}
		size_t s = size_t(it - self.ptr);
		str_resize(self, s);
	}

	// trims the string from the right by removing any rune in the cutset
	inline static void
	str_trim_right(Str& self, const Str& cutset)
	{
		str_trim_right_pred(self, [&cutset](Rune r) { return str_find(cutset, r, 0) != size_t(-1); });
	}

	// trims the string from the right by removing any rune in the cutset
	inline static void
	str_trim_right(Str& self, const char* cutset)
	{
		str_trim_right(self, str_lit(cutset));
	}

	// removes whitespaces from the right of the string
	inline static void
	str_trim_right(Str& self)
	{
		str_trim_right(self, str_lit("\n\t\r\v "));
	}

	// trims string from both ends by removing runes which when passed to the functor it returns true
	template<typename TFunc>
	inline static void
	str_trim_pred(Str& self, TFunc&& f)
	{
		str_trim_left_pred(self, f);
		str_trim_right_pred(self, f);
	}

	// trims string from both ends by removing runes in the given cutset
	inline static void
	str_trim(Str& self, const Str& cutset)
	{
		str_trim_pred(self, [&cutset](Rune r) { return str_find(cutset, r, 0) != size_t(-1); });
	}

	// trims string from both ends by removing runes in the given cutset
	inline static void
	str_trim(Str& self, const char* cutset)
	{
		str_trim(self, str_lit(cutset));
	}

	// removes whitespaces from both ends of the string
	inline static void
	str_trim(Str& self)
	{
		str_trim(self, str_lit("\n\t\r\v "));
	}

	// returns whether the string is empty
	inline static bool
	str_empty(const Str& self)
	{
		return self.count == 0;
	}

	// converts the given string to lower case
	MN_EXPORT void
	str_lower(Str& self);

	// converts the given string to upper case
	MN_EXPORT void
	str_upper(Str& self);

	// a rune iterator which allows string to be used in a range for loop to iterator over its runes
	struct Rune_Iterator
	{
		const char* it;
		Rune r;

		Rune_Iterator&
		operator++()
		{
			it = rune_next(it);
			r = rune_read(it);
			return *this;
		}

		Rune_Iterator
		operator++(int)
		{
			auto tmp = *this;
			it = rune_next(it);
			r = rune_read(it);
			return tmp;
		}

		bool
		operator==(const Rune_Iterator& other) const
		{
			return it == other.it;
		}

		bool
		operator!=(const Rune_Iterator& other) const
		{
			return !operator==(other);
		}

		const Rune&
		operator*() const
		{
			return r;
		}

		const Rune*
		operator->() const
		{
			return &r;
		}
	};

	// runes sub string, which is used to make range for loops work over string runes
	struct Str_Runes
	{
		const char* begin_it;
		const char* end_it;

		Rune_Iterator
		begin() const
		{
			Rune_Iterator res{};
			res.it = begin_it;
			res.r = rune_read(res.it);
			return res;
		}

		Rune_Iterator
		end() const
		{
			Rune_Iterator res{};
			res.it = end_it;
			return res;
		}
	};

	// returns a constant range suitable for usage in range for loops, so that you can loop over string runes
	inline static Str_Runes
	str_runes(const Str& self)
	{
		return Str_Runes {
			begin(self),
			end(self)
		};
	}

	// returns a constant range suitable for usage in range for loops, so that you can loop over string runes
	inline static Str_Runes
	str_runes(const char* str)
	{
		return str_runes(str_lit(str));
	}

	// clone function overload for string clone
	inline static Str
	clone(const Str& other)
	{
		return str_clone(other);
	}

	template<>
	struct Hash<Str>
	{
		inline size_t
		operator()(const Str& str) const
		{
			return str.count ? murmur_hash(str.ptr, str.count) : 0;
		}
	};

	// compares two strings and returns 0 if they are equal, 1 if a > b, and -1 if a < b
	inline static int
	str_cmp(const char* a, const char* b)
	{
		if (a != nullptr && b != nullptr)
		{
			return ::strcmp(a, b);
		}
		else if (a == nullptr && b == nullptr)
		{
			return 0;
		}
		else if (a != nullptr && b == nullptr)
		{
			if (::strlen(a) == 0)
				return 0;
			else
				return 1;
		}
		else if (a == nullptr && b != nullptr)
		{
			if (::strlen(b) == 0)
				return 0;
			else
				return -1;
		}
		else
		{
			assert(false && "UNREACHABLE");
			return 0;
		}
	}

	inline static bool
	operator==(const Str& a, const Str& b)
	{
		return str_cmp(a.ptr, b.ptr) == 0;
	}

	inline static bool
	operator!=(const Str& a, const Str& b)
	{
		return str_cmp(a.ptr, b.ptr) != 0;
	}

	inline static bool
	operator<(const Str& a, const Str& b)
	{
		return str_cmp(a.ptr, b.ptr) < 0;
	}

	inline static bool
	operator<=(const Str& a, const Str& b)
	{
		return str_cmp(a.ptr, b.ptr) <= 0;
	}

	inline static bool
	operator>(const Str& a, const Str& b)
	{
		return str_cmp(a.ptr, b.ptr) > 0;
	}

	inline static bool
	operator>=(const Str& a, const Str& b)
	{
		return str_cmp(a.ptr, b.ptr) >= 0;
	}


	inline static bool
	operator==(const Str& a, const char* b)
	{
		return str_cmp(a.ptr, b) == 0;
	}

	inline static bool
	operator!=(const Str& a, const char* b)
	{
		return str_cmp(a.ptr, b) != 0;
	}

	inline static bool
	operator<(const Str& a, const char* b)
	{
		return str_cmp(a.ptr, b) < 0;
	}

	inline static bool
	operator<=(const Str& a, const char* b)
	{
		return str_cmp(a.ptr, b) <= 0;
	}

	inline static bool
	operator>(const Str& a, const char* b)
	{
		return str_cmp(a.ptr, b) > 0;
	}

	inline static bool
	operator>=(const Str& a, const char* b)
	{
		return str_cmp(a.ptr, b) >= 0;
	}


	inline static bool
	operator==(const char* a, const Str& b)
	{
		return str_cmp(a, b.ptr) == 0;
	}

	inline static bool
	operator!=(const char* a, const Str& b)
	{
		return str_cmp(a, b.ptr) != 0;
	}

	inline static bool
	operator<(const char* a, const Str& b)
	{
		return str_cmp(a, b.ptr) < 0;
	}

	inline static bool
	operator<=(const char* a, const Str& b)
	{
		return str_cmp(a, b.ptr) <= 0;
	}

	inline static bool
	operator>(const char* a, const Str& b)
	{
		return str_cmp(a, b.ptr) > 0;
	}

	inline static bool
	operator>=(const char* a, const Str& b)
	{
		return str_cmp(a, b.ptr) >= 0;
	}
}
