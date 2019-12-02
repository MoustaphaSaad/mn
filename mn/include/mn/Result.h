#pragma once

#include <mn/Str.h>
#include <mn/Fmt.h>

#include <assert.h>

namespace mn
{
	struct Err
	{
		Str msg;

		Err(): msg(str_new()) {}

		template<typename... TArgs>
		Err(const char* fmt, TArgs&& ... args)
			:msg(strf(fmt, std::forward<TArgs>(args)...))
		{}

		Err(const Err& other)
			:msg(clone(other.msg))
		{}

		Err(Err&& other)
			:msg(other.msg)
		{
			other.msg = str_new();
		}

		~Err() { str_free(msg); }

		Err&
		operator=(const Err& other)
		{
			str_clear(msg);
			str_push(msg, other.msg);
			return *this;
		}

		Err&
		operator=(Err&& other)
		{
			str_free(msg);
			msg = other.msg;
			other.msg = str_new();
			return *this;
		}

		explicit operator bool() const { return msg.count != 0; }
		bool operator==(bool v) const { return (msg.count != 0) == v; }
		bool operator!=(bool v) const { return !operator==(v); }
	};

	template<typename T, typename E = Err>
	struct Result
	{
		static_assert(!std::is_same_v<Err, T>, "Error can't be of the same type as value");

		T val;
		E err;

		Result(E e)
			:err(e)
		{}

		template<typename... TArgs>
		Result(TArgs&& ... args)
			:val(std::forward<TArgs>(args)...),
			 err(E{})
		{}

		Result(const Result&) = delete;

		Result(Result&&) = default;

		Result& operator=(const Result&) = delete;

		Result& operator=(Result&&) = default;

		~Result() = default;
	};

	template<typename T>
	struct Result<T, Err>
	{
		static_assert(!std::is_same_v<Err, T>, "Error can't be of the same type as value");

		T val;
		Err err;

		Result(Err e):err(e) { assert(err.msg.count > 0); }

		template<typename... TArgs>
		Result(TArgs&& ... args)
			:val(std::forward<TArgs>(args)...)
		{}

		Result(const Result&) = delete;

		Result(Result&&) = default;

		Result& operator=(const Result&) = delete;

		Result& operator=(Result&&) = default;

		~Result() = default;

		explicit operator bool() const { return !bool(err); }
		bool operator==(bool v) const { return !bool(err) == v; }
		bool operator!=(bool v) const { return !operator==(v); }
	};
}

namespace fmt
{
	template<>
	struct formatter<mn::Err>
	{
		template<typename ParseContext>
		constexpr auto
		parse(ParseContext &ctx)
		{
			return ctx.begin();
		}

		template<typename FormatContext>
		auto
		format(const mn::Err &err, FormatContext &ctx)
		{
			return format_to(ctx.out(), "{}", err.msg);
		}
	};
} // namespace fmt
