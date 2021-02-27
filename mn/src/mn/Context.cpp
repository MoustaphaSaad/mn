#include "mn/Context.h"
#include "mn/Memory.h"
#include "mn/memory/Leak.h"
#include "mn/memory/Fast_Leak.h"
#include "mn/Stream.h"
#include "mn/Reader.h"
#include "mn/Memory_Stream.h"
#include "mn/Fmt.h"

#include <assert.h>
#include <stdio.h>

namespace mn
{
	static Memory_Profile_Interface MEMORY;
	static Log_Interface LOG;
	static Thread_Profile_Interface THREAD;
	thread_local bool PROFILING_DISABLED = false;

	struct Context_Wrapper
	{
		Context self;

		Context_Wrapper()
		{
			context_init(&self);
		}

		~Context_Wrapper()
		{
			#if DEBUG
				fprintf(stderr, "Temp Allocator %p: %zu bytes used at exit, %zu bytes highwater mark\n",
					self._allocator_tmp, self._allocator_tmp->used_mem, self._allocator_tmp->highwater_mem);
			#endif

			context_free(&self);
		}
	};

	inline static Context_Wrapper*
	_context_wrapper()
	{
		thread_local Context_Wrapper _CONTEXT;
		return &_CONTEXT;
	}
	thread_local Context* _CURRENT_CONTEXT = nullptr;


	//API
	void
	context_init(Context* self)
	{
		for (size_t i = 0; i < Context::ALLOCATOR_CAPACITY; ++i)
			self->_allocator_stack[i] = nullptr;

			#if DEBUG
				#if MN_LEAK
					self->_allocator_stack[0] = memory::leak();
				#else
					self->_allocator_stack[0] = memory::fast_leak();
				#endif
			#else
				self->_allocator_stack[0] = memory::clib();
			#endif
		self->_allocator_stack_count = 1;

		self->_allocator_tmp = alloc_construct_from<memory::Arena>(memory::clib(), 4ULL * 1024ULL * 1024ULL, memory::clib());

		self->reader_tmp = reader_new(nullptr, memory::clib());
	}

	void
	context_free(Context* self)
	{
		free_destruct_from(memory::clib(), self->_allocator_tmp);
		reader_free(self->reader_tmp);
	}

	Context*
	context_local(Context* new_context)
	{
		if (_CURRENT_CONTEXT == nullptr)
			_CURRENT_CONTEXT = &_context_wrapper()->self;

		if(new_context == nullptr)
			return _CURRENT_CONTEXT;

		Context* res = _CURRENT_CONTEXT;
		_CURRENT_CONTEXT = new_context;
		return res;
	}

	Allocator
	allocator_top()
	{
		Context* self = context_local();
		return self->_allocator_stack[self->_allocator_stack_count - 1];
	}

	void
	allocator_push(Allocator allocator)
	{
		Context* self = context_local();
		assert(self->_allocator_stack_count < Context::ALLOCATOR_CAPACITY);
		self->_allocator_stack[self->_allocator_stack_count++] = allocator;
	}

	void
	allocator_pop()
	{
		Context* self = context_local();
		assert(self->_allocator_stack_count > 0);
		--self->_allocator_stack_count;
	}

	namespace memory
	{
		Arena*
		tmp()
		{
			return context_local()->_allocator_tmp;
		}
	}

	Reader
	reader_tmp()
	{
		return context_local()->reader_tmp;
	}

	Memory_Profile_Interface
	memory_profile_interface_set(Memory_Profile_Interface self)
	{
		auto res = MEMORY;
		MEMORY = self;
		return res;
	}

	void
	_memory_profile_alloc(void* ptr, size_t size)
	{
		if (PROFILING_DISABLED)
			return;

		if(MEMORY.profile_alloc)
			MEMORY.profile_alloc(MEMORY.self, ptr, size);
	}

	void
	_memory_profile_free(void* ptr, size_t size)
	{
		if (PROFILING_DISABLED)
			return;

		if(MEMORY.profile_free)
			MEMORY.profile_free(MEMORY.self, ptr, size);
	}

	Log_Interface
	log_interface_set(Log_Interface self)
	{
		auto res = LOG;
		LOG = self;
		return res;
	}

	void
	_log_debug_str(const char* msg)
	{
		if (LOG.debug)
			LOG.debug(LOG.self, msg);
		else
			mn::printerr("[debug]: {}\n", msg);
	}

	void
	_log_info_str(const char* msg)
	{
		if (LOG.info)
			LOG.info(LOG.self, msg);
		else
			mn::printerr("[info]: {}\n", msg);
	}

	void
	_log_warning_str(const char* msg)
	{
		if (LOG.warning)
			LOG.warning(LOG.self, msg);
		else
			mn::printerr("[warning]: {}\n", msg);
	}

	void
	_log_error_str(const char* msg)
	{
		if (LOG.error)
			LOG.error(LOG.self, msg);
		else
			mn::printerr("[error]: {}\n", msg);
	}

	void
	_log_critical_str(const char* msg)
	{
		if (LOG.critical)
			LOG.critical(LOG.self, msg);
		else
			mn::printerr("[critical]: {}\n", msg);
	}

	Thread_Profile_Interface
	thread_profile_interface_set(Thread_Profile_Interface self)
	{
		auto res = THREAD;
		THREAD = self;
		return res;
	}

	void
	_thread_new(Thread handle, const char* name)
	{
		if (PROFILING_DISABLED)
			return;

		if (THREAD.thread_new)
			THREAD.thread_new(handle, name);
	}

	void*
	_mutex_new(Mutex handle, const char* name)
	{
		if (PROFILING_DISABLED)
			return nullptr;

		if (THREAD.mutex_new)
			return THREAD.mutex_new(handle, name);
		return nullptr;
	}

	void
	_mutex_free(Mutex handle, void* user_data)
	{
		if (PROFILING_DISABLED)
			return;

		if (THREAD.mutex_free)
			THREAD.mutex_free(handle, user_data);
	}

	bool
	_mutex_before_lock(Mutex handle, void* user_data)
	{
		if (PROFILING_DISABLED)
			return false;

		if (THREAD.mutex_before_lock)
			return THREAD.mutex_before_lock(handle, user_data);
		return false;
	}

	void
	_mutex_after_lock(Mutex handle, void* user_data)
	{
		if (PROFILING_DISABLED)
			return;

		if (THREAD.mutex_after_lock)
			THREAD.mutex_after_lock(handle, user_data);
	}

	void
	_mutex_after_unlock(Mutex handle, void* user_data)
	{
		if (PROFILING_DISABLED)
			return;

		if (THREAD.mutex_after_unlock)
			THREAD.mutex_after_unlock(handle, user_data);
	}

	void*
	_mutex_rw_new(Mutex_RW handle, const char* name)
	{
		if (PROFILING_DISABLED)
			return nullptr;

		if (THREAD.mutex_rw_new)
			return THREAD.mutex_rw_new(handle, name);

		return nullptr;
	}

	void
	_mutex_rw_free(Mutex_RW handle, void* user_data)
	{
		if (PROFILING_DISABLED)
			return;

		if (THREAD.mutex_rw_free)
			THREAD.mutex_rw_free(handle, user_data);
	}

	bool
	_mutex_before_read_lock(Mutex_RW handle, void* user_data)
	{
		if (PROFILING_DISABLED)
			return false;

		if (THREAD.mutex_before_read_lock)
			return THREAD.mutex_before_read_lock(handle, user_data);
		return false;
	}

	void
	_mutex_after_read_lock(Mutex_RW handle, void* user_data)
	{
		if (PROFILING_DISABLED)
			return;

		if (THREAD.mutex_after_read_lock)
			THREAD.mutex_after_read_lock(handle, user_data);
	}

	bool
	_mutex_before_write_lock(Mutex_RW handle, void* user_data)
	{
		if (PROFILING_DISABLED)
			return false;

		if (THREAD.mutex_before_write_lock)
			return THREAD.mutex_before_write_lock(handle, user_data);
		return false;
	}

	void
	_mutex_after_write_lock(Mutex_RW handle, void* user_data)
	{
		if (PROFILING_DISABLED)
			return;

		if (THREAD.mutex_after_write_lock)
			THREAD.mutex_after_write_lock(handle, user_data);
	}

	void
	_mutex_after_read_unlock(Mutex_RW handle, void* user_data)
	{
		if (PROFILING_DISABLED)
			return;

		if (THREAD.mutex_after_read_unlock)
			THREAD.mutex_after_read_unlock(handle, user_data);
	}

	void
	_mutex_after_write_unlock(Mutex_RW handle, void* user_data)
	{
		if (PROFILING_DISABLED)
			return;

		if (THREAD.mutex_after_write_unlock)
			THREAD.mutex_after_write_unlock(handle, user_data);
	}

	void
	_disable_profiling_for_this_thread()
	{
		PROFILING_DISABLED = true;
	}
}
