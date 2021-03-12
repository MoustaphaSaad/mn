#pragma once

#include "mn/Exports.h"
#include "mn/Buf.h"
#include "mn/Str.h"
#include "mn/Result.h"

namespace mn
{
	enum RGX_OP: uint8_t
	{
		// [RUNE, 'a']: matches a rune
		RGX_OP_RUNE,
		// [ANY]: matches any rune
		RGX_OP_ANY,
		// [SPLIT, offset(int32_t), offset(int32_t)]: splits execution into 2 threads
		RGX_OP_SPLIT,
		// [JUMP, offset(int32_t)]: changes the instruction pointer
		RGX_OP_JUMP,
		// [SET, count(int32_t), (RANGE, BYTE)]: matches any byte that's inside the given set of options
		RGX_OP_SET,
		// [NOT_SET, count(int32_t), (RANGE, BYTE)]: matches any byte that's outside the given set of options
		RGX_OP_NOT_SET,
		// [RANGE, 'a', 'z']: specifies a range of bytes
		RGX_OP_RANGE,
		// [MATCH]: registers a match
		RGX_OP_MATCH,
		// [MATCH2, data(int32_t)]: registers a match with an int, this is used in case you have multiple matches and want
		// to make know which match did succeed
		RGX_OP_MATCH2,
	};

	struct Regex
	{
		Buf<uint8_t> bytes;
	};

	inline static Regex
	regex_new()
	{
		return Regex{};
	}

	inline static void
	regex_free(Regex& self)
	{
		buf_free(self.bytes);
	}

	inline static void
	destruct(Regex& self)
	{
		regex_free(self);
	}

	inline static Regex
	regex_clone(const Regex& other, Allocator allocator = allocator_top())
	{
		return Regex{ buf_memcpy_clone(other.bytes, allocator) };
	}

	inline static Regex
	clone(const Regex& other)
	{
		return regex_clone(other);
	}

	// regex compiler
	struct Regex_Compile_Unit
	{
		Allocator program_allocator;
		Str str;
		bool enable_payload;
		int32_t payload;
	};

	MN_EXPORT Result<Regex>
	regex_compile(Regex_Compile_Unit unit);

	inline static Result<Regex>
	regex_compile_with_payload(const Str& str, int32_t payload, Allocator allocator = allocator_top())
	{
		Regex_Compile_Unit unit{};
		unit.program_allocator = allocator;
		unit.str = str;
		unit.enable_payload = true;
		unit.payload = payload;
		return regex_compile(unit);
	}

	inline static Result<Regex>
	regex_compile_with_payload(const char* str, int32_t payload, Allocator allocator = allocator_top())
	{
		return regex_compile_with_payload(str_lit(str), payload, allocator);
	}

	inline static Result<Regex>
	regex_compile(const Str& str, Allocator allocator = allocator_top())
	{
		Regex_Compile_Unit unit{};
		unit.program_allocator = allocator;
		unit.str = str;
		return regex_compile(unit);
	}

	inline static Result<Regex>
	regex_compile(const char* str, Allocator allocator = allocator_top())
	{
		return regex_compile(str_lit(str), allocator);
	}

	// regex vm
	struct Match_Result
	{
		const char* begin;
		const char* end;
		bool match;
		bool with_payload;
		int32_t payload;
	};

	MN_EXPORT Match_Result
	regex_match(const Regex& program, const char* str);

	MN_EXPORT Match_Result
	regex_search(const Regex& program, const char* str);
}