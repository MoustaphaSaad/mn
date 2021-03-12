#include "mn/Regex.h"
#include "mn/Defer.h"

namespace mn
{
	// regex_compiler
	union Int
	{
		int oint;
		uint8_t obytes[sizeof(int)];
	};

	inline static void
	push_op(Regex& program, RGX_OP op)
	{
		buf_push(program.bytes, op);
	}

	inline static void
	push_int(Regex& program, int offset)
	{
		Int v{};
		v.oint = offset;
		for (size_t i = 0; i < sizeof(int); ++i)
			buf_push(program.bytes, v.obytes[i]);
	}

	inline static void
	push_rune(Regex& program, Rune c)
	{
		push_int(program, c);
	}

	inline static void
	push_program(Regex& program, const Regex& other)
	{
		buf_concat(program.bytes, other.bytes);
	}

	inline static void
	patch_int_at(Regex& program, size_t index, int offset)
	{
		Int v{};
		v.oint = offset;
		for (size_t i = 0; i < sizeof(int); ++i)
			program.bytes[i + index] = v.obytes[i];
	}

	enum REGEX_OPERATOR
	{
		REGEX_OPERATOR_CLOSE_PAREN,
		REGEX_OPERATOR_OPEN_PAREN,
		REGEX_OPERATOR_OR,
		REGEX_OPERATOR_CONCAT,
		REGEX_OPERATOR_PLUS,
		REGEX_OPERATOR_PLUS_NON_GREEDY,
		REGEX_OPERATOR_STAR,
		REGEX_OPERATOR_STAR_NON_GREEDY,
		REGEX_OPERATOR_OPTIONAL,
		REGEX_OPERATOR_OPTIONAL_NON_GREEDY,
	};

	struct Regex_Compiler
	{
		Str str;
		const char* it;
		Buf<Regex> operands_stack;
		Buf<REGEX_OPERATOR> operators_stack;
		bool recommend_concat;
		bool ignore;
	};

	inline static Regex_Compiler
	regex_compiler_new(const Str& str)
	{
		Regex_Compiler self{};
		self.str = str;
		self.it = str.ptr;
		self.operands_stack = buf_new<Regex>();
		self.operators_stack = buf_new<REGEX_OPERATOR>();
		self.recommend_concat = false;
		self.ignore = false;
		return self;
	}

	inline static void
	regex_compiler_free(Regex_Compiler& self)
	{
		destruct(self.operands_stack);
		buf_free(self.operators_stack);
	}

	inline static bool
	regex_compiler_eof(const Regex_Compiler& self)
	{
		return self.it >= end(self.str);
	}

	inline static void
	regex_compiler_eat_rune(Regex_Compiler& compiler)
	{
		compiler.it = rune_next(compiler.it);
	}

	inline static Rune
	regex_compiler_current_rune(const Regex_Compiler& compiler)
	{
		return rune_read(compiler.it);
	}

	inline static Rune
	regex_compiler_next_rune(const Regex_Compiler& compiler)
	{
		auto it = compiler.it;
		if (it >= end(compiler.str))
			return Rune{};

		it = rune_next(compiler.it);
		if (it >= end(compiler.str))
			return Rune{};

		return rune_read(it);
	}

	inline static bool
	regex_compiler_concat(Regex_Compiler& compiler)
	{
		if (compiler.operands_stack.count < 2)
			return false;

		auto B = buf_top(compiler.operands_stack);
		buf_pop(compiler.operands_stack);
		mn_defer(regex_free(B));

		auto& A = buf_top(compiler.operands_stack);
		push_program(A, B);

		return true;
	}

	inline static bool
	regex_compiler_or(Regex_Compiler& compiler)
	{
		if (compiler.operands_stack.count < 2)
			return false;

		auto B = buf_top(compiler.operands_stack);
		buf_pop(compiler.operands_stack);
		mn_defer(regex_free(B));

		auto A = buf_top(compiler.operands_stack);
		buf_pop(compiler.operands_stack);
		mn_defer(regex_free(A));

		auto C = regex_new();
		buf_reserve(C.bytes, B.bytes.count + A.bytes.count + 14);

		push_op(C, RGX_OP_SPLIT);
		push_int(C, 0);
		push_int(C, (int)A.bytes.count + 5); // +5 for the jump instruction
		push_program(C, A);
		push_op(C, RGX_OP_JUMP);
		push_int(C, (int)B.bytes.count);
		push_program(C, B);

		buf_push(compiler.operands_stack, C);
		return true;
	}

	inline static bool
	regex_compiler_star(Regex_Compiler& compiler, bool greedy)
	{
		if (compiler.operands_stack.count < 1)
			return false;

		auto A = buf_top(compiler.operands_stack);
		buf_pop(compiler.operands_stack);
		mn_defer(regex_free(A));

		auto C = regex_new();
		buf_reserve(C.bytes, A.bytes.count + 14);

		push_op(C, RGX_OP_SPLIT);
		if (greedy)
		{
			push_int(C, 0);
			push_int(C, (int)A.bytes.count + 5); // +5 for the jump instruction
		}
		else
		{
			push_int(C, (int)A.bytes.count + 5); // +5 for the jump instruction
			push_int(C, 0);
		}
		push_program(C, A);
		push_op(C, RGX_OP_JUMP);
		push_int(C, -1 * ((int)A.bytes.count + 14));

		buf_push(compiler.operands_stack, C);
		return true;
	}

	inline static bool
	regex_compiler_plus(Regex_Compiler& compiler, bool greedy)
	{
		if (compiler.operands_stack.count < 1)
			return false;

		auto& A = buf_top(compiler.operands_stack);
		int offset = -1 * ((int)A.bytes.count + 9); // +9 for the split instruction

		push_op(A, RGX_OP_SPLIT);
		if (greedy)
		{
			push_int(A, offset);
			push_int(A, 0);
		}
		else
		{
			push_int(A, 0);
			push_int(A, offset);
		}

		return true;
	}

	inline static bool
	regex_compiler_optional(Regex_Compiler& compiler, bool greedy)
	{
		if (compiler.operands_stack.count < 1)
			return false;

		auto A = buf_top(compiler.operands_stack);
		buf_pop(compiler.operands_stack);
		mn_defer(regex_free(A));

		auto C = regex_new();
		buf_reserve(C.bytes, A.bytes.count + 9); // +9 for the split instruction

		push_op(C, RGX_OP_SPLIT);
		if (greedy)
		{
			push_int(C, 0);
			push_int(C, (int)A.bytes.count);
		}
		else
		{
			push_int(C, (int)A.bytes.count);
			push_int(C, 0);
		}
		push_program(C, A);

		buf_push(compiler.operands_stack, C);
		return true;
	}

	inline static bool
	regex_compiler_eval(Regex_Compiler& compiler)
	{
		if (compiler.operators_stack.count == 0)
			return false;

		auto op = buf_top(compiler.operators_stack);
		buf_pop(compiler.operators_stack);

		switch(op)
		{
		case REGEX_OPERATOR_CONCAT: return regex_compiler_concat(compiler);
		case REGEX_OPERATOR_OR: return regex_compiler_or(compiler);
		case REGEX_OPERATOR_STAR: return regex_compiler_star(compiler, true);
		case REGEX_OPERATOR_STAR_NON_GREEDY: return regex_compiler_star(compiler, false);
		case REGEX_OPERATOR_PLUS: return regex_compiler_plus(compiler, true);
		case REGEX_OPERATOR_PLUS_NON_GREEDY: return regex_compiler_plus(compiler, false);
		case REGEX_OPERATOR_OPTIONAL: return regex_compiler_optional(compiler, true);
		case REGEX_OPERATOR_OPTIONAL_NON_GREEDY: return regex_compiler_optional(compiler, false);
		default: assert(false && "unreachable"); return false;
		}
	}

	inline static bool
	regex_compiler_push_operator(Regex_Compiler& compiler, REGEX_OPERATOR op)
	{
		while (true)
		{
			if (compiler.operators_stack.count == 0)
				break;

			if (buf_top(compiler.operators_stack) < op)
				break;

			if (regex_compiler_eval(compiler) == false)
				return false;
		}
		buf_push(compiler.operators_stack, op);
		return true;
	}

	inline static bool
	regex_compiler_handle_concat(Regex_Compiler& compiler)
	{
		if (compiler.recommend_concat)
		{
			if (regex_compiler_push_operator(compiler, REGEX_OPERATOR_CONCAT) == false)
				return false;
			compiler.recommend_concat = false;
		}
		return true;
	}

	inline static bool
	regex_compiler_is_operator(char c)
	{
		return (
			c == '\\' ||
			c == '|' ||
			c == '*' ||
			c == '+' ||
			c == '?' ||
			c == '.' ||
			c == '(' ||
			c == ')' ||
			c == '[' ||
			c == ']'
		);
	}

	inline static bool
	regex_compiler_process_rune(Regex_Compiler& compiler)
	{
		if (regex_compiler_eof(compiler))
			return false;

		auto c = regex_compiler_current_rune(compiler);

		if (c == '\\' && compiler.ignore == false)
		{
			compiler.ignore = true;
		}
		else if (c == '|' && compiler.ignore == false)
		{
			if (regex_compiler_push_operator(compiler, REGEX_OPERATOR_OR) == false)
				return false;
			compiler.ignore = false;
			compiler.recommend_concat = false;
		}
		else if (c == '*' && compiler.ignore == false)
		{
			auto op = REGEX_OPERATOR_STAR;
			if (regex_compiler_next_rune(compiler) == '?')
			{
				regex_compiler_eat_rune(compiler);
				op = REGEX_OPERATOR_STAR_NON_GREEDY;
			}

			if (regex_compiler_push_operator(compiler, op) == false)
				return false;

			compiler.ignore = false;
			compiler.recommend_concat = true;
		}
		else if (c == '+' && compiler.ignore == false)
		{
			auto op = REGEX_OPERATOR_PLUS;
			if (regex_compiler_next_rune(compiler) == '?')
			{
				regex_compiler_eat_rune(compiler);
				op = REGEX_OPERATOR_PLUS_NON_GREEDY;
			}

			if (regex_compiler_push_operator(compiler, op) == false)
				return false;

			compiler.ignore = false;
			compiler.recommend_concat = true;
		}
		else if (c == '?' && compiler.ignore == false)
		{
			auto op = REGEX_OPERATOR_OPTIONAL;
			if (regex_compiler_next_rune(compiler) == '?')
			{
				regex_compiler_eat_rune(compiler);
				op = REGEX_OPERATOR_OPTIONAL_NON_GREEDY;
			}

			if (regex_compiler_push_operator(compiler, op) == false)
				return false;

			compiler.ignore = false;
			compiler.recommend_concat = true;
		}
		else if (c == '.' && compiler.ignore == false)
		{
			if (regex_compiler_handle_concat(compiler) == false)
				return false;

			auto fragment = regex_new();
			push_op(fragment, RGX_OP_ANY);
			buf_push(compiler.operands_stack, fragment);
			compiler.ignore = false;
			compiler.recommend_concat = true;
		}
		else if (c == '(' && compiler.ignore == false)
		{
			if (regex_compiler_handle_concat(compiler) == false)
				return false;

			buf_push(compiler.operators_stack, REGEX_OPERATOR_OPEN_PAREN);

			compiler.ignore = false;
			compiler.recommend_concat = false;
		}
		else if (c == ')' && compiler.ignore == false)
		{
			while (compiler.operators_stack.count > 0 && buf_top(compiler.operators_stack) != REGEX_OPERATOR_OPEN_PAREN)
				if (regex_compiler_eval(compiler) == false)
					return false;

			buf_pop(compiler.operators_stack);
			compiler.ignore = false;
			compiler.recommend_concat = true;
		}
		else if (c == '[' && compiler.ignore == false)
		{
			if (regex_compiler_handle_concat(compiler) == false)
				return false;

			auto op = RGX_OP_SET;
			if (regex_compiler_next_rune(compiler) == '^')
			{
				regex_compiler_eat_rune(compiler);
				op = RGX_OP_NOT_SET;
			}

			auto C = regex_new();
			push_op(C, op);
			push_int(C, 0);

			bool local_ignore = false;
			auto prev_c = c;
			auto options_start = C.bytes.count;
			for (regex_compiler_eat_rune(compiler); regex_compiler_eof(compiler) == false; regex_compiler_eat_rune(compiler))
			{
				c = regex_compiler_current_rune(compiler);
				if (c == '\\' && local_ignore == false)
				{
					local_ignore = true;
				}
				else if (c == ']' && local_ignore == false)
				{
					break;
				}
				else if (c == '-' && local_ignore == false)
				{
					// pop the rune op
					buf_pop(C.bytes);
					buf_pop(C.bytes);
					buf_pop(C.bytes);
					buf_pop(C.bytes);
					buf_pop(C.bytes);

					regex_compiler_eat_rune(compiler);

					if (regex_compiler_eof(compiler))
					{
						regex_free(C);
						return false;
					}

					auto next_c = regex_compiler_current_rune(compiler);
					if (next_c < prev_c)
					{
						regex_free(C);
						return false;
					}

					push_op(C, RGX_OP_RANGE);
					push_rune(C, prev_c);
					push_rune(C, next_c);
					local_ignore = false;
				}
				else
				{
					push_op(C, RGX_OP_RUNE);
					push_rune(C, c);
					local_ignore = false;
				}

				prev_c = c;
			}

			if (c != ']')
			{
				regex_free(C);
				return false;
			}

			auto options_end = C.bytes.count;
			patch_int_at(C, 1, (int)(options_end - options_start));
			compiler.ignore = false;
			compiler.recommend_concat = true;
			buf_push(compiler.operands_stack, C);
		}
		else
		{
			// no regex operator support yet
			if (regex_compiler_handle_concat(compiler) == false)
				return false;

			auto fragment = regex_new();
			push_op(fragment, RGX_OP_RUNE);
			push_rune(fragment, c);
			buf_push(compiler.operands_stack, fragment);
			compiler.ignore = false;
			compiler.recommend_concat = true;
		}

		regex_compiler_eat_rune(compiler);
		return true;
	}


	// vm part
	struct Regex_Thread
	{
		size_t id;
		size_t ip;
	};

	inline static size_t
	thread_depth_level(size_t id)
	{
		return (size_t)::log2(id);
	}

	inline static size_t
	thread_parent(size_t id)
	{
		return (id - 1) / 2;
	}

	inline static bool
	thread_should_update_result(size_t new_res_thread_id, size_t old_res_thread_id)
	{
		if (old_res_thread_id == SIZE_MAX)
			return true;

		auto new_res_thread_depth_level = thread_depth_level(new_res_thread_id);
		auto old_res_thread_depth_level = thread_depth_level(old_res_thread_id);

		while (new_res_thread_depth_level < old_res_thread_depth_level)
		{
			old_res_thread_id = thread_parent(old_res_thread_id);
			--old_res_thread_depth_level;
		}

		while (new_res_thread_depth_level > old_res_thread_depth_level)
		{
			new_res_thread_id = thread_parent(new_res_thread_id);
			--new_res_thread_depth_level;
		}

		assert(new_res_thread_depth_level == old_res_thread_depth_level);
		return new_res_thread_id < old_res_thread_id;
	}

	inline static RGX_OP
	pop_op(const Regex& program, Regex_Thread& thread)
	{
		auto res = program.bytes[thread.ip];
		++thread.ip;
		return (RGX_OP)res;
	}

	inline static int
	pop_int(const Regex& program, Regex_Thread& thread)
	{
		assert(thread.ip + sizeof(int) <= program.bytes.count);
		int res = *(int*)(program.bytes.ptr + thread.ip);
		thread.ip += sizeof(int);
		return res;
	}

	inline static mn::Rune
	pop_rune(const Regex& program, Regex_Thread& thread)
	{
		return pop_int(program, thread);
	}

	// API
	Result<Regex>
	regex_compile(Regex_Compile_Unit unit)
	{
		auto compiler = regex_compiler_new(unit.str);
		mn_defer(regex_compiler_free(compiler));

		while (regex_compiler_eof(compiler) == false)
		{
			auto ok = regex_compiler_process_rune(compiler);
			if (ok == false)
				return Err{ "can't process rune at offset {}", compiler.it - begin(compiler.str) };
		}

		while (compiler.operators_stack.count > 0)
			if (regex_compiler_eval(compiler) == false)
				return Err{ "failed to process regex operator" };

		if (compiler.operands_stack.count != 1)
			return Err { "no operands in the stack!" };

		auto last_fragment = buf_top(compiler.operands_stack);
		buf_pop(compiler.operands_stack);
		mn_defer(regex_free(last_fragment));
		if (unit.enable_payload)
		{
			push_op(last_fragment, RGX_OP_MATCH2);
			push_int(last_fragment, unit.payload);
		}
		else
		{
			push_op(last_fragment, RGX_OP_MATCH);
		}

		Regex res{};
		res.bytes = buf_memcpy_clone(last_fragment.bytes, unit.program_allocator);
		return res;
	}

	Match_Result
	regex_match(const Regex& program, const char* str)
	{
		auto current_threads = mn::buf_with_allocator<Regex_Thread>(mn::memory::tmp());
		auto new_threads = mn::buf_with_allocator<Regex_Thread>(mn::memory::tmp());
		auto new_thread_set = mn::set_with_allocator<size_t>(mn::memory::tmp());
		Match_Result res{str, str, false, false, 0};
		size_t res_thread_id = SIZE_MAX;

		mn::buf_push(current_threads, Regex_Thread{});

		auto it = str;
		for (it = str;; it = mn::rune_next(it))
		{
			if (current_threads.count == 0)
				break;

			auto str_c = mn::rune_read(it);

			for (size_t i = 0; i < current_threads.count; ++i)
			{
				auto& thread = current_threads[i];
				auto op = pop_op(program, thread);
				switch(op)
				{
				case RGX_OP_RUNE:
				{
					auto c = pop_rune(program, thread);
					if(str_c != c)
						break;
					if (mn::set_lookup(new_thread_set, thread.ip) == nullptr)
					{
						mn::buf_push(new_threads, thread);
						mn::set_insert(new_thread_set, thread.ip);
					}
					break;
				}
				case RGX_OP_ANY:
				{
					if (str_c == 0)
						break;
					if (mn::set_lookup(new_thread_set, thread.ip) == nullptr)
					{
						mn::buf_push(new_threads, thread);
						mn::set_insert(new_thread_set, thread.ip);
					}
					break;
				}
				case RGX_OP_SPLIT:
				{
					auto offset_1 = pop_int(program, thread);
					auto offset_2 = pop_int(program, thread);
					mn::buf_push(current_threads, Regex_Thread{ thread.id * 2 + 1, thread.ip + offset_1 });
					mn::buf_push(current_threads, Regex_Thread{ thread.id * 2 + 2, thread.ip + offset_2 });
					break;
				}
				case RGX_OP_JUMP:
				{
					auto offset = pop_int(program, thread);
					thread.ip += offset;
					mn::buf_push(current_threads, thread);
					break;
				}
				case RGX_OP_SET:
				case RGX_OP_NOT_SET:
				{
					auto options_end_offset = pop_int(program, thread);
					auto options_end = thread.ip + options_end_offset;
					bool inside_set = false;
					while (thread.ip < options_end)
					{
						if (inside_set == true)
						{
							thread.ip = options_end;
							break;
						}

						auto local_op = pop_op(program, thread);
						switch (local_op)
						{
						case RGX_OP_RANGE:
						{
							auto a = pop_rune(program, thread);
							auto z = pop_rune(program, thread);
							inside_set |= (str_c >= a && str_c <= z);
							break;
						}
						case RGX_OP_RUNE:
						{
							auto c = pop_rune(program, thread);
							inside_set |= str_c == c;
							break;
						}
						default:
							assert(false && "unreachable");
							break;
						}
					}

					if ((op == RGX_OP_SET && inside_set) ||
						(op == RGX_OP_NOT_SET && inside_set == false))
					{
						if (mn::set_lookup(new_thread_set, thread.ip) == nullptr)
						{
							mn::buf_push(new_threads, thread);
							mn::set_insert(new_thread_set, thread.ip);
						}
					}
					break;
				}
				case RGX_OP_MATCH:
					if (thread_should_update_result(thread.id, res_thread_id))
					{
						res = Match_Result { str, it, true, false, 0 };
						res_thread_id = thread.id;
					}
					break;
				case RGX_OP_MATCH2:
				{
					auto payload = pop_int(program, thread);
					if (thread_should_update_result(thread.id, res_thread_id))
					{
						res = Match_Result {};
						res.begin = str;
						res.end = it;
						res.match = true;
						res.with_payload = true;
						res.payload = payload;
						res_thread_id = thread.id;
					}
					break;
				}
				default:
					assert(false && "unknown opcode");
					return Match_Result { str, it, false, false, 0};
				}
			}
			auto tmp = new_threads;
			new_threads = current_threads;
			current_threads = tmp;
			mn::buf_clear(new_threads);
			mn::set_clear(new_thread_set);
			if (str_c == '\0')
				break;
		}

		if (res.match == false)
			res.end = it;

		return res;
	}

	Match_Result
	regex_search(const Regex& program, const char* str)
	{
		auto it = str;
		while(*it)
		{
			auto res = regex_match(program, it);
			if (res.match)
				return res;
			if (res.end != it)
				it = res.end;
			else
				it = mn::rune_next(it);
		}
		return Match_Result{str, it, false, false, 0};
	}
}