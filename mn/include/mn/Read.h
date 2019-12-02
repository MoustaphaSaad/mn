#pragma once
#include "mn/File.h"
#include "mn/Memory_Stream.h"
#include "mn/Reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits>
#include <assert.h>

namespace mn
{
	/**
	 * [[markdown]]
	 * # The IO
	 * Here's a listing of the function provided by this file
	 * - **read**: reads values as strings from the buffered stdin
	 * - **vreadb**: reads values as binary data from a specific Stream
	 * - **vreads**: reads values as string values from a specific Stream
	 * 
	 * ## Custom Read String Function
	 * ```C++
	 * inline static usize
	 * read_str(Reader reader, Your_Type& value);
	 * ```
	 * If you define this function then it will be used when you use the `vreads` function
	 * 
	 * ## Custom Read Binary Function
	 * ```C++
	 * inline static usize
	 * read_bin(Stream stream, Your_Type& value)
	 * 
	 * inline static usize
	 * read_bin(Reader reader, Your_Type& value)
	 * ```
	 * If you define this function then it will be used when you use the `vreadb` function
	 * The default behaviour for your custom types is that we just read the `sizeof(Your_Type)` from the IO_Trait so if that suits you then there's no need to provide a custom function
	 */

	//reader
	inline static bool
	_is_whitespace(char c)
	{
		return (c == ' '  ||
				c == '\f' ||
				c == '\n' ||
				c == '\r' ||
				c == '\t' ||
				c == '\v');
	}

	inline static void
	_guarantee_text_chunk(Reader reader, size_t REQUEST_SIZE)
	{
		size_t requested_size = 0;
		size_t last_size = size_t(-1);

		//skip all the spaces at the start of the string
		while (true)
		{
			Block bytes = reader_peek(reader, requested_size);

			if(bytes.size == last_size)
				break;

			size_t whitespace_count = 0;
			for(size_t i = 0; i < bytes.size; ++i)
			{
				char byte = ((char*)bytes.ptr)[i];
				if(_is_whitespace(byte))
					++whitespace_count;
				else
					break;
			}
			reader_skip(reader, whitespace_count);
			if (whitespace_count < bytes.size)
				break;

			requested_size += REQUEST_SIZE;
			last_size = bytes.size;
		}

		//guarantee that we didn't chop of early
		//i.e. [12345]13412\r\n
		//so we have to check for whitespaces at the end also
		//set the requested size to 0 to get the already buffered data
		requested_size = 0;
		last_size = size_t(-1);
		while (true)
		{
			bool found_whitespace = false;

			Block bytes = reader_peek(reader, requested_size);

			if(bytes.size == last_size)
				break;

			last_size = bytes.size;
			char* ptr = (char*)bytes.ptr + bytes.size;
			while(ptr-- != bytes.ptr)
			{
				if(_is_whitespace(*ptr))
				{
					found_whitespace = true;
					break;
				}
			}

			if (found_whitespace)
				break;

			requested_size += REQUEST_SIZE;
		}
	}

	inline static size_t
	_read_uint64(Reader reader, uint64_t& value, int base)
	{
		_guarantee_text_chunk(reader, 40);
		Block peeked_content = reader_peek(reader, 0);

		if(!peeked_content)
			return 0;

		char* begin = (char*)peeked_content.ptr;
		char* end = (char*)peeked_content.ptr + peeked_content.size;
		//we cannot parse negative numbers
		if(*begin == '-')
			return 0;

		errno = 0;
		uint64_t tmp_value = ::strtoull(begin, &end, base);

		if(errno == ERANGE)
			return 0;

		value = tmp_value;

		return end - begin;
	}

	inline static size_t
	_read_int64(Reader reader, int64_t& value, int base)
	{
		_guarantee_text_chunk(reader, 40);
		Block peeked_content = reader_peek(reader, 0);

		char* begin = (char*)peeked_content.ptr;
		char* end = (char*)peeked_content.ptr + peeked_content.size;
		errno = 0;
		int64_t tmp_value = ::strtoll(begin, &end, base);

		if(errno == ERANGE)
			return 0;

		value = tmp_value;

		return end - begin;
	}

	inline static size_t
	_read_float(Reader reader, float& value)
	{
		_guarantee_text_chunk(reader, 40);
		Block peeked_content = reader_peek(reader, 0);

		char* begin = (char*)peeked_content.ptr;
		char* end = (char*)peeked_content.ptr + peeked_content.size;
		errno = 0;
		float tmp_value = ::strtof(begin, &end);

		if(errno == ERANGE)
			return 0;

		value = tmp_value;

		return end - begin;
	}

	inline static size_t
	_read_double(Reader reader, double& value)
	{
		_guarantee_text_chunk(reader, 40);
		auto peeked_content = reader_peek(reader, 0);

		char* begin = (char*)peeked_content.ptr;
		char* end = (char*)peeked_content.ptr + peeked_content.size;
		errno = 0;
		double tmp_value = ::strtod(begin, &end);

		if(errno == ERANGE)
			return 0;

		value = tmp_value;

		return end - begin;
	}

	//signed functions
	#define READ_STR_SIGNED(TYPE) \
	inline static size_t \
	read_str(Reader reader, TYPE& value) \
	{ \
		int64_t tmp_value = 0; \
		size_t parsed_size = _read_int64(reader, tmp_value, 10); \
		if(parsed_size == 0) \
			return 0;\
		if (tmp_value < std::numeric_limits<TYPE>::lowest() || \
			tmp_value > std::numeric_limits<TYPE>::max()) \
		{ \
			assert(false && "signed integer overflow"); \
			return 0; \
		} \
		value = static_cast<TYPE>(tmp_value); \
		return reader_skip(reader, parsed_size); \
	}

	READ_STR_SIGNED(int8_t)
	READ_STR_SIGNED(int16_t)
	READ_STR_SIGNED(int32_t)
	READ_STR_SIGNED(int64_t)

	#undef READ_STR_SIGNED

	inline static size_t
	read_str(Reader reader, char& value)
	{
		return reader_read(reader, block_from(value));
	}

	//unsigned functions
	#define READ_STR_UNSIGNED(TYPE) \
	inline static size_t \
	read_str(Reader reader, TYPE& value) \
	{ \
		uint64_t tmp_value = 0; \
		size_t parsed_size = _read_uint64(reader, tmp_value, 10); \
		if(parsed_size == 0) \
			return 0;\
		if (tmp_value > std::numeric_limits<TYPE>::max()) \
		{ \
			assert(false && "unsigned integer overflow"); \
			return 0; \
		} \
		value = static_cast<TYPE>(tmp_value); \
		return reader_skip(reader, parsed_size); \
	}

	READ_STR_UNSIGNED(uint8_t)
	READ_STR_UNSIGNED(uint16_t)
	READ_STR_UNSIGNED(uint32_t)
	READ_STR_UNSIGNED(uint64_t)

	#undef READ_STR_UNSIGNED

	inline static size_t
	read_str(Reader reader, void*& value)
	{
		uint64_t tmp_value = 0;
		size_t parsed_size = _read_uint64(reader, tmp_value, 16);
		if(parsed_size == 0)
			return 0;
		if(tmp_value > std::numeric_limits<size_t>::max())
		{
			assert(false && "pointer address overflow");
			return 0;
		}
		value = reinterpret_cast<void*>(tmp_value);
		return reader_skip(reader, parsed_size);
	}

	inline static size_t
	read_str(Reader reader, float& value)
	{
		float tmp_value = 0;
		size_t parsed_size = _read_float(reader, tmp_value);
		if(parsed_size == 0)
		{
			assert(false && "float overflow");
			return 0;
		}
		value = tmp_value;
		return reader_skip(reader, parsed_size);
	}

	inline static size_t
	read_str(Reader reader, double& value)
	{
		double tmp_value = 0;
		size_t parsed_size = _read_double(reader, tmp_value);
		if(parsed_size == 0)
		{
			assert(false && "double overflow");
			return 0;
		}
		value = tmp_value;
		return reader_skip(reader, parsed_size);
	}

	inline static size_t
	read_str(Reader reader, Str& value)
	{
		_guarantee_text_chunk(reader, 1024);
		auto bytes = reader_peek(reader, 0);
		size_t non_whitespace_count = 0;
		for(size_t i = 0; i < bytes.size; ++i)
		{
			char byte = ((char*)bytes.ptr)[i];
			if(_is_whitespace(byte))
				break;
			++non_whitespace_count;
		}

		if(non_whitespace_count == 0)
			return 0;

		str_clear(value);
		str_block_push(value, Block { bytes.ptr, non_whitespace_count });
		return reader_skip(reader, non_whitespace_count);
	}

	inline static size_t
	readln(Reader reader, Str& value)
	{
		size_t newline_offset = size_t(-1);
		size_t last_size = size_t(-1);
		size_t request_size = 0;
		while(true)
		{
			auto bytes = reader_peek(reader, request_size);

			bool found_newline = false;
			for(size_t i = 0; i < bytes.size; ++i)
			{
				char byte = ((char*)bytes.ptr)[i];
				if(byte == '\n')
				{
					found_newline = true;
					newline_offset = i;
					break;
				}
			}

			if(found_newline)
				break;
			else if(last_size == bytes.size)
				break;

			request_size += 1024;
			last_size = bytes.size;
		}

		auto bytes = reader_peek(reader, 0);
		str_clear(value);
		if(newline_offset != size_t(-1))
		{
			size_t additional_skip = 1;
			if(newline_offset > 0 &&
			   ((char*)bytes.ptr)[newline_offset - 1] == '\r')
			{
				//because of the \r\n on window
				--newline_offset;
				++additional_skip;
			}

			str_block_push(value, Block { bytes.ptr, newline_offset });
			return reader_skip(reader, newline_offset + additional_skip);
		}
		else
		{
			str_block_push(value, bytes);
			return reader_skip(reader, bytes.size);
		}
	}

	inline static size_t
	readln(Str& value)
	{
		return readln(reader_stdin(), value);
	}

	inline static void
	_variadic_read_string_helper(Reader, size_t&)
	{
		return;
	}

	template<typename TFirst, typename ... TArgs>
	inline static void
	_variadic_read_string_helper(Reader reader,
								 size_t& size,
								 TFirst&& first_arg,
								 TArgs&& ... args)
	{
		size += read_str(reader, std::forward<TFirst>(first_arg)) != 0;
		_variadic_read_string_helper(reader, size, std::forward<TArgs>(args)...);
	}

	template<typename ... TArgs>
	inline static size_t
	vreads(Reader reader, TArgs&& ... args)
	{
		size_t result = 0;
		_variadic_read_string_helper(reader, result, std::forward<TArgs>(args)...);
		return result;
	}

	template<typename ... TArgs>
	inline static size_t
	reads(const Str& str, TArgs&& ... args)
	{
		Reader reader = reader_tmp();
		reader = reader_wrap_str(reader, str);
		size_t result = 0;
		_variadic_read_string_helper(reader, result, std::forward<TArgs>(args)...);
		return result;
	}

	template<typename ... TArgs>
	inline static size_t
	reads(const char* str, TArgs&& ... args)
	{
		return reads(str_lit(str), std::forward<TArgs>(args)...);
	}

	template<typename ... TArgs>
	inline static size_t
	read(TArgs&& ... args)
	{
		return vreads(reader_stdin(), std::forward<TArgs>(args)...);
	}


	template<typename T>
	inline static size_t
	read_bin(Stream stream, T& value)
	{
		return stream_read(stream, block_from(value));
	}

	template<typename T>
	inline static size_t
	read_bin(Reader reader, T& value)
	{
		return reader_read(reader, block_from(value));
	}

	inline static size_t
	read_bin(Stream stream, Block value)
	{
		return stream_read(stream, value);
	}

	inline static size_t
	read_bin(Reader reader, Block value)
	{
		return reader_read(reader, value);
	}

	inline static void
	_variadic_read_binary_helper(Stream, size_t&)
	{
		return;
	}

	template<typename TFirst, typename ... TArgs>
	inline static void
	_variadic_read_binary_helper(Stream stream,
								 size_t& size,
								 TFirst&& first_arg,
								 TArgs&& ... args)
	{
		size += read_bin(stream, std::forward<TFirst>(first_arg));
		_variadic_read_binary_helper(stream, size, std::forward<TArgs>(args)...);
	}

	inline static void
	_variadic_read_binary_helper(Reader, size_t&)
	{
		return;
	}

	template<typename TFirst, typename ... TArgs>
	inline static void
	_variadic_read_binary_helper(Reader reader,
								 size_t& size,
								 TFirst&& first_arg,
								 TArgs&& ... args)
	{
		size += read_bin(reader, std::forward<TFirst>(first_arg));
		_variadic_read_binary_helper(reader, size, std::forward<TArgs>(args)...);
	}

	template<typename ... TArgs>
	inline static size_t
	vreadb(Stream stream, TArgs&& ... args)
	{
		size_t result = 0;
		_variadic_read_binary_helper(stream, result, std::forward<TArgs>(args)...);
		return result;
	}

	template<typename ... TArgs>
	inline static size_t
	vreadb(Reader reader, TArgs&& ... args)
	{
		size_t result = 0;
		_variadic_read_binary_helper(reader, result, std::forward<TArgs>(args)...);
		return result;
	}
}
