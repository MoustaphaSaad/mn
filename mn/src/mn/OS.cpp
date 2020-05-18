#include "mn/OS.h"
#include "mn/Debug.h"
#include "mn/File.h"

#include <stdio.h>
#include <stdlib.h>

namespace mn
{
	void
	_panic(const char* cause)
	{
		constexpr int frames_count = 20;
		void* frames[frames_count];
		callstack_capture(frames, frames_count);

		::fprintf(stderr, "[PANIC]: %s\n", cause);
		callstack_print_to(frames, frames_count, file_stderr());
		::exit(-1);
	}
}