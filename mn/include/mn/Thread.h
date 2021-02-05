#pragma once

#include "mn/Exports.h"
#include "mn/Base.h"
#include "mn/OS.h"

#include <stdint.h>
#include <atomic>

namespace mn
{
	/**
	 * Mutex Handle
	 */
	typedef struct IMutex* Mutex;

	MN_EXPORT Mutex
	_leak_allocator_mutex();

	/**
	 * @brief      Creates a new mutex
	 *
	 * @param[in]  name  The mutex name
	 */
	MN_EXPORT Mutex
	mutex_new(const char* name = "Mutex");

	/**
	 * @brief      Locks the given mutex
	 *
	 * @param[in]  mutex  The mutex
	 */
	MN_EXPORT void
	mutex_lock(Mutex mutex);

	/**
	 * @brief      Unlocks the given mutex
	 *
	 * @param[in]  mutex  The mutex
	 */
	MN_EXPORT void
	mutex_unlock(Mutex mutex);

	/**
	 * @brief      Frees the given mutex
	 *
	 * @param[in]  mutex  The mutex
	 */
	MN_EXPORT void
	mutex_free(Mutex mutex);

	/**
	 * @brief      Destruct function overload for the mutex type
	 *
	 * @param[in]  mutex  The mutex
	 */
	inline static void
	destruct(Mutex mutex)
	{
		mutex_free(mutex);
	}


	//Read preferring multi-reader single-writer mutex
	typedef struct IMutex_RW* Mutex_RW;

	/**
	 * @brief      Creates a new multi-reader single-writer mutex
	 *
	 * @param[in]  name  The mutex name
	 */
	MN_EXPORT Mutex_RW
	mutex_rw_new(const char* name = "Mutex_RW");

	/**
	 * @brief      Frees the mutex
	 */
	MN_EXPORT void
	mutex_rw_free(Mutex_RW mutex);

	/**
	 * @brief      destruct overload function for Mutex_RW
	 */
	inline static void
	destruct(Mutex_RW mutex)
	{
		mutex_rw_free(mutex);
	}

	/**
	 * @brief      Locks the mutex for a read operation
	 */
	MN_EXPORT void
	mutex_read_lock(Mutex_RW mutex);

	/**
	 * @brief      Unlocks the mutex from a read operation
	 */
	MN_EXPORT void
	mutex_read_unlock(Mutex_RW mutex);

	/**
	 * @brief      Locks the mutex for a write operation
	 */
	MN_EXPORT void
	mutex_write_lock(Mutex_RW mutex);

	/**
	 * @brief      Unlocks the mutex from a write operation
	 */
	MN_EXPORT void
	mutex_write_unlock(Mutex_RW mutex);


	//Thread API
	typedef struct IThread* Thread;

	/**
	 * Thread function
	 */
	using Thread_Func = void(*)(void*);

	/**
	 * @brief      Creates a new thread
	 *
	 * @param[in]  func  The function to run
	 * @param      arg   The argument to pass to the function
	 * @param[in]  name  The name of the thread
	 */
	MN_EXPORT Thread
	thread_new(Thread_Func func, void* arg, const char* name = "Thread");

	/**
	 * @brief      Frees the thread
	 */
	MN_EXPORT void
	thread_free(Thread thread);

	/**
	 * @brief      destruct overload for the thread type
	 */
	inline static void
	destruct(Thread thread)
	{
		thread_free(thread);
	}

	/**
	 * @brief      joins the execution of the calling thread with the given thread
	 */
	MN_EXPORT void
	thread_join(Thread thread);

	/**
	 * @brief      sleeps the calling thread by the given milliseconds
	 */
	MN_EXPORT void
	thread_sleep(uint32_t milliseconds);


	// time in milliseonds
	MN_EXPORT uint64_t
	time_in_millis();


	// Condition Variable
	typedef struct ICond_Var* Cond_Var;

	enum class Cond_Var_Wake_State
	{
		SIGNALED,
		TIMEOUT,
		SPURIOUS
	};

	MN_EXPORT Cond_Var
	cond_var_new();

	MN_EXPORT void
	cond_var_free(Cond_Var self);

	inline static void
	destruct(Cond_Var self)
	{
		cond_var_free(self);
	}

	MN_EXPORT void
	cond_var_wait(Cond_Var self, Mutex mtx);

	template<typename TFunc>
	inline static void
	cond_var_wait(Cond_Var self, Mutex mtx, TFunc&& func)
	{
		while (func() == false)
			cond_var_wait(self, mtx);
	}

	MN_EXPORT Cond_Var_Wake_State
	cond_var_wait_timeout(Cond_Var self, Mutex mtx, uint32_t millis);

	template<typename TFunc>
	inline static bool
	cond_var_wait_timeout(Cond_Var self, Mutex mtx, uint32_t millis, TFunc&& func)
	{
		auto start_time = time_in_millis();
		while (func() == false)
		{
			auto state = cond_var_wait_timeout(self, mtx, millis);
			if (state == Cond_Var_Wake_State::SIGNALED)
			{
				return true;
			}
			else if (state == Cond_Var_Wake_State::TIMEOUT)
			{
				return false;
			}
			else
			{
				auto end_time = time_in_millis();
				if (end_time - start_time >= millis)
					return false;
			}
		}
	}

	MN_EXPORT void
	cond_var_notify(Cond_Var self);

	MN_EXPORT void
	cond_var_notify_all(Cond_Var self);

	// Semaphore
	typedef std::atomic<int32_t> Waitgroup;

	MN_EXPORT void
	waitgroup_wait(Waitgroup& self);

	MN_EXPORT void
	waitgroup_wake(Waitgroup& self);

	inline static void
	waitgroup_add(Waitgroup& self, int32_t i)
	{
		self.fetch_add(i);
	}

	inline static void
	waitgroup_done(Waitgroup& self)
	{
		[[maybe_unused]] int prev = self.fetch_sub(1);
		if (prev < 1)
			panic("Waitgroup misuse: add called concurrently with wait");
		if (prev == 1)
			waitgroup_wake(self);
	}
}
