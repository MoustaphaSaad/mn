#include "mn/Thread.h"
#include "mn/Pool.h"
#include "mn/Memory.h"
#include "mn/OS.h"
#include "mn/Fabric.h"

#include <pthread.h>
#include <unistd.h>

#include <assert.h>
#include <chrono>

namespace mn
{
	pthread_mutex_t*
	_leak_allocator_mutex_new()
	{
		static pthread_mutex_t mtx;
		[[maybe_unused]] int result = pthread_mutex_init(&mtx, NULL);
		assert(result == 0);
		return &mtx;
	}


	Mutex
	_leak_allocator_mutex()
	{
		static Mutex mtx = (Mutex)_leak_allocator_mutex_new();
		return mtx;
	}

	static void
	ms2ts(struct timespec *ts, unsigned long ms)
	{
		ts->tv_sec = ms / 1000;
		ts->tv_nsec = (ms % 1000) * 1000000;
	}


	struct IMutex
	{
		pthread_mutex_t handle;
		const char* name;
	};

	Mutex
	mutex_new(const char* name)
	{
		Mutex self = alloc<IMutex>();
		self->name = name;
		[[maybe_unused]] int result = pthread_mutex_init(&self->handle, NULL);
		assert(result == 0);
		return self;
	}

	void
	mutex_lock(Mutex self)
	{
		worker_block_ahead();
		[[maybe_unused]] int result = pthread_mutex_lock(&self->handle);
		assert(result == 0);
		worker_block_clear();
	}

	void
	mutex_unlock(Mutex self)
	{
		[[maybe_unused]] int result = pthread_mutex_unlock(&self->handle);
		assert(result == 0);
	}

	void
	mutex_free(Mutex self)
	{
		[[maybe_unused]] int result = pthread_mutex_destroy(&self->handle);
		assert(result == 0);
		free(self);
	}


	//Mutex_RW API
	struct IMutex_RW
	{
		pthread_rwlock_t lock;
		const char* name;
	};

	Mutex_RW
	mutex_rw_new(const char* name)
	{
		Mutex_RW self = alloc<IMutex_RW>();
		pthread_rwlock_init(&self->lock, NULL);
		self->name = name;
		return self;
	}

	void
	mutex_rw_free(Mutex_RW self)
	{
		pthread_rwlock_destroy(&self->lock);
		free(self);
	}

	void
	mutex_read_lock(Mutex_RW self)
	{
		worker_block_ahead();
		pthread_rwlock_rdlock(&self->lock);
		worker_block_clear();
	}

	void
	mutex_read_unlock(Mutex_RW self)
	{
		pthread_rwlock_unlock(&self->lock);
	}

	void
	mutex_write_lock(Mutex_RW self)
	{
		worker_block_ahead();
		pthread_rwlock_wrlock(&self->lock);
		worker_block_clear();
	}

	void
	mutex_write_unlock(Mutex_RW self)
	{
		pthread_rwlock_unlock(&self->lock);
	}


	//Thread
	struct IThread
	{
		pthread_t handle;
		Thread_Func func;
		void* user_data;
		const char* name;
	};

	void*
	_thread_start(void* user_data)
	{
		Thread self = (Thread)user_data;
		if(self->func)
			self->func(self->user_data);
		return 0;
	}

	Thread
	thread_new(Thread_Func func, void* arg, const char* name)
	{
		Thread self = alloc<IThread>();
		self->func = func;
		self->user_data = arg;
		self->name = name;
		[[maybe_unused]] int result = pthread_create(&self->handle, NULL, _thread_start, self);
		assert(result == 0 && "pthread_create failed");
		return self;
	}

	void
	thread_free(Thread self)
	{
		free(self);
	}

	void
	thread_join(Thread self)
	{
		worker_block_ahead();
		[[maybe_unused]] int result = pthread_join(self->handle, NULL);
		assert(result == 0 && "pthread_join failed");
		worker_block_clear();
	}

	void
	thread_sleep(uint32_t milliseconds)
	{
		usleep(milliseconds * 1000);
	}


	uint64_t
	time_in_millis()
	{
		auto tp = std::chrono::high_resolution_clock::now().time_since_epoch();
		return std::chrono::duration_cast<std::chrono::milliseconds>(tp).count();
	}


	// Condition Variables
	struct ICond_Var
	{
		pthread_cond_t cv;
	};

	Cond_Var
	cond_var_new()
	{
		auto self = alloc<ICond_Var>();
		[[maybe_unused]] auto res = pthread_cond_init(&self->cv, NULL);
		assert(res == 0);
		return self;
	}

	void
	cond_var_free(Cond_Var self)
	{
		[[maybe_unused]] auto res = pthread_cond_destroy(&self->cv);
		assert(res == 0);
		mn::free(self);
	}

	void
	cond_var_wait(Cond_Var self, Mutex mtx)
	{
		worker_block_ahead();
		pthread_cond_wait(&self->cv, &mtx->handle);
		worker_block_clear();
	}

	Cond_Var_Wake_State
	cond_var_wait_timeout(Cond_Var self, Mutex mtx, uint32_t millis)
	{
		timespec ts{};
		ms2ts(&ts, millis);

		worker_block_ahead();
		auto res = pthread_cond_timedwait(&self->cv, &mtx->handle, &ts);
		worker_block_clear();

		if (res == 0)
			return Cond_Var_Wake_State::SIGNALED;

		if (res == ETIMEDOUT)
			return Cond_Var_Wake_State::TIMEOUT;

		return Cond_Var_Wake_State::SPURIOUS;
	}

	void
	cond_var_notify(Cond_Var self)
	{
		pthread_cond_signal(&self->cv);
	}

	void
	cond_var_notify_all(Cond_Var self)
	{
		pthread_cond_broadcast(&self->cv);
	}
}
