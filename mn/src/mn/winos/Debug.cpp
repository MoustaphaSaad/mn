#include "mn/Debug.h"
#include "mn/IO.h"
#include "mn/RAD.h"

#include <string.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>
#include <DbgHelp.h>

namespace mn
{
	struct Debugger_Callstack
	{
		Debugger_Callstack()
		{
			SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
			SymInitialize(GetCurrentProcess(), NULL, true);
		}

		~Debugger_Callstack()
		{
			SymCleanup(GetCurrentProcess());
		}
	};

	// callstack_capture captures the top frames_count callstack frames and writes it
	// to the given frames pointer
	size_t
	callstack_capture(void** frames, size_t frames_count)
	{
		::memset(frames, 0, frames_count * sizeof(frames));
		return CaptureStackBackTrace(1, (DWORD)frames_count, frames, NULL);
	}

	// callstack_print_to prints the captured callstack to the given stream
	void
	callstack_print_to(void** frames, size_t frames_count, mn::Stream out)
	{
		#if DEBUG
		static Debugger_Callstack _d;

		// gather all the loaded libraries first
		Buf<void*> libs{};
		buf_push(libs, GetCurrentProcess());
		DWORD bytes_needed = 0;
		if (EnumProcessModules(libs[0], NULL, 0, &bytes_needed))
		{
			buf_resize(libs, bytes_needed/sizeof(HMODULE) + libs.count);
			if (EnumProcessModules(libs[0], (HMODULE*)(libs.ptr + 1), bytes_needed, &bytes_needed) == FALSE)
			{
				// reset it back to 1 we have failed
				buf_resize(libs, 1);
			}
		}

		constexpr size_t MAX_NAME_LEN = 256;
		// allocate a buffer for the symbol info
		// windows lays the symbol info in memory in this form
		// [struct][name buffer]
		// and the name buffer size is the same as the MaxNameLen set below
		char buffer[sizeof(SYMBOL_INFO) + MAX_NAME_LEN];

		SYMBOL_INFO* symbol = (SYMBOL_INFO*)buffer;
		::memset(symbol, 0, sizeof(SYMBOL_INFO));
		symbol->MaxNameLen = MAX_NAME_LEN;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

		for(size_t i = 0; i < frames_count; ++i)
		{
			// we have reached the end
			if (frames[i] == nullptr)
				break;

			bool symbol_found = false;
			bool line_found = false;
			IMAGEHLP_LINE64 line{};
			line.SizeOfStruct = sizeof(line);
			DWORD dis = 0;
			for (auto lib: libs)
			{
				if (SymFromAddr(lib, (DWORD64)(frames[i]), NULL, symbol))
				{
					symbol_found = true;
					line_found = SymGetLineFromAddr64(lib, (DWORD64)(frames[i]), &dis, &line);
					break;
				}
			}

			mn::print_to(
				out,
				"[{}]: {}, {}:{}\n",
				frames_count - i - 1,
				symbol_found ? symbol->Name : "UNKNOWN_SYMBOL",
				line_found ? line.FileName : "<NO_FILE_FOUND>",
				line_found ? line.LineNumber : 0UL
			);
		}
		buf_free(libs);
		#endif
	}
}
