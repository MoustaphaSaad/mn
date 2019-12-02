#include "mn/Context.h"
#include "mn/Memory.h"
#include "mn/memory/Leak.h"
#include "mn/memory/Fast_Leak.h"
#include "mn/Stream.h"
#include "mn/Reader.h"
#include "mn/Memory_Stream.h"

#include <assert.h>
#include <stdio.h>

namespace mn
{
	static Memory_Profile_Interface MEMORY_PROFILE;

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
		auto res = MEMORY_PROFILE;
		MEMORY_PROFILE = self;
		return res;
	}

	void
	memory_profile_alloc(void* ptr, size_t size)
	{
		if(MEMORY_PROFILE.profile_alloc)
			MEMORY_PROFILE.profile_alloc(MEMORY_PROFILE.self, ptr, size);
	}

	void
	memory_profile_free(void* ptr, size_t size)
	{
		if(MEMORY_PROFILE.profile_free)
			MEMORY_PROFILE.profile_free(MEMORY_PROFILE.self, ptr, size);
	}
}
