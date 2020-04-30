#include "hot_reload_lib.h"

#include <mn/Memory.h>
#include <mn/IO.h>

namespace hot_reload_lib
{
	Foo*
	foo_new()
	{
		auto self = mn::alloc_zerod<Foo>();
		return self;
	}

	void
	foo_free(Foo* self)
	{
		mn::free(self);
	}
}

void*
rad_api(void* old_api, bool reload)
{
	// first time call 
	if (old_api == nullptr)
	{
		mn::print("hot_reload_lib first load\n");
		return hot_reload_lib::foo_new();
	}
	// reload request
	else if (old_api != nullptr && reload)
	{
		mn::print("hot_reload_lib reload happened\n");
		return old_api;
	}
	// destroy request
	else if (old_api != nullptr && reload == false)
	{
		mn::print("hot_reload_lib destroy request\n");
		hot_reload_lib::foo_free((hot_reload_lib::Foo*)old_api);
		return nullptr;
	}

	return nullptr;
}
