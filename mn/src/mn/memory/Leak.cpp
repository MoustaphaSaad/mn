#include "mn/memory/Leak.h"
#include "mn/memory/CLib.h"
#include "mn/Debug.h"
#include "mn/Context.h"
#include "mn/File.h"
#include "mn/OS.h"

#include <stdlib.h>
#include <stdio.h>

namespace mn::memory
{
	Leak::Leak()
	{
		this->head = nullptr;
		this->mtx = _leak_allocator_mutex();
		this->report_on_destruct = true;
	}

	Leak::~Leak()
	{
		if (this->report_on_destruct)
			report(false);
	}

	Block
	Leak::alloc(size_t size, uint8_t)
	{
		Node* ptr = (Node*)::malloc(size + sizeof(Node));

		if (ptr == nullptr)
			mn::panic("system out of memory");

		ptr->size = size;
		ptr->prev = nullptr;
		mutex_lock(this->mtx);
			ptr->next = this->head;
			if (this->head != nullptr)
				this->head->prev = ptr;
			this->head = ptr;
		mutex_unlock(this->mtx);

		callstack_capture(ptr->callstack, Leak::CALLSTACK_MAX_FRAMES);
		auto res = Block{ ptr + 1, size };
		_memory_profile_alloc(res.ptr, res.size);
		return res;
	}

	void
	Leak::free(Block block)
	{
		if (block_is_empty(block) == false)
		{
			Node* ptr = ((Node*)block.ptr) - 1;

			mutex_lock(this->mtx);
			if (ptr == this->head)
				this->head = ptr->next;

			if (ptr->prev)
				ptr->prev->next = ptr->next;

			if (ptr->next)
				ptr->next->prev = ptr->prev;
			mutex_unlock(this->mtx);

			_memory_profile_free(block.ptr, block.size);
			::free(ptr);
		}
	}

	void
	Leak::report(bool report_on_destruct_)
	{
		this->report_on_destruct = report_on_destruct_;
		if (this->head == nullptr)
			return;

		size_t count = 0;
		size_t size = 0;
		auto it = head;
		while (it)
		{
			::fprintf(stderr, "Leak size: %zu, call stack:\n", it->size);
			#if DEBUG
				callstack_print_to(it->callstack, Leak::CALLSTACK_MAX_FRAMES, file_stderr());
			#else
				::fprintf(stderr, "run in debug mode to get call stack info\n");
			#endif

			auto ptr = (char*)(it + 1);
			size_t len = it->size > 128 ? 128 : it->size;

			::fprintf(stderr, "content bytes[%zu]: {", len);
			for (size_t i = 0; i < len; ++i)
			{
				if (i + 1 < len)
					::fprintf(stderr, "%#02x, ", ptr[i]);
				else
					::fprintf(stderr, "%#02x", ptr[i]);
			}
			::fprintf(stderr, "}\n");

			::fprintf(stderr, "content string[%zu]: '", len);
			for (size_t i = 0; i < len; ++i)
				::fprintf(stderr, "%c", ptr[i]);
			::fprintf(stderr, "'\n\n");

			++count;
			size += it->size;
			it = it->next;
		}
		::fprintf(stderr, "Leaks count: %zu, Leaks size(bytes): %zu\n", count, size);
	}

	Leak*
	leak()
	{
		static Leak _leak_allocator;
		return &_leak_allocator;
	}
}
