#include "mn/Str_Intern.h"

#include <assert.h>

namespace mn
{
	const char*
	str_intern(Str_Intern& self, const char* str)
	{
		if(auto it = set_lookup(self.strings, str_lit(str)))
		{
			return it->ptr;
		}
		return set_insert(self.strings, str_from_c(str, self.tmp_str.allocator))->ptr;
	}

	const char*
	str_intern(Str_Intern& self, const Str& str)
	{
		if(auto it = set_lookup(self.strings, str))
		{
			return it->ptr;
		}
		return set_insert(self.strings, str_clone(str, self.tmp_str.allocator))->ptr;
	}

	const char*
	str_intern(Str_Intern& self, const char* begin, const char* end)
	{
		assert(end >= begin && "Invalid SubStr");
		str_clear(self.tmp_str);
		buf_resize(self.tmp_str, end - begin + 1);
		--self.tmp_str.count;
		::memcpy(self.tmp_str.ptr, begin, self.tmp_str.count);
		self.tmp_str.ptr[self.tmp_str.count] = '\0';
		return str_intern(self, self.tmp_str);
	}
}