#include "mn/IPC.h"
#include "mn/Str.h"
#include "mn/Memory.h"

#include <errno.h> // errno, ENOENT
#include <fcntl.h> // O_RDWR, O_CREATE
#include <linux/limits.h> // NAME_MAX
#include <sys/mman.h> // shm_open, shm_unlink, mmap, munmap,
                      // PROT_READ, PROT_WRITE, MAP_SHARED, MAP_FAILED
#include <unistd.h> // ftruncate, close
#include <pthread.h>
#include <atomic>

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

	Mutex_Result
	mutex_new(const Str& name, bool immediate_lock)
	{
		errno = 0;
		// Open existing shared memory object, or create one.
		// Two separate calls are needed here, to mark fact of creation
		// for later initialization of pthread mutex.
		int shm_fd = shm_open(name.ptr, O_RDWR, 0660);
		bool created = false;
		bool locked = false;

		// mutex was not created before, create it!
		if(errno == ENOENT)
		{
			shm_fd = shm_open(name.ptr, O_RDWR | O_CREAT, 0660);
			created = true;
		}

		if(shm_fd == -1)
			return Mutex_Result{};

		// Truncate shared memory segment so it would contain IShared_Mutex
		if (ftruncate(shm_fd, sizeof(IShared_Mutex)) != 0)
			return Mutex_Result{};

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
			return Mutex_Result{};

		auto shared_mtx = (IShared_Mutex*)addr;

		// if first one to create the mutex then initialize it!
		if(created)
		{
			pthread_mutexattr_t attr{};
			if(pthread_mutexattr_init(&attr))
				return Mutex_Result{};

			if(pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED))
				return Mutex_Result{};

			if(pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST))
				return Mutex_Result{};

			if(pthread_mutex_init(&shared_mtx->mtx, &attr))
				return Mutex_Result{};

			shared_mtx->atomic_rc = 1;

			if(immediate_lock)
			{
				pthread_mutex_lock(&shared_mtx->mtx);
				locked = true;
			}
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

		return Mutex_Result{self, locked};
	}

	void
	mutex_free(Mutex self)
	{
		if(self->shared_mtx->atomic_rc.fetch_sub(1) <= 1)
		{
			int res = pthread_mutex_destroy(&self->shared_mtx->mtx);
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
			int res = munmap(self->shared_mtx, sizeof(self->shared_mtx));
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
