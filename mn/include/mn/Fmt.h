#pragma once

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <mn/Str.h>
#include <mn/Buf.h>
#include <mn/Map.h>
#include <mn/File.h>

namespace fmt
{
	template<>
	struct formatter<mn::Str> {
		template <typename ParseContext>
		constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

		template <typename FormatContext>
		auto format(const mn::Str &str, FormatContext &ctx) {
			return format_to(ctx.out(), "{}", str.ptr);
		}
	};

	template<typename T>
	struct formatter<mn::Buf<T>> {
		template <typename ParseContext>
		constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

		template <typename FormatContext>
		auto format(const mn::Buf<T> &buf, FormatContext &ctx) {
			format_to(ctx.out(), "[{}]{{", buf.count);
			for(size_t i = 0; i < buf.count; ++i)
			{
				if(i != 0)
					format_to(ctx.out(), ", ");
				format_to(ctx.out(), "{}: {}", i, buf[i]);
			}
			format_to(ctx.out(), " }}");
			return ctx.begin();
		}
	};

	template<typename TKey, typename TValue, typename THash>
	struct formatter<mn::Map<TKey, TValue, THash>> {
		template <typename ParseContext>
		constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

		template <typename FormatContext>
		auto format(const mn::Map<TKey, TValue, THash> &map, FormatContext &ctx) {
			format_to(ctx.out(), "[{}]{{ ", map.count);
			size_t i = 0;
			for(auto it = map_begin(map); it != map_end(map); it = map_next(map, it))
			{
				if(i != 0)
					format_to(ctx.out(), ", ");
				format_to(ctx.out(), "{}: {}", it->key, it->value);
				++i;
			}
			format_to(ctx.out(), " }}");
			return ctx.begin();
		}
	};
}

namespace mn
{
	template<typename ... Args>
	inline static Str
	strf(Str out, const char* format_str, Args&& ... args)
	{
		fmt::memory_buffer buf;
		fmt::format_to(buf, format_str, args...);
		str_block_push(out, Block{buf.data(), buf.size()});
		return out;
	}

	template<typename ... Args>
	inline static Str
	strf(Allocator allocator, const char* format_str, Args&& ... args)
	{
		return strf(str_with_allocator(allocator), format_str, args...);
	}

	template<typename ... Args>
	inline static Str
	strf(const char* format_str, Args&& ... args)
	{
		return strf(str_new(), format_str, args...);
	}

	template<typename ... Args>
	inline static Str
	str_tmpf(const char* format_str, Args&& ... args)
	{
		return strf(str_tmp(), format_str, args...);
	}

	template<typename ... Args>
	inline static size_t
	print_to(Stream stream, const char* format_str, Args&& ... args)
	{
		fmt::memory_buffer buf;
		fmt::format_to(buf, format_str, args...);
		return stream_write(stream, Block{buf.data(), buf.size()});
	}

	template<typename ... Args>
	inline static size_t
	print(const char* format_str, Args&& ... args)
	{
		return stream_print(file_stdout(), format_str, args...);
	}

	template<typename ... Args>
	inline static size_t
	printerr(const char* format_str, Args&& ... args)
	{
		return stream_print(file_stderr(), format_str, args...);
	}
}