#include "mn/Library.h"

#include <dlfcn.h>

namespace mn
{
	// API
	Library
	library_open(const Str& filename)
	{
		if(filename.count == 0)
			return dlopen(NULL, RTLD_NOW);

		return dlopen(filename.ptr, RTLD_NOW);
	}

	void
	library_close(Library self)
	{
		dlclose(self);
	}

	void*
	library_proc(Library self, const Str& proc_name)
	{
		return dlsym(self, proc_name.ptr);
	}
}