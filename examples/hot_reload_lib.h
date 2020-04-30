#pragma once

#include "hot_reload_lib_exports.h"

namespace hot_reload_lib
{
	struct Foo
	{
		int x;
	};

	HOT_RELOAD_LIB_EXPORT Foo*
	foo_new();

	HOT_RELOAD_LIB_EXPORT void
	foo_free(Foo* self);
}

#define HOT_RELOAD_LIB_NAME "hot_reload_lib"

extern "C" HOT_RELOAD_LIB_EXPORT void*
rad_api(void* old_api, bool reload);
