#pragma once

#include "mn/Exports.h"
#include "mn/Str.h"

namespace mn::ipc
{
	typedef struct IIPC_Mutex *Mutex;

	MN_EXPORT Mutex
	mutex_new(const Str& name);

	inline static Mutex
	mutex_new(const char* name)
	{
		return mutex_new(str_lit(name));
	}

	MN_EXPORT void
	mutex_free(Mutex self);

	inline static void
	destruct(Mutex self)
	{
		mn::ipc::mutex_free(self);
	}

	MN_EXPORT void
	mutex_lock(Mutex self);

	MN_EXPORT bool
	mutex_try_lock(Mutex self);

	MN_EXPORT void
	mutex_unlock(Mutex self);
}