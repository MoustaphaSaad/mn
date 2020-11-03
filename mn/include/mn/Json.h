#pragma once

#include "mn/Exports.h"
#include "mn/Str.h"
#include "mn/Buf.h"
#include "mn/Map.h"
#include "mn/Result.h"
#include "mn/Fmt.h"

namespace mn::json
{
	struct Value
	{
		enum KIND: uint8_t
		{
			KIND_NULL,
			KIND_BOOL,
			KIND_NUMBER,
			KIND_STRING,
			KIND_ARRAY,
			KIND_OBJECT,
		};

		KIND kind;
		union
		{
			bool as_bool;
			float as_number;
			Str* as_string;
			Buf<Value>* as_array;
			Map<Str, Value>* as_object;
		};
	};

	inline static Value
	value_bool_new(bool v)
	{
		Value self{};
		self.kind = Value::KIND_BOOL;
		self.as_bool = v;
		return self;
	}

	inline static Value
	value_number_new(float v)
	{
		Value self{};
		self.kind = Value::KIND_NUMBER;
		self.as_number = v;
		return self;
	}

	inline static Value
	value_string_new(Str v)
	{
		Value self{};
		self.kind = Value::KIND_STRING;
		self.as_string = alloc<Str>();
		*self.as_string = v;
		return self;
	}

	inline static Value
	value_string_new(const char* v)
	{
		Value self{};
		self.kind = Value::KIND_STRING;
		self.as_string = alloc<Str>();
		*self.as_string = str_from_c(v);
		return self;
	}

	inline static Value
	value_array_new()
	{
		Value self{};
		self.kind = Value::KIND_ARRAY;
		self.as_array = alloc_zerod<Buf<Value>>();
		return self;
	}

	inline static Value
	value_object_new()
	{
		Value self{};
		self.kind = Value::KIND_OBJECT;
		self.as_object = alloc_zerod<Map<Str, Value>>();
		return self;
	}

	inline static void
	value_free(Value& self)
	{
		switch(self.kind)
		{
		case Value::KIND_NULL:
		case Value::KIND_BOOL:
		case Value::KIND_NUMBER:
			break;
		case Value::KIND_STRING:
			str_free(*self.as_string);
			free(self.as_string);
			break;
		case Value::KIND_ARRAY:
			destruct(*self.as_array);
			free(self.as_array);
			break;
		case Value::KIND_OBJECT:
			destruct(*self.as_object);
			free(self.as_object);
			break;
		default:
			assert(false && "unreachable");
			break;
		}
	}

	inline static void
	destruct(Value& self)
	{
		value_free(self);
	}

	inline static const Value&
	value_array_at(const Value& self, size_t index)
	{
		return *(self.as_array->ptr + index);
	}

	inline static Value&
	value_array_at(Value& self, size_t index)
	{
		return *(self.as_array->ptr + index);
	}

	inline static void
	value_array_push(Value& self, Value v)
	{
		buf_push(*self.as_array, v);
	}

	inline static Buf<Value>&
	value_array_iter(Value& self)
	{
		return *self.as_array;
	}

	inline static const Value*
	value_object_lookup(const Value& self, const Str& key)
	{
		if (auto it = map_lookup(*self.as_object, key))
			return &it->value;
		return nullptr;
	}

	inline static Value*
	value_object_lookup(Value& self, const Str& key)
	{
		if (auto it = map_lookup(*self.as_object, key))
			return &it->value;
		return nullptr;
	}

	inline static void
	value_object_insert(Value& self, const Str& key, Value v)
	{
		if (auto it = map_lookup(*self.as_object, key))
		{
			value_free(it->value);
			it->value = v;
		}
		else
		{
			map_insert(*self.as_object, clone(key), v);
		}
	}

	inline static void
	value_object_insert(Value& self, const char* key, Value v)
	{
		map_insert(*self.as_object, str_from_c(key), v);
	}

	inline static Map<Str, Value>&
	value_object_iter(Value& self)
	{
		return *self.as_object;
	}

	MN_EXPORT Result<Value>
	parse(const Str& content);

	inline static Result<Value>
	parse(const char* content)
	{
		return parse(str_lit(content));
	}
}

namespace fmt
{
	template<>
	struct formatter<mn::json::Value> {
		template <typename ParseContext>
		constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

		template <typename FormatContext>
		auto format(const mn::json::Value &v, FormatContext &ctx) {
			switch(v.kind)
			{
			case mn::json::Value::KIND_NULL:
				format_to(ctx.out(), "null");
				break;
			case mn::json::Value::KIND_BOOL:
				format_to(ctx.out(), "{}", v.as_bool ? "true" : "false");
				break;
			case mn::json::Value::KIND_NUMBER:
				format_to(ctx.out(), "{}", v.as_number);
				break;
			case mn::json::Value::KIND_STRING:
				format_to(ctx.out(), "\"{}\"", *v.as_string);
				break;
			case mn::json::Value::KIND_ARRAY:
				format_to(ctx.out(), "[");
				for(size_t i = 0; i < v.as_array->count; ++i)
				{
					if (i != 0)
						format_to(ctx.out(), ", ");
					format_to(ctx.out(), "{}", (*v.as_array)[i]);
				}
				format_to(ctx.out(), "]");
				break;
			case mn::json::Value::KIND_OBJECT:
			{
				format_to(ctx.out(), "{{");
				size_t i = 0;
				for (const auto& [key, value]: *v.as_object)
				{
					if (i != 0)
						format_to(ctx.out(), ", ");
					format_to(ctx.out(), "\"{}\":{}", key, value);
					++i;
				}
				format_to(ctx.out(), "}}");
				break;
			}
			default:
				assert(false && "unreachable");
				break;
			}
			return ctx.out();
		}
	};
}