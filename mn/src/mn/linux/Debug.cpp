#include "mn/Debug.h"
#include "mn/IO.h"

#include <cxxabi.h>
#include <execinfo.h>

#include <stdlib.h>

namespace mn
{
	size_t
	callstack_capture(void** frames, size_t frames_count)
	{
		::memset(frames, 0, frames_count * sizeof(frames));
		return backtrace(frames, frames_count);
	}

	void
	callstack_print_to(void** frames, size_t frames_count, mn::Stream out)
	{
		#if DEBUG
		constexpr size_t MAX_NAME_LEN = 255;
		//+1 for null terminated string
		char name_buffer[MAX_NAME_LEN+1];
		char** symbols = backtrace_symbols(frames, frames_count);
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
					mn::print_to(out, "[{}]: {}\n", frames_count - i - 1, symbols[i]);
					continue;
				}

				//copy the function name into the name buffer
				size_t copy_size = mangled_name_size > MAX_NAME_LEN ? MAX_NAME_LEN : mangled_name_size;
				memcpy(name_buffer, name_begin, copy_size);
				name_buffer[copy_size] = 0;

				int status = 0;
				char* demangled_name = abi::__cxa_demangle(name_buffer, NULL, 0, &status);

				if(status == 0)
					mn::print_to(out, "[{}]: {}\n", frames_count - i - 1, demangled_name);
				else
					mn::print_to(out, "[{}]: {}\n", frames_count - i - 1, name_buffer);

				::free(demangled_name);
			}
			::free(symbols);
		}
		#endif
	}
}
