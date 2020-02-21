#include "mn/IPC.h"
#include "mn/Str.h"
#include "mn/Memory.h"
#include "mn/Fabric.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>

namespace mn::ipc
{
	bool
	_mutex_try_lock(Mutex self, int64_t offset, int64_t size)
	{
		assert(offset >= 0 && size >= 0);
		struct flock fl{};
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(intptr_t(self), F_SETLK, &fl) != -1;
	}

	bool
	_mutex_unlock(Mutex self, int64_t offset, int64_t size)
	{
		assert(offset >= 0 && size >= 0);
		struct flock fl{};
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(intptr_t(self), F_SETLK, &fl) != -1;
	}

	// API
	Mutex
	mutex_new(const Str& name)
	{
		int flags = O_WRONLY | O_CREAT | O_APPEND;

		int macos_handle = ::open(name.ptr, flags, S_IRWXU);
		if(macos_handle == -1)
			return nullptr;

		return Mutex(macos_handle);
	}

	void
	mutex_free(Mutex mtx)
	{
		::close(intptr_t(mtx));
	}

	void
	mutex_lock(Mutex mtx)
	{
		worker_block_ahead();

		worker_block_on([&]{
			return _mutex_try_lock(mtx, 0, 0);
		});

		worker_block_clear();
	}

	bool
	mutex_try_lock(Mutex mtx)
	{
		return _mutex_try_lock(mtx, 0, 0);
	}

	void
	mutex_unlock(Mutex mtx)
	{
		_mutex_unlock(mtx, 0, 0);
	}
}
