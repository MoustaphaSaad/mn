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
	/**
	 * A String is just a `Buf<char>` so it's suitable to use the buf functions with the string
	 * but use with caution since the null terminator should be maintained all the time
	 */
	using Str = Buf<char>;

	/**
	 * @brief      Returns a new string
	 */
	MN_EXPORT Str
	str_new();

	/**
	 * @brief      Returns a new string
	 *
	 * @param[in]  allocator  The allocator to be used by the string
	 */
	MN_EXPORT Str
	str_with_allocator(Allocator allocator);

	/**
	 * @brief      Returns a new string which has the same content as the given
	 * C string (performs a copy)
	 *
	 * @param[in]  str        The C string
	 * @param[in]  allocator  The allocator
	 */
	MN_EXPORT Str
	str_from_c(const char* str, Allocator allocator = allocator_top());

	/**
	 * @brief      Returns a new string which has the same content of the given
	 * C sub string (performs a copy)
	 *
	 * @param[in]  begin      The begin
	 * @param[in]  end        The end
	 * @param[in]  allocator  The allocator
	 */
	MN_EXPORT Str
	str_from_substr(const char* begin, const char* end, Allocator allocator = allocator_top());

	/**
	 * @brief      Just wraps the given C string literal (doesn't copy)
	 *
	 * @param[in]  lit   The C string literal
	 */
	MN_EXPORT Str
	str_lit(const char* lit);

	inline static Str
	str_tmp(const char* str = nullptr)
	{
		return str_from_c(str, memory::tmp());
	}

	/**
	 * @brief      Frees the string
	 */
	MN_EXPORT void
	str_free(Str& self);

	/**
	 * @brief      Destruct function overload for the string
	 *
	 * @param      self  The string
	 */
	inline static void
	destruct(Str& self)
	{
		str_free(self);
	}

	/**
	 * @brief      Returns the rune count of the given string
	 *
	 * @param[in]  self  The string
	 */
	inline static size_t
	str_rune_count(const Str& self)
	{
		if(self.count)
			return rune_count(self.ptr);
		return 0;
	}

	/**
	 * @brief      Pushes the second string into the first one
	 *
	 * @param      self  The first string
	 * @param[in]  str   The second string literal
	 */
	MN_EXPORT void
	str_push(Str& self, const char* str);

	/**
	 * @brief      Pushes the second string into the first one
	 *
	 * @param      self  The first string
	 * @param[in]  str   The second string
	 */
	inline static void
	str_push(Str& self, const Str& str)
	{
		str_push(self, str.ptr);
	}

	/**
	 * @brief      Pushes a block of memory into the string
	 *
	 * @param      self   The string
	 * @param[in]  block  The block
	 */
	MN_EXPORT void
	str_block_push(Str& self, Block block);

	MN_EXPORT void
	str_push(Str& self, Rune r);

	/**
	 * @brief      Ensures that the string is null terminated
	 *
	 * @param      self  The string
	 */
	MN_EXPORT void
	str_null_terminate(Str& self);

	/**
	 * @brief      Searches for the given string
	 *
	 * @param[in]  self    The string to search in
	 * @param[in]  target  The target string to find
	 * @param[in]  start   The start index to begin the search with
	 *
	 * @return     index of the found target or size_t(-1) instead
	 */
	MN_EXPORT size_t
	str_find(const Str& self, const Str& target, size_t start);

	/**
	 * @brief      Searches for the given string
	 *
	 * @param[in]  self    The string to search in
	 * @param[in]  target  The target string to find
	 * @param[in]  start   The start index to begin the search with
	 *
	 * @return     index of the found target or size_t(-1) instead
	 */
	inline static size_t
	str_find(const Str& self, const char* target, size_t start)
	{
		return str_find(self, str_lit(target), start);
	}

	/**
	 * @brief      Searches for the given string
	 *
	 * @param[in]  self    The string to search in
	 * @param[in]  target  The target string to find
	 * @param[in]  start   The start index to begin the search with
	 *
	 * @return     index of the found target or size_t(-1) instead
	 */
	inline static size_t
	str_find(const char* self, const Str& target, size_t start)
	{
		return str_find(str_lit(self), target, start);
	}

	/**
	 * @brief      Searches for the given string
	 *
	 * @param[in]  self    The string to search in
	 * @param[in]  target  The target string to find
	 * @param[in]  start   The start index to begin the search with
	 *
	 * @return     index of the found target or size_t(-1) instead
	 */
	inline static size_t
	str_find(const char* self, const char* target, size_t start)
	{
		return str_find(str_lit(self), str_lit(target), start);
	}

	MN_EXPORT size_t
	str_find(const Str& self, Rune r, size_t start_in_bytes);

	inline static size_t
	str_find(const char* self, Rune r, size_t start_in_bytes)
	{
		return str_find(str_lit(self), r, start_in_bytes);
	}

	MN_EXPORT void
	str_replace(Str& self, char to_remove, char to_add);

	MN_EXPORT void
	str_replace(Str& self, const Str& search, const Str& replace);

	inline static void
	str_replace(Str& self, const Str& search, const char* replace)
	{
		str_replace(self, search, str_lit(replace));
	}

	inline static void
	str_replace(Str& self, const char* search, const Str& replace)
	{
		str_replace(self, str_lit(search), replace);
	}

	inline static void
	str_replace(Str& self, const char* search, const char* replace)
	{
		str_replace(self, str_lit(search), str_lit(replace));
	}

	/**
	 * @brief      Splits the string with the given delimiter
	 *
	 * @param[in]  self        The string
	 * @param[in]  delim       The delimiter to split with
	 * @param[in]  skip_empty  determines whether we shouldn't return empty strings or not
	 * @param[in]  allocator   The allocator [optional] default is the tmp allocator
	 */
	MN_EXPORT Buf<Str>
	str_split(const Str& self, const Str& delim, bool skip_empty, Allocator allocator = memory::tmp());

	/**
	 * @brief      Splits the string with the given delimiter
	 *
	 * @param[in]  self        The string
	 * @param[in]  delim       The delimiter to split with
	 * @param[in]  skip_empty  determines whether we shouldn't return empty strings or not
	 * @param[in]  allocator   The allocator [optional] default is the tmp allocator
	 */
	inline static Buf<Str>
	str_split(const Str& self, const char* delim, bool skip_empty, Allocator allocator = memory::tmp())
	{
		return str_split(self, str_lit(delim), skip_empty, allocator);
	}

	/**
	 * @brief      Splits the string with the given delimiter
	 *
	 * @param[in]  self        The string
	 * @param[in]  delim       The delimiter to split with
	 * @param[in]  skip_empty  determines whether we shouldn't return empty strings or not
	 * @param[in]  allocator   The allocator [optional] default is the tmp allocator
	 */
	inline static Buf<Str>
	str_split(const char* self, const Str& delim, bool skip_empty, Allocator allocator = memory::tmp())
	{
		return str_split(str_lit(self), delim, skip_empty, allocator);
	}

	/**
	 * @brief      Splits the string with the given delimiter
	 *
	 * @param[in]  self        The string
	 * @param[in]  delim       The delimiter to split with
	 * @param[in]  skip_empty  determines whether we shouldn't return empty strings or not
	 * @param[in]  allocator   The allocator [optional] default is the tmp allocator
	 */
	inline static Buf<Str>
	str_split(const char* self, const char* delim, bool skip_empty, Allocator allocator = memory::tmp())
	{
		return str_split(str_lit(self), str_lit(delim), skip_empty, allocator);
	}

	/**
	 * @brief      Checks if the string is starting with the given prefix
	 *
	 * @param[in]  self    The string
	 * @param[in]  prefix  The prefix
	 */
	MN_EXPORT bool
	str_prefix(const Str& self, const Str& prefix);

	/**
	 * @brief      Checks if the string is starting with the given prefix
	 *
	 * @param[in]  self    The string
	 * @param[in]  prefix  The prefix
	 */
	inline static bool
	str_prefix(const Str& self, const char* prefix)
	{
		return str_prefix(self, str_lit(prefix));
	}

	/**
	 * @brief      Checks if the string is starting with the given prefix
	 *
	 * @param[in]  self    The string
	 * @param[in]  prefix  The prefix
	 */
	inline static bool
	str_prefix(const char* self, const Str& prefix)
	{
		return str_prefix(str_lit(self), prefix);
	}

	/**
	 * @brief      Checks if the string is starting with the given prefix
	 *
	 * @param[in]  self    The string
	 * @param[in]  prefix  The prefix
	 */
	inline static bool
	str_prefix(const char* self, const char* prefix)
	{
		return str_prefix(str_lit(self), str_lit(prefix));
	}

	/**
	 * @brief      Checks if the string is ending with the given suffix
	 *
	 * @param[in]  self    The string
	 * @param[in]  suffix  The suffix
	 */
	MN_EXPORT bool
	str_suffix(const Str& self, const Str& suffix);

	/**
	 * @brief      Checks if the string is ending with the given suffix
	 *
	 * @param[in]  self    The string
	 * @param[in]  suffix  The suffix
	 */
	inline static bool
	str_suffix(const Str& self, const char* suffix)
	{
		return str_suffix(self, str_lit(suffix));
	}

	/**
	 * @brief      Checks if the string is ending with the given suffix
	 *
	 * @param[in]  self    The string
	 * @param[in]  suffix  The suffix
	 */
	inline static bool
	str_suffix(const char* self, const Str& suffix)
	{
		return str_suffix(str_lit(self), suffix);
	}

	/**
	 * @brief      Checks if the string is ending with the given suffix
	 *
	 * @param[in]  self    The string
	 * @param[in]  suffix  The suffix
	 */
	inline static bool
	str_suffix(const char* self, const char* suffix)
	{
		return str_suffix(str_lit(self), str_lit(suffix));
	}

	/**
	 * @brief      Resizes the string to the given size
	 *
	 * @param      self  The string
	 * @param[in]  size  The size
	 */
	MN_EXPORT void
	str_resize(Str& self, size_t size);

	/**
	 * @brief      Clears the string
	 *
	 * @param      self  The string
	 */
	MN_EXPORT void
	str_clear(Str& self);

	/**
	 * @brief      Clones the given string
	 *
	 * @param[in]  other      The string
	 * @param[in]  allocator  The allocator to be used in the returned string
	 *
	 * @return     The newly cloned string
	 */
	MN_EXPORT Str
	str_clone(const Str& other, Allocator allocator = allocator_top());

	template<typename TFunc>
	inline static void
	str_trim_left_pred(Str& self, TFunc&& f)
	{
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

	inline static void
	str_trim_left(Str& self, const Str& cutset)
	{
		str_trim_left_pred(self, [&cutset](Rune r) { return str_find(cutset, r, 0) != size_t(-1); });
	}

	inline static void
	str_trim_left(Str& self, const char* cutset)
	{
		str_trim_left(self, str_lit(cutset));
	}

	inline static void
	str_trim_left(Str& self)
	{
		str_trim_left(self, str_lit("\n\t\r\v "));
	}

	template<typename TFunc>
	inline static void
	str_trim_right_pred(Str& self, TFunc&& f)
	{
		auto it = rune_prev(end(self));
		for(; it != begin(self); it = rune_prev(it))
		{
			auto c = rune_read(it);
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

	inline static void
	str_trim_right(Str& self, const Str& cutset)
	{
		str_trim_right_pred(self, [&cutset](Rune r) { return str_find(cutset, r, 0) != size_t(-1); });
	}

	inline static void
	str_trim_right(Str& self, const char* cutset)
	{
		str_trim_right(self, str_lit(cutset));
	}

	inline static void
	str_trim_right(Str& self)
	{
		str_trim_right(self, str_lit("\n\t\r\v "));
	}

	template<typename TFunc>
	inline static void
	str_trim_pred(Str& self, TFunc&& f)
	{
		str_trim_left_pred(self, f);
		str_trim_right_pred(self, f);
	}

	inline static void
	str_trim(Str& self, const Str& cutset)
	{
		str_trim_pred(self, [&cutset](Rune r) { return str_find(cutset, r, 0) != size_t(-1); });
	}

	inline static void
	str_trim(Str& self, const char* cutset)
	{
		str_trim(self, str_lit(cutset));
	}

	inline static void
	str_trim(Str& self)
	{
		str_trim(self, str_lit("\n\t\r\v "));
	}

	inline static bool
	str_empty(const Str& self)
	{
		return self.count == 0;
	}

	/**
	 * @brief      Clone function overload for the string type
	 *
	 * @param[in]  other  The string to be cloned
	 *
	 * @return     Copy of this object.
	 */
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
