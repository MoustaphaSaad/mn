#include "mn/Debug.h"
#include "mn/IO.h"

#include <cxxabi.h>
#include <execinfo.h>

#include <stdlib.h>

namespace mn
{
	Str
	callstack_dump(Allocator allocator)
	{
        Str str = str_with_allocator(allocator);
		constexpr size_t MAX_NAME_LEN = 1024;
		constexpr size_t STACK_MAX = 4096;
		void* callstack[STACK_MAX];

		//+1 for null terminated string
		char name_buffer[MAX_NAME_LEN+1];
		char demangled_buffer[MAX_NAME_LEN+1];
		size_t demangled_buffer_length = MAX_NAME_LEN;

		//capture the call stack
		size_t frames_count = backtrace(callstack, STACK_MAX);
		//resolve the symbols
        char** symbols = backtrace_symbols(callstack, frames_count);

		if(symbols)
		{
			for(size_t i = 0; i < frames_count; ++i)
			{
				//isolate the function name
				char *name_begin = nullptr, *name_end = nullptr, *name_it = symbols[i];
				while(*name_it != 0)
				{
					if(*name_it == '(')
						name_begin = name_it+1;
					else if(*name_it == ')' || *name_it == '+')
					{
						name_end = name_it;
						break;
					}
					++name_it;
				}

				
				size_t mangled_name_size = name_end - name_begin;
				//function maybe inlined
				if(mangled_name_size == 0)
				{
					str = strf(str, "[{}]: unknown symbol\n", frames_count - i - 1);
					continue;
				}

				//copy the function name into the name buffer
				size_t copy_size = mangled_name_size > MAX_NAME_LEN ? MAX_NAME_LEN : mangled_name_size;
                memcpy(name_buffer, name_begin, copy_size);
				name_buffer[copy_size] = 0;

				int status = 0;
				abi::__cxa_demangle(name_buffer, demangled_buffer, &demangled_buffer_length, &status);
				demangled_buffer[MAX_NAME_LEN] = 0;
				if(status == 0)
					str = strf(str, "[{}]: {}\n", frames_count - i - 1, demangled_buffer);
				else
					str = strf(str, "[{}]: {}\n", frames_count - i - 1, name_buffer);
			}
			::free(symbols);
		}
		return str;
	}
}
