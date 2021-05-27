#pragma once

#include "mn/Exports.h"

#include <stddef.h>

namespace mn
{
	namespace memory
	{
		struct Interface;
		struct Arena;
	}

	using Allocator = memory::Interface*;

	typedef struct IReader* Reader;

	struct Context
	{
		//allocators data
		inline static constexpr size_t ALLOCATOR_CAPACITY = 1024;
		Allocator _allocator_stack[ALLOCATOR_CAPACITY];
		size_t _allocator_stack_count;

		//tmp allocator
		memory::Arena* _allocator_tmp;

		//Local tmp stream
		Reader reader_tmp;
	};

	MN_EXPORT void
	context_init(Context* self);

	MN_EXPORT void
	context_free(Context* self);

	inline static void
	destruct(Context* self)
	{
		context_free(self);
	}

	MN_EXPORT Context*
	context_local(Context* new_context = nullptr);

	/**
	 * Allocators are organized in a per thread stack so that you can change the allocator of any
	 * part of the code even if it doesn't support custom allocator by using
	 * allocator_push() and allocator_pop()
	 * At the base of the stack is the clib_allocator and it can't be popped
	 *
	 * @return     The current top of allocator stack of the calling thread
	 */
	MN_EXPORT Allocator
	allocator_top();

	/**
	 * @brief      Pushes the given allocator to the top of the calling thread allocator stack
	 */
	MN_EXPORT void
	allocator_push(Allocator allocator);

	/**
	 * @brief      Pops an allocator off the calling thread allocator stack
	 */
	MN_EXPORT void
	allocator_pop();

	namespace memory
	{
		MN_EXPORT Arena*
		tmp();
	}

	MN_EXPORT memory::Arena*
	_memory_tmp_set(memory::Arena* a);

	MN_EXPORT Reader
	reader_tmp();

	// Profiling Wrapper
	struct Memory_Profile_Interface
	{
		void* self;
		void (*profile_alloc)(void* self, void* ptr, size_t size);
		void (*profile_free)(void* self, void* ptr, size_t size);
	};

	MN_EXPORT Memory_Profile_Interface
	memory_profile_interface_set(Memory_Profile_Interface self);

	MN_EXPORT void
	_memory_profile_alloc(void* ptr, size_t size);

	MN_EXPORT void
	_memory_profile_free(void* ptr, size_t size);

	// Log Wrapper
	struct Log_Interface
	{
		void* self;
		void (*debug)(void* self, const char* msg);
		void (*info)(void* self, const char* msg);
		void (*warning)(void* self, const char* msg);
		void (*error)(void* self, const char* msg);
		void (*critical)(void* self, const char* msg);
	};

	MN_EXPORT Log_Interface
	log_interface_set(Log_Interface self);

	MN_EXPORT void
	_log_debug_str(const char* msg);

	MN_EXPORT void
	_log_info_str(const char* msg);

	MN_EXPORT void
	_log_warning_str(const char* msg);

	MN_EXPORT void
	_log_error_str(const char* msg);

	MN_EXPORT void
	_log_critical_str(const char* msg);

	// Thread Profiling Wrapper
	typedef struct IThread* Thread;
	typedef struct IMutex* Mutex;
	typedef struct IMutex_RW* Mutex_RW;

	struct Thread_Profile_Interface
	{
		// Thread hooks functions

		// this function is called for each thread new call with the thread handle and name
		// it's also called from the given thread
		void (*thread_new)(Thread handle, const char* name);

		// Mutex hooks functions
		void* (*mutex_new)(Mutex handle, const char* name);
		void (*mutex_free)(Mutex handle, void* user_data);
		bool (*mutex_before_lock)(Mutex handle, void* user_data);
		void (*mutex_after_lock)(Mutex handle, void* user_data);
		void (*mutex_after_unlock)(Mutex handle, void* user_data);

		// Mutex RW hooks functions
		void* (*mutex_rw_new)(Mutex_RW handle, const char* name);
		void (*mutex_rw_free)(Mutex_RW handle, void* user_data);
		bool (*mutex_before_read_lock)(Mutex_RW handle, void* user_data);
		void (*mutex_after_read_lock)(Mutex_RW handle, void* user_data);
		bool (*mutex_before_write_lock)(Mutex_RW handle, void* user_data);
		void (*mutex_after_write_lock)(Mutex_RW handle, void* user_data);
		void (*mutex_after_read_unlock)(Mutex_RW handle, void* user_data);
		void (*mutex_after_write_unlock)(Mutex_RW handle, void* user_data);
	};

	// used to setup the global threading profiling interface
	MN_EXPORT Thread_Profile_Interface
	thread_profile_interface_set(Thread_Profile_Interface self);

	MN_EXPORT void
	_thread_new(Thread handle, const char* name);

	MN_EXPORT void*
	_mutex_new(Mutex handle, const char* name);

	MN_EXPORT void
	_mutex_free(Mutex handle, void* user_data);

	MN_EXPORT bool
	_mutex_before_lock(Mutex handle, void* user_data);

	MN_EXPORT void
	_mutex_after_lock(Mutex handle, void* user_data);

	MN_EXPORT void
	_mutex_after_unlock(Mutex handle, void* user_data);

	MN_EXPORT void*
	_mutex_rw_new(Mutex_RW handle, const char* name);

	MN_EXPORT void
	_mutex_rw_free(Mutex_RW handle, void* user_data);

	MN_EXPORT bool
	_mutex_before_read_lock(Mutex_RW handle, void* user_data);

	MN_EXPORT void
	_mutex_after_read_lock(Mutex_RW handle, void* user_data);

	MN_EXPORT bool
	_mutex_before_write_lock(Mutex_RW handle, void* user_data);

	MN_EXPORT void
	_mutex_after_write_lock(Mutex_RW handle, void* user_data);

	MN_EXPORT void
	_mutex_after_read_unlock(Mutex_RW handle, void* user_data);

	MN_EXPORT void
	_mutex_after_write_unlock(Mutex_RW handle, void* user_data);

	MN_EXPORT void
	_disable_profiling_for_this_thread();
}
