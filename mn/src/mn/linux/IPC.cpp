#include "mn/IPC.h"
#include "mn/Str.h"
#include "mn/Memory.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <atomic>
#include <assert.h>

namespace mn::ipc
{
	struct IShared_Mutex
	{
		pthread_mutex_t mtx;
		std::atomic<int32_t> atomic_rc;
	};

	struct IMutex
	{
		IShared_Mutex* shared_mtx;
		int shm_fd;
		Str name;
		bool created;
	};

	Mutex
	mutex_new(const Str& name)
	{
		errno = 0;
		// Open existing shared memory object, or create one.
		// Two separate calls are needed here, to mark fact of creation
		// for later initialization of pthread mutex.
		int shm_fd = shm_open(name.ptr, O_RDWR, 0660);
		bool created = false;

		// mutex was not created before, create it!
		if(errno == ENOENT)
		{
			shm_fd = shm_open(name.ptr, O_RDWR | O_CREAT, 0660);
			created = true;
		}

		if(shm_fd == -1)
			return nullptr;

		// Truncate shared memory segment so it would contain IShared_Mutex
		if (ftruncate(shm_fd, sizeof(IShared_Mutex)) != 0)
		{
			close(shm_fd);
			shm_unlink(name.ptr);
			return nullptr;
		}

		// Map pthread mutex into the shared memory.
		void *addr = mmap(
		  NULL,
		  sizeof(IShared_Mutex),
		  PROT_READ|PROT_WRITE,
		  MAP_SHARED,
		  shm_fd,
		  0
		);

		if(addr == MAP_FAILED)
		{
			close(shm_fd);
			shm_unlink(name.ptr);
			return nullptr;
		}

		auto shared_mtx = (IShared_Mutex*)addr;

		// if first one to create the mutex then initialize it!
		if(created)
		{
			pthread_mutexattr_t attr{};
			[[maybe_unused]] auto mtx_err = pthread_mutexattr_init(&attr);
			assert(mtx_err == 0);

			mtx_err = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
			assert(mtx_err == 0);

			mtx_err = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
			assert(mtx_err == 0);

			mtx_err = pthread_mutex_init(&shared_mtx->mtx, &attr);
			assert(mtx_err == 0);

			shared_mtx->atomic_rc = 1;
		}
		else
		{
			shared_mtx->atomic_rc.fetch_add(1);
		}

		auto self = alloc<IMutex>();
		self->shared_mtx = shared_mtx;
		self->shm_fd = shm_fd;
		self->name = clone(name);
		self->created = created;

		return self;
	}

	void
	mutex_free(Mutex self)
	{
		if(self->shared_mtx->atomic_rc.fetch_sub(1) <= 1)
		{
			[[maybe_unused]] int res = pthread_mutex_destroy(&self->shared_mtx->mtx);
			assert(res == 0);

			res = munmap(self->shared_mtx, sizeof(self->shared_mtx));
			assert(res == 0);

			res = close(self->shm_fd);
			assert(res == 0);

			res = shm_unlink(self->name.ptr);
			assert(res == 0);

			str_free(self->name);

			mn::free(self);
		}
		else
		{
			[[maybe_unused]] int res = munmap(self->shared_mtx, sizeof(self->shared_mtx));
			assert(res == 0);

			res = close(self->shm_fd);
			assert(res == 0);

			str_free(self->name);

			mn::free(self);
		}
	}

	void
	mutex_lock(Mutex self)
	{
		while(true)
		{
			int res = pthread_mutex_lock(&self->shared_mtx->mtx);
			if(res == EOWNERDEAD)
			{
				pthread_mutex_consistent(&self->shared_mtx->mtx);
				res = pthread_mutex_unlock(&self->shared_mtx->mtx);
				assert(res == 0);
				self->shared_mtx->atomic_rc.fetch_sub(1);
				continue;
			}
			assert(res == 0);
			break;
		}
	}

	LOCK_RESULT
	mutex_try_lock(Mutex self)
	{
		int res = pthread_mutex_trylock(&self->shared_mtx->mtx);
		if(res == 0)
		{
			return LOCK_RESULT::OBTAINED;
		}
		else if (res == EOWNERDEAD)
		{
			pthread_mutex_consistent(&self->shared_mtx->mtx);
			res = pthread_mutex_unlock(&self->shared_mtx->mtx);
			assert(res == 0);
			self->shared_mtx->atomic_rc.fetch_sub(1);

			res = pthread_mutex_trylock(&self->shared_mtx->mtx);
			if(res == 0)
				return LOCK_RESULT::ABANDONED;
			else
				return LOCK_RESULT::FAILED;
		}
		else
		{
			return LOCK_RESULT::FAILED;
		}
	}

	void
	mutex_unlock(Mutex self)
	{
		[[maybe_unused]] int res = pthread_mutex_unlock(&self->shared_mtx->mtx);	
		assert(res == 0);
	}
}
