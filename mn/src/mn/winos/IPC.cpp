#include "mn/IPC.h"
#include "mn/File.h"
#include "mn/Defer.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <mbstring.h>
#include <tchar.h>

namespace mn::ipc
{
	// API
	Mutex_Result
	mutex_new(const Str& name, bool immediate_lock)
	{
		auto os_str = to_os_encoding(name, allocator_top());
		mn_defer(mn::free(os_str));

		auto handle = (Mutex)CreateMutex(0, immediate_lock, (LPCWSTR)os_str.ptr);
		if (handle == INVALID_HANDLE_VALUE)
			return Mutex_Result{};

		return Mutex_Result{ handle, GetLastError() != ERROR_ALREADY_EXISTS };
	}

	void
	mutex_free(Mutex self)
	{
		[[maybe_unused]] auto res = CloseHandle((HANDLE)self);
		assert(res == TRUE);
	}

	void
	mutex_lock(Mutex mtx)
	{
		auto self = (HANDLE)mtx;
		WaitForSingleObject(self, INFINITE);
	}

	LOCK_RESULT
	mutex_try_lock(Mutex mtx)
	{
		auto self = (HANDLE)mtx;
		auto res = WaitForSingleObject(self, 0);
		switch(res)
		{
		case WAIT_OBJECT_0:
			return LOCK_RESULT::OBTAINED;
		case WAIT_ABANDONED:
			return LOCK_RESULT::ABANDONED;
		default:
			return LOCK_RESULT::FAILED;
		}
	}

	void
	mutex_unlock(Mutex mtx)
	{
		auto self = (HANDLE)mtx;
		[[maybe_unused]] BOOL res = ReleaseMutex(self);
		assert(res == TRUE);
	}
}
