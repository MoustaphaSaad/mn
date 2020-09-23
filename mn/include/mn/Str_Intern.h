#pragma once

#include "mn/Exports.h"
#include "mn/Str.h"
#include "mn/Map.h"

namespace mn
{
	/**
	 * @brief      String Interning structure
	 * A String interning is an operation in which all of the unique strings is stored once
	 * and every time a duplicate is encountered it returns a pointer to the same stored string
	 * it's used mainly to avoid string compare functions since all you have to do now is compare
	 * the string pointers if they are the same then they have the same content
	 */
	struct Str_Intern
	{
		Str tmp_str;
		Set<Str> strings;
	};

	/**
	 * @brief      Creates a new str_intern
	 */
	inline static Str_Intern
	str_intern_new()
	{
		return Str_Intern {
			str_new(),
			set_new<Str>()
		};
	}

	/**
	 * @brief      Creates a new str_intern
	 *
	 * @param[in]  allocator  The allocator
	 */
	inline static Str_Intern
	str_intern_with_allocator(Allocator allocator)
	{
		return Str_Intern {
			str_with_allocator(allocator),
			set_with_allocator<Str>(allocator)
		};
	}

	/**
	 * @brief      Frees the given str_intern
	 *
	 * @param      self  The str_intern
	 */
	inline static void
	str_intern_free(Str_Intern& self)
	{
		str_free(self.tmp_str);
		destruct(self.strings);
	}

	/**
	 * @brief      Destruct function overload for the str_intern type
	 *
	 * @param      self  The str_intern
	 */
	inline static void
	destruct(Str_Intern& self)
	{
		str_intern_free(self);
	}

	/**
	 * @brief      Interns the given C string and returns a string pointer to the interned value
	 *
	 * @param      self  The str_intern
	 * @param[in]  str   The C string
	 */
	MN_EXPORT const char*
	str_intern(Str_Intern& self, const char* str);

	/**
	 * @brief      Interns the given string and returns a string pointer to the interned value
	 *
	 * @param      self  The str_intern
	 * @param[in]  str   The string
	 */
	MN_EXPORT const char*
	str_intern(Str_Intern& self, const Str& str);

	/**
	 * @brief      Interns the given C sub-string and returns a string pointer to the interned value
	 *
	 * @param      self  The str_intern
	 * @param[in]  str   The C sub-string
	 */
	MN_EXPORT const char*
	str_intern(Str_Intern& self, const char* begin, const char* end);
}