#include "mn/IPC.h"
#include "mn/File.h"
#include "mn/Defer.h"
#include "mn/Fabric.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <mbstring.h>
#include <tchar.h>

namespace mn::ipc
{
	// API
	Mutex
	mutex_new(const Str& name)
	{
		auto os_str = to_os_encoding(name, allocator_top());
		mn_defer(mn::free(os_str));

		auto handle = CreateMutex(0, false, (LPCWSTR)os_str.ptr);
		if (handle == INVALID_HANDLE_VALUE)
			return nullptr;

		return (Mutex)handle;
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
		worker_block_ahead();
		WaitForSingleObject(self, INFINITE);
		worker_block_clear();
	}

	bool
	mutex_try_lock(Mutex mtx)
	{
		auto self = (HANDLE)mtx;
		auto res = WaitForSingleObject(self, 0);
		switch(res)
		{
		case WAIT_OBJECT_0:
		case WAIT_ABANDONED:
			return true;
		default:
			return false;
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
