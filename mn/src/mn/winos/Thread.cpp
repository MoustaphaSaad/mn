#include "mn/Thread.h"
#include "mn/Memory.h"
#include "mn/Fabric.h"
#include "mn/Map.h"
#include "mn/Defer.h"
#include "mn/Debug.h"
#include "mn/Log.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <assert.h>
#include <emmintrin.h>
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

	// Deadlock detector
	struct Mutex_Thread_Owner
	{
		DWORD id;
		int callstack_count;
		void* callstack[20];
	};

	struct Mutex_Deadlock_Reason
	{
		void* mtx;
		Mutex_Thread_Owner* owner;
	};

	struct Mutex_Ownership
	{
		enum KIND
		{
			KIND_EXCLUSIVE,
			KIND_SHARED,
		};

		KIND kind;
		union
		{
			Mutex_Thread_Owner exclusive;
			Map<DWORD, Mutex_Thread_Owner> shared;
		};
	};

	inline static Mutex_Ownership
	mutex_ownership_exclusive(DWORD thread_id)
	{
		Mutex_Ownership self{};
		self.kind = Mutex_Ownership::KIND_EXCLUSIVE;
		self.exclusive.id = thread_id;
		self.exclusive.callstack_count = callstack_capture(self.exclusive.callstack, 20);
		return self;
	}

	inline static Mutex_Ownership
	mutex_ownership_shared()
	{
		Mutex_Ownership self{};
		self.kind = Mutex_Ownership::KIND_SHARED;
		self.shared = map_with_allocator<DWORD, Mutex_Thread_Owner>(memory::clib());
		return self;
	}

	inline static void
	mutex_ownership_free(Mutex_Ownership& self)
	{
		switch(self.kind)
		{
		case Mutex_Ownership::KIND_EXCLUSIVE:
			// do nothing
			break;
		case Mutex_Ownership::KIND_SHARED:
			map_free(self.shared);
			break;
		default:
			assert(false && "unreachable");
			break;
		}
	}

	inline static void
	destruct(Mutex_Ownership& self)
	{
		mutex_ownership_free(self);
	}

	inline static void
	mutex_ownership_shared_add_owner(Mutex_Ownership& self, DWORD thread_id)
	{
		Mutex_Thread_Owner owner{};
		owner.id = thread_id;
		owner.callstack_count = callstack_capture(owner.callstack, 20);
		map_insert(self.shared, thread_id, owner);
	}

	inline static void
	mutex_ownership_shared_remove_owner(Mutex_Ownership& self, DWORD thread_id)
	{
		map_remove(self.shared, thread_id);
	}

	inline static bool
	mutex_ownership_check(Mutex_Ownership& self, DWORD thread_id)
	{
		switch(self.kind)
		{
		case Mutex_Ownership::KIND_EXCLUSIVE:
			return self.exclusive.id == thread_id;
		case Mutex_Ownership::KIND_SHARED:
			return map_lookup(self.shared, thread_id) != nullptr;
		default:
			assert(false && "unreachable");
			return false;
		}
	}

	inline static Mutex_Thread_Owner*
	mutex_ownership_get_owner(Mutex_Ownership& self, DWORD thread_id)
	{
		switch(self.kind)
		{
		case Mutex_Ownership::KIND_EXCLUSIVE:
			return &self.exclusive;
		case Mutex_Ownership::KIND_SHARED:
			return &map_lookup(self.shared, thread_id)->value;
		default:
			assert(false && "unreachable");
			return nullptr;
		}
	}

	struct Deadlock_Detector
	{
		IMutex mtx;
		Map<void*, Mutex_Ownership> mutex_thread_owner;
		Map<DWORD, void*> thread_mutex_block;

		Deadlock_Detector()
		{
			mtx.name = "deadlock mutex";
			InitializeCriticalSectionAndSpinCount(&mtx.cs, 1<<14);
			mutex_thread_owner = map_with_allocator<void*, Mutex_Ownership>(memory::clib());
			thread_mutex_block = map_with_allocator<DWORD, void*>(memory::clib());
		}

		~Deadlock_Detector()
		{
			DeleteCriticalSection(&mtx.cs);
			destruct(mutex_thread_owner);
			map_free(thread_mutex_block);
		}
	};

	inline static Deadlock_Detector*
	_deadlock_detector()
	{
		static Deadlock_Detector _detector;
		return &_detector;
	}

	inline static bool
	_deadlock_detector_has_block_loop(Deadlock_Detector* self, void* mtx, DWORD thread_id, Buf<Mutex_Deadlock_Reason>& reasons)
	{
		auto it = map_lookup(self->mutex_thread_owner, mtx);
		if (it == nullptr)
			return false;

		bool deadlock_detected = false;
		DWORD reason_thread_id = thread_id;
		if (mutex_ownership_check(it->value, thread_id))
		{
			deadlock_detected = true;
		}
		else
		{
			switch(it->value.kind)
			{
			case Mutex_Ownership::KIND_EXCLUSIVE:
				if (auto block_it = map_lookup(self->thread_mutex_block, it->value.exclusive.id))
				{
					deadlock_detected = _deadlock_detector_has_block_loop(self, block_it->value, thread_id, reasons);
					reason_thread_id = it->value.exclusive.id;
				}
				break;
			case Mutex_Ownership::KIND_SHARED:
				for (auto [id, owner]: it->value.shared)
				{
					if (auto block_it = map_lookup(self->thread_mutex_block, id))
					{
						if (_deadlock_detector_has_block_loop(self, block_it->value, thread_id, reasons))
						{
							deadlock_detected = true;
							reason_thread_id = block_it->key;
							break;
						}
					}
				}
				break;
			default:
				assert(false && "unreachable");
				break;
			}
		}

		if (deadlock_detected)
		{
			auto owner = mutex_ownership_get_owner(it->value, reason_thread_id);
			Mutex_Deadlock_Reason reason{};
			reason.mtx = it->key;
			reason.owner = owner;
			buf_push(reasons, reason);
			return true;
		}
		return false;
	}

	inline static void
	_deadlock_detector_mutex_block(void* mtx)
	{
		#ifdef MN_DEADLOCK
		auto self = _deadlock_detector();
		auto thread_id = GetCurrentThreadId();

		EnterCriticalSection(&self->mtx.cs);
		mn_defer(LeaveCriticalSection(&self->mtx.cs));

		map_insert(self->thread_mutex_block, thread_id, mtx);

		Buf<Mutex_Deadlock_Reason> reasons{};
		if (_deadlock_detector_has_block_loop(self, mtx, thread_id, reasons))
		{
			log_error("Deadlock on mutex {} by thread #{} because of #{} reasons are listed below:", mtx, thread_id, reasons.count);
			void* callstack[20];
			auto callstack_count = callstack_capture(callstack, 20);
			callstack_print_to(callstack, callstack_count, file_stderr());
			printerr("\n");

			for (size_t i = 0; i < reasons.count; ++i)
			{
				auto ix = reasons.count - i - 1;
				auto reason = reasons[ix];

				auto block_it = map_lookup(self->thread_mutex_block, reason.owner->id);
				log_error(
					"reason #{}: Mutex {} was locked at the callstack listed below by thread #{} (while it was waiting for mutex {} to be released):",
					ix + 1,
					reason.mtx,
					reason.owner->id,
					block_it->value
				);
				callstack_print_to(reason.owner->callstack, reason.owner->callstack_count, file_stderr());
				printerr("\n");
			}
			::exit(-1);
		}
		buf_free(reasons);

		#endif
	}

	inline static void
	_deadlock_detector_mutex_set_exclusive_owner(void* mtx)
	{
		#ifdef MN_DEADLOCK
		auto self = _deadlock_detector();
		auto thread_id = GetCurrentThreadId();

		EnterCriticalSection(&self->mtx.cs);
		mn_defer(LeaveCriticalSection(&self->mtx.cs));

		if (auto it = map_lookup(self->mutex_thread_owner, mtx))
		{
			panic("Deadlock on mutex {} by thread #{}", mtx, thread_id);
		}

		map_remove(self->thread_mutex_block, thread_id);
		map_insert(self->mutex_thread_owner, mtx, mutex_ownership_exclusive(thread_id));
		#endif
	}

	inline static void
	_deadlock_detector_mutex_set_shared_owner(void* mtx)
	{
		#ifdef MN_DEADLOCK
		auto self = _deadlock_detector();
		auto thread_id = GetCurrentThreadId();

		EnterCriticalSection(&self->mtx.cs);
		mn_defer(LeaveCriticalSection(&self->mtx.cs));

		map_remove(self->thread_mutex_block, thread_id);
		if (auto it = map_lookup(self->mutex_thread_owner, mtx))
		{
			mutex_ownership_shared_add_owner(it->value, thread_id);
		}
		else
		{
			auto mutex_ownership = mutex_ownership_shared();
			mutex_ownership_shared_add_owner(mutex_ownership, thread_id);
			map_insert(self->mutex_thread_owner, mtx, mutex_ownership);
		}
		#endif
	}

	inline static void
	_deadlock_detector_mutex_unset_owner(void* mtx)
	{
		#ifdef MN_DEADLOCK
		auto self = _deadlock_detector();
		auto thread_id = GetCurrentThreadId();

		EnterCriticalSection(&self->mtx.cs);
		mn_defer(LeaveCriticalSection(&self->mtx.cs));

		auto it = map_lookup(self->mutex_thread_owner, mtx);
		switch(it->value.kind)
		{
		case Mutex_Ownership::KIND_EXCLUSIVE:
			map_remove(self->mutex_thread_owner, mtx);
			break;
		case Mutex_Ownership::KIND_SHARED:
			mutex_ownership_shared_remove_owner(it->value, thread_id);
			if (it->value.shared.count == 0)
			{
				mutex_ownership_free(it->value);
				map_remove(self->mutex_thread_owner, mtx);
			}
			break;
		default:
			assert(false && "unreachable");
			break;
		}
		#endif
	}

	// API
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
		if (TryEnterCriticalSection(&self->cs))
		{
			_deadlock_detector_mutex_set_exclusive_owner(self);
			return;
		}

		worker_block_ahead();
		_deadlock_detector_mutex_block(self);
		EnterCriticalSection(&self->cs);
		_deadlock_detector_mutex_set_exclusive_owner(self);
		worker_block_clear();
	}

	void
	mutex_unlock(Mutex self)
	{
		_deadlock_detector_mutex_unset_owner(self);
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
		if (TryAcquireSRWLockShared(&self->lock))
		{
			_deadlock_detector_mutex_set_shared_owner(self);
			return;
		}

		worker_block_ahead();
		_deadlock_detector_mutex_block(self);
		AcquireSRWLockShared(&self->lock);
		_deadlock_detector_mutex_set_shared_owner(self);
		worker_block_clear();
	}

	void
	mutex_read_unlock(Mutex_RW self)
	{
		_deadlock_detector_mutex_unset_owner(self);
		ReleaseSRWLockShared(&self->lock);
	}

	void
	mutex_write_lock(Mutex_RW self)
	{
		if (TryAcquireSRWLockExclusive(&self->lock))
		{
			_deadlock_detector_mutex_set_exclusive_owner(self);
			return;
		}

		worker_block_ahead();
		_deadlock_detector_mutex_block(self);
		AcquireSRWLockExclusive(&self->lock);
		_deadlock_detector_mutex_set_exclusive_owner(self);
		worker_block_clear();
	}

	void
	mutex_write_unlock(Mutex_RW self)
	{
		_deadlock_detector_mutex_unset_owner(self);
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

#ifndef NDEBUG
		// Setting a thread name is useful when debugging
		{
			/*
			 * SetThreadDescription may not be available on this version of Windows,
			 * and testing the operating system version is not the best way to do this.
			 * First, it requires the application/library to be manifested for Windows 10,
			 * and even then, the functionality may be available through a redistributable DLL.
			 *
			 * Instead, check that the function exists in the appropriate library (kernel32.dll).
			 *
			 * Reference: (https://docs.microsoft.com/en-us/windows/win32/sysinfo/operating-system-version)
			 */
			HMODULE kernel = LoadLibrary(L"kernel32.dll");
			if (kernel != nullptr)
			{
				mn_defer(FreeLibrary(kernel));

				using prototype = HRESULT(*)(HANDLE, PCWSTR);
				auto set_thread_description = (prototype)GetProcAddress(kernel, "SetThreadDescription");
				if (set_thread_description != nullptr)
				{
					int buffer_size = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
					Block buffer = alloc(buffer_size * sizeof(WCHAR), alignof(WCHAR));
					mn_defer(free(buffer));
					MultiByteToWideChar(CP_UTF8, 0, name, -1, (LPWSTR)buffer.ptr, buffer_size);

					set_thread_description(self->handle, (LPWSTR)buffer.ptr);
				}
			}
		}
#endif

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
		worker_block_ahead();
		if(self->handle)
		{
			[[maybe_unused]] DWORD result = WaitForSingleObject(self->handle, INFINITE);
			assert(result == WAIT_OBJECT_0);
		}
		worker_block_clear();
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
		worker_block_ahead();
		_deadlock_detector_mutex_unset_owner(mtx);
		SleepConditionVariableCS(&self->cv, &mtx->cs, INFINITE);
		_deadlock_detector_mutex_set_exclusive_owner(mtx);
		worker_block_clear();
	}

	Cond_Var_Wake_State
	cond_var_wait_timeout(Cond_Var self, Mutex mtx, uint32_t millis)
	{
		worker_block_ahead();
		_deadlock_detector_mutex_unset_owner(mtx);
		auto res = SleepConditionVariableCS(&self->cv, &mtx->cs, millis);
		_deadlock_detector_mutex_set_exclusive_owner(mtx);
		worker_block_clear();

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

	// Waitgroup
#if MN_WAITGROUP_FUTEX
	void
	waitgroup_wait(Waitgroup& self)
	{
		auto v = self.load();
		if (v == 0)
			return;

		worker_block_ahead();
		[[maybe_unused]] auto res = WaitOnAddress(&self, &v, sizeof(self), INFINITE);
		assert(res == TRUE);
		worker_block_clear();
	}

	void
	waitgroup_wake(Waitgroup& self)
	{
		WakeByAddressAll(&self);
	}
#else
	void
	waitgroup_wait(Waitgroup& self)
	{
		worker_block_ahead();

		constexpr int SPIN_LIMIT = 128;
		int spin_count = 0;

		while(self.load() > 0)
		{
			if (spin_count < SPIN_LIMIT)
			{
				++spin_count;
				_mm_pause();
			}
			else
			{
				thread_sleep(1);
			}
		}

		worker_block_clear();
	}

	void
	waitgroup_wake(Waitgroup& self)
	{
		// do nothing
	}
#endif
}
