#pragma once

#include "mn/Exports.h"

namespace mn
{
	struct Root;

	MN_EXPORT Root*
	root_new();

	MN_EXPORT void
	root_free(Root* self);

	inline static void
	destruct(Root* self)
	{
		root_free(self);
	}

	MN_EXPORT bool
	root_register(Root* self, const char* name, const char* filepath);

	MN_EXPORT void*
	root_api_ptr(Root* self, const char* name);

	template<typename T>
	inline static T*
	root_api(Root* self, const char* name)
	{
		return (T*)root_api_ptr(self, name);
	}

	MN_EXPORT bool
	root_update(Root* self);
}
