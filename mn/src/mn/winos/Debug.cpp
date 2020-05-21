#include "mn/Debug.h"
#include "mn/IO.h"

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
		return CaptureStackBackTrace(1, frames_count, frames, NULL);
	}

	// callstack_print_to prints the captured callstack to the given stream
	void
	callstack_print_to(void** frames, size_t frames_count, mn::Stream out)
	{
		#if DEBUG
		static Debugger_Callstack _d;

		auto process_handle = GetCurrentProcess();

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

			if (SymFromAddr(process_handle, (DWORD64)(frames[i]), NULL, symbol))
			{
				IMAGEHLP_LINE64 line{};
				line.SizeOfStruct = sizeof(line);
				DWORD dis = 0;
				BOOL line_found = SymGetLineFromAddr64(process_handle, symbol->Address, &dis, &line);

				mn::print_to(
					out,
					"[{}]: {}, {}:{}\n",
					frames_count - i - 1,
					symbol->Name,
					line_found ? line.FileName : "<NO_FILE_FOUND>",
					line_found ? line.LineNumber + 1 : 0UL
				);
			}
			else
			{
				IMAGEHLP_LINE64 line;
				DWORD dis = 0;
				BOOL line_found = SymGetLineFromAddr64(process_handle, symbol->Address, &dis, &line);

				mn::print_to(
					out,
					"[{}]: {}, {}:{}\n",
					frames_count - i - 1,
					symbol->Name,
					line_found ? line.FileName : "<NO_FILE_FOUND>",
					line_found ? line.LineNumber + 1 : 0UL
				);
			}
		}
		#endif
	}
}
