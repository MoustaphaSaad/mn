#include "mn/Json.h"

namespace mn::json
{
	struct Token
	{
		enum KIND
		{
			KIND_NONE,
			KIND_OPEN_CURLY,
			KIND_CLOSE_CURLY,
			KIND_OPEN_BRACKET,
			KIND_CLOSE_BRACKET,
			KIND_COMMA,
			KIND_COLON,
			KIND_BOOL,
			KIND_NUMBER,
			KIND_STRING,
			KIND_NULL,
		};

		inline operator bool() const { return kind != KIND_NONE; }

		KIND kind;
		const char *begin, *end;
		union
		{
			bool val_bool;
			double val_num;
		};
	};

	inline static const char *
	_json_token_kind_str(Token::KIND kind)
	{
		switch (kind)
		{
		case Token::KIND_OPEN_CURLY:
			return "{";
		case Token::KIND_CLOSE_CURLY:
			return "}";
		case Token::KIND_OPEN_BRACKET:
			return "[";
		case Token::KIND_CLOSE_BRACKET:
			return "]";
		case Token::KIND_COMMA:
			return ":";
		case Token::KIND_COLON:
			return ",";
		case Token::KIND_BOOL:
			return "bool";
		case Token::KIND_NUMBER:
			return "number";
		case Token::KIND_STRING:
			return "string";
		case Token::KIND_NULL:
			return "null";
		case Token::KIND_NONE:
		default:
			return "unidentified";
		}
	}

	struct Lexer
	{
		const char *it = nullptr;
		char c = '\0';

		Err err;
	};

	inline static bool
	_lexer_eof(Lexer &self)
	{
		return self.c == 0;
	}

	inline static bool
	_lexer_is_ws(char c)
	{
		return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
	}

	inline static bool
	_lexer_read_rune(Lexer &self)
	{
		if (_lexer_eof(self))
			return false;

		++self.it;
		self.c = *self.it;

		return true;
	}

	inline static void
	_lexer_skip_ws(Lexer &self)
	{
		while (_lexer_is_ws(self.c))
			if (_lexer_read_rune(self) == false)
				break;
	}

	inline static bool
	_lexer_is_letter(char c)
	{
		// 0x80 because all the keywords in json is ascii not utf-8
		// if you have utf-8 runes then you'll have to provide a utf-8 is letter function
		// in the else branch
		if (c < 0x80)
			return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
		return false;
	}

	inline static bool
	_lexer_is_digit(char c)
	{
		return (c >= '0' && c <= '9');
	}

	inline static void
	_lexer_scan_id(Lexer &self, Token &tkn)
	{
		const char *old_it = self.it;
		while (_lexer_is_letter(self.c))
			if (_lexer_read_rune(self) == false)
				break;
		tkn.begin = old_it;
		tkn.end	  = self.it;
	}

	inline static void
	_lexer_scan_str(Lexer &self, Token &tkn)
	{
		tkn.begin = self.it;

		char prev = self.c;
		// eat all runes even those escaped by \ like \"
		while (self.c != '"' || prev == '\\')
		{
			prev = self.c;
			if (_lexer_read_rune(self) == false)
			{
				self.err = Err{"unexpected end of string '{:.{}s}'", tkn.begin, self.it - tkn.begin};
				break;
			}
		}

		tkn.end = self.it;
		_lexer_read_rune(self); // for the "
	}

	inline static Token
	_lexer_lex(Lexer &self)
	{
		_lexer_skip_ws(self);

		Token tkn{};

		if (_lexer_eof(self))
			return tkn;

		tkn.begin = self.it;

		if (_lexer_is_letter(self.c))
		{
			_lexer_scan_id(self, tkn);

			switch(tkn.end - tkn.begin)
			{
			case 4:
				if (strncmp(tkn.begin, "null", 4) == 0)
				{
					tkn.kind = Token::KIND_NULL;
				}
				else if (strncmp(tkn.begin, "true", 4) == 0)
				{
					tkn.kind = Token::KIND_BOOL;
					tkn.val_bool = true;
				}
				break;
			case 5:
				if (strncmp(tkn.begin, "false", 5) == 0)
				{
					tkn.kind = Token::KIND_BOOL;
					tkn.val_bool = false;
				}
				break;
			default:
				self.err = Err{"unidentified keyword '{:.{}s}'", tkn.begin, tkn.end - tkn.begin};
				break;
			}
		}
		else if (_lexer_is_digit(self.c) || self.c == '-' || self.c == '+')
		{
			char *end	= nullptr;
			tkn.kind	= Token::KIND_NUMBER;
			tkn.val_num = ::strtod(self.it, &end);
			if (errno == ERANGE)
			{
				self.err = Err{"number out of range '{:.{}s}'", tkn.begin, end - tkn.begin};
			}

			self.it = end;
			self.c	= *self.it;
		}
		else
		{
			int32_t c = self.c;
			_lexer_read_rune(self);

			switch (c)
			{
			case '"':
				tkn.kind = Token::KIND_STRING;
				_lexer_scan_str(self, tkn);
				break;

			case ':':
				tkn.kind = Token::KIND_COLON;
				tkn.end	 = self.it;
				break;

			case ',':
				tkn.kind = Token::KIND_COMMA;
				tkn.end	 = self.it;
				break;

			case '{':
				tkn.kind = Token::KIND_OPEN_CURLY;
				tkn.end	 = self.it;
				break;

			case '}':
				tkn.kind = Token::KIND_CLOSE_CURLY;
				tkn.end	 = self.it;
				break;

			case '[':
				tkn.kind = Token::KIND_OPEN_BRACKET;
				tkn.end	 = self.it;
				break;

			case ']':
				tkn.kind = Token::KIND_CLOSE_BRACKET;
				tkn.end	 = self.it;
				break;

			default:
				self.err = Err{"unidentified rune '{:c}'", c};
				break;
			}
		}
		return tkn;
	}

	struct Parser
	{
		Lexer lexer;
		Token current;
		Err err;
	};

	inline static Token
	_parser_look(Parser& self)
	{
		return self.current;
	}

	inline static Token
	_parser_look_kind(Parser& self, Token::KIND k)
	{
		if (self.current.kind == k)
			return self.current;
		return Token{};
	}

	inline static Token
	_parser_eat(Parser& self)
	{
		auto res = self.current;
		self.current = _lexer_lex(self.lexer);
		return res;
	}

	inline static Token
	_parser_eat_kind(Parser& self, Token::KIND k)
	{
		auto res = self.current;
		if (res.kind != k)
			return Token{};
		self.current = _lexer_lex(self.lexer);
		if (self.lexer.err && !self.err)
			self.err = self.lexer.err;
		return res;
	}

	inline static Token
	_parser_eat_must(Parser& self, Token::KIND k)
	{
		if (self.current.kind == Token::KIND_NONE && _lexer_eof(self.lexer))
		{
			self.err = Err{"expected '{}' but found EOF", _json_token_kind_str(self.current.kind)};
			return Token{};
		}

		auto res = self.current;
		self.current = _lexer_lex(self.lexer);
		if (self.lexer.err && !self.err)
			self.err = self.lexer.err;
		if (res.kind == k)
			return res;

		self.err = Err{
			"expected '{}' but found '{:.{}s}'",
			_json_token_kind_str(k),
			res.begin,
			res.end - res.begin
		};
		return Token{};
	}

	inline static Value
	_parser_parse_value(Parser &self)
	{
		if (auto tkn = _parser_eat_kind(self, Token::KIND_NULL))
		{
			return Value{};
		}
		else if (auto tkn = _parser_eat_kind(self, Token::KIND_BOOL))
		{
			return value_bool_new(tkn.val_bool);
		}
		else if (auto tkn = _parser_eat_kind(self, Token::KIND_NUMBER))
		{
			return value_number_new(tkn.val_num);
		}
		else if (auto tkn = _parser_eat_kind(self, Token::KIND_STRING))
		{
			return value_string_new(str_from_substr(tkn.begin, tkn.end));
		}
		else if (auto tkn = _parser_eat_kind(self, Token::KIND_OPEN_BRACKET))
		{
			auto array = value_array_new();
			while (_parser_look_kind(self, Token::KIND_CLOSE_BRACKET) == false)
			{
				if (auto value = _parser_parse_value(self); !self.err)
				{
					value_array_push(array, value);
				}
				else
				{
					value_free(array);
					return Value{};
				}

				if (_parser_eat_kind(self, Token::KIND_COMMA) == false)
					break;
			}
			_parser_eat_must(self, Token::KIND_CLOSE_BRACKET);
			return array;
		}
		else if (auto tkn = _parser_eat_kind(self, Token::KIND_OPEN_CURLY))
		{
			auto object = value_object_new();

			while (_parser_look_kind(self, Token::KIND_CLOSE_CURLY) == false)
			{
				auto key = _parser_eat_must(self, Token::KIND_STRING);
				_parser_eat_must(self, Token::KIND_COLON);

				if (auto value = _parser_parse_value(self); !self.err)
				{
					auto key_str = str_from_substr(key.begin, key.end);
					if (auto it = map_lookup(*object.as_object, key_str))
					{
						str_free(key_str);
						value_free(it->value);
						it->value = value;
					}
					else
					{
						map_insert(*object.as_object, key_str, value);
					}
				}
				else
				{
					value_free(object);
					return Value{};
				}

				if (_parser_eat_kind(self, Token::KIND_COMMA) == false)
					break;
			}
			_parser_eat_must(self, Token::KIND_CLOSE_CURLY);
			return object;
		}
		else if (auto tkn = _parser_eat(self))
		{
			self.err = Err{
				"unidentified token '{:.{}s}' of kind '{}'",
				tkn.begin,
				tkn.end - tkn.begin,
				_json_token_kind_str(tkn.kind)
			};
		}

		return Value{};
	}

	// API
	Result<Value>
	parse(const Str& content)
	{
		Lexer lexer;
		lexer.it = content.ptr;
		lexer.c	= *lexer.it;

		Parser parser;
		parser.lexer = lexer;
		parser.current = _lexer_lex(parser.lexer);

		auto res = _parser_parse_value(parser);
		if (parser.err)
			return parser.err;
		return res;
	}
}