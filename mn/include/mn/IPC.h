#pragma once

#include "mn/Exports.h"
#include "mn/Str.h"

namespace mn::ipc
{
	typedef struct IMutex *Mutex;

	struct Mutex_Result
	{
		Mutex mtx;
		bool locked;
	};

	enum class LOCK_RESULT
	{
		OBTAINED,
		ABANDONED,
		FAILED
	};

	MN_EXPORT Mutex_Result
	mutex_new(const Str& name, bool immediate_lock = false);

	inline static Mutex_Result
	mutex_new(const char* name, bool immediate_lock = false)
	{
		return mutex_new(str_lit(name), immediate_lock);
	}

	MN_EXPORT void
	mutex_free(Mutex self);

	inline static void
	destruct(Mutex self)
	{
		mutex_free(self);
	}

	MN_EXPORT void
	mutex_lock(Mutex self);

	MN_EXPORT LOCK_RESULT
	mutex_try_lock(Mutex self);

	MN_EXPORT void
	mutex_unlock(Mutex self);
}