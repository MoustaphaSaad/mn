#include "mn/Thread.h"
#include "mn/Memory.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <assert.h>

#include <chrono>

namespace mn
{
	struct IMutex
	{
		const char* name;
		CRITICAL_SECTION cs;
	};

	struct Leak_Allocator_Mutex
	{
		IMutex self;

		Leak_Allocator_Mutex()
		{
			self.name = "allocators mutex";
			InitializeCriticalSectionAndSpinCount(&self.cs, 1<<14);
		}

		~Leak_Allocator_Mutex()
		{
			DeleteCriticalSection(&self.cs);
		}
	};

	Mutex
	_leak_allocator_mutex()
	{
		static Leak_Allocator_Mutex mtx;
		return &mtx.self;
	}

	Mutex
	mutex_new(const char* name)
	{
		Mutex self = alloc<IMutex>();
		self->name = name;
		InitializeCriticalSectionAndSpinCount(&self->cs, 1<<14);
		return self;
	}

	void
	mutex_lock(Mutex self)
	{
		EnterCriticalSection(&self->cs);
	}

	void
	mutex_unlock(Mutex self)
	{
		LeaveCriticalSection(&self->cs);
	}

	void
	mutex_free(Mutex self)
	{
		DeleteCriticalSection(&self->cs);
		free(self);
	}


	//Mutex_RW API
	struct IMutex_RW
	{
		SRWLOCK lock;
		const char* name;
	};

	Mutex_RW
	mutex_rw_new(const char* name)
	{
		Mutex_RW self = alloc<IMutex_RW>();
		self->lock = SRWLOCK_INIT;
		self->name = name;
		return self;
	}

	void
	mutex_rw_free(Mutex_RW self)
	{
		free(self);
	}

	void
	mutex_read_lock(Mutex_RW self)
	{
		AcquireSRWLockShared(&self->lock);
	}

	void
	mutex_read_unlock(Mutex_RW self)
	{
		ReleaseSRWLockShared(&self->lock);
	}

	void
	mutex_write_lock(Mutex_RW self)
	{
		AcquireSRWLockExclusive(&self->lock);
	}

	void
	mutex_write_unlock(Mutex_RW self)
	{
		ReleaseSRWLockExclusive(&self->lock);
	}


	//Thread API
	struct IThread
	{
		HANDLE handle;
		DWORD id;
		Thread_Func func;
		void* user_data;
		const char* name;
	};

	DWORD WINAPI
	_thread_start(LPVOID user_data)
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

		self->handle = CreateThread(NULL, //default security attributes
									0, //default stack size
									_thread_start, //thread start function
									self, //thread start function arg
									0, //default creation flags
									&self->id); //thread id
		return self;
	}

	void
	thread_free(Thread self)
	{
		if(self->handle)
		{
			[[maybe_unused]] BOOL result = CloseHandle(self->handle);
			assert(result == TRUE);
		}
		free(self);
	}

	void
	thread_join(Thread self)
	{
		if(self->handle)
		{
			[[maybe_unused]] DWORD result = WaitForSingleObject(self->handle, INFINITE);
			assert(result == WAIT_OBJECT_0);
		}
	}

	void
	thread_sleep(uint32_t milliseconds)
	{
		Sleep(DWORD(milliseconds));
	}


	// time
	uint64_t
	time_in_millis()
	{
		auto tp = std::chrono::high_resolution_clock::now().time_since_epoch();
		return std::chrono::duration_cast<std::chrono::milliseconds>(tp).count();
	}


	// Condition Variable
	struct ICond_Var
	{
		CONDITION_VARIABLE cv;
	};

	Cond_Var
	cond_var_new()
	{
		auto self = alloc<ICond_Var>();
		InitializeConditionVariable(&self->cv);
		return self;
	}

	void
	cond_var_free(Cond_Var self)
	{
		mn::free(self);
	}

	void
	cond_var_wait(Cond_Var self, Mutex mtx)
	{
		SleepConditionVariableCS(&self->cv, &mtx->cs, INFINITE);
	}

	Cond_Var_Wake_State
	cond_var_wait_timeout(Cond_Var self, Mutex mtx, uint32_t millis)
	{
		auto res = SleepConditionVariableCS(&self->cv, &mtx->cs, millis);
		if (res)
			return Cond_Var_Wake_State::SIGNALED;

		if (GetLastError() == ERROR_TIMEOUT)
			return Cond_Var_Wake_State::TIMEOUT;

		return Cond_Var_Wake_State::SPURIOUS;
	}

	void
	cond_var_read_wait(Cond_Var self, Mutex_RW mtx)
	{
		SleepConditionVariableSRW(&self->cv, &mtx->lock, INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED);
	}

	Cond_Var_Wake_State
	cond_var_read_wait_timeout(Cond_Var self, Mutex_RW mtx, uint32_t millis)
	{
		auto res = SleepConditionVariableSRW(&self->cv, &mtx->lock, millis, CONDITION_VARIABLE_LOCKMODE_SHARED);
		if (res)
			return Cond_Var_Wake_State::SIGNALED;

		if (GetLastError() == ERROR_TIMEOUT)
			return Cond_Var_Wake_State::TIMEOUT;

		return Cond_Var_Wake_State::SPURIOUS;
	}

	void
	cond_var_write_wait(Cond_Var self, Mutex_RW mtx)
	{
		SleepConditionVariableSRW(&self->cv, &mtx->lock, INFINITE, 0);
	}

	Cond_Var_Wake_State
	cond_var_write_wait_timeout(Cond_Var self, Mutex_RW mtx, uint32_t millis)
	{
		auto res = SleepConditionVariableSRW(&self->cv, &mtx->lock, millis, 0);
		if (res)
			return Cond_Var_Wake_State::SIGNALED;

		if (GetLastError() == ERROR_TIMEOUT)
			return Cond_Var_Wake_State::TIMEOUT;

		return Cond_Var_Wake_State::SPURIOUS;
	}

	void
	cond_var_notify(Cond_Var self)
	{
		WakeConditionVariable(&self->cv);
	}

	void
	cond_var_notify_all(Cond_Var self)
	{
		WakeAllConditionVariable(&self->cv);
	}


	//Limbo
	struct ILimbo
	{
		CRITICAL_SECTION cs;
		CONDITION_VARIABLE cv;
		const char* name;
	};

	Limbo
	limbo_new(const char* name)
	{
		Limbo self = alloc<ILimbo>();
		InitializeCriticalSectionAndSpinCount(&self->cs, 4096);
		self->cv = CONDITION_VARIABLE_INIT;
		self->name = name;
		return self;
	}

	void
	limbo_free(Limbo self)
	{
		DeleteCriticalSection(&self->cs);
		free(self);
	}

	void
	limbo_lock(Limbo self, Limbo_Predicate* pred)
	{
		EnterCriticalSection(&self->cs);

		while(pred->should_wake() == false)
			SleepConditionVariableCS(&self->cv, &self->cs, INFINITE);
	}

	void
	limbo_unlock_one(Limbo self)
	{
		LeaveCriticalSection(&self->cs);
		WakeConditionVariable(&self->cv);
	}

	void
	limbo_unlock_all(Limbo self)
	{
		LeaveCriticalSection(&self->cs);
		WakeAllConditionVariable(&self->cv);
	}
}
