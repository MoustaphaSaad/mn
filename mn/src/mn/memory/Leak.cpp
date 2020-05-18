#include "mn/memory/Leak.h"
#include "mn/memory/CLib.h"
#include "mn/Debug.h"
#include "mn/Context.h"
#include "mn/File.h"

#include <stdlib.h>
#include <stdio.h>

namespace mn::memory
{
	Leak::Leak()
	{
		this->head = nullptr;
		this->mtx = _leak_allocator_mutex();
	}

	Leak::~Leak()
	{
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

			++count;
			size += it->size;
			it = it->next;
		}
		::fprintf(stderr, "Leaks count: %zu, Leaks size(bytes): %zu\n", count, size);
		::fprintf(stderr, "Press any key to continue...\n");
		::getchar();
	}

	Block
	Leak::alloc(size_t size, uint8_t)
	{
		Node* ptr = (Node*)::malloc(size + sizeof(Node));

		if (ptr == nullptr)
			return Block{};

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
		memory_profile_alloc(res.ptr, res.size);
		return res;
	}

	void
	Leak::free(Block block)
	{
		if (block)
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

			memory_profile_free(block.ptr, block.size);
			::free(ptr);
		}
	}

	Leak*
	leak()
	{
		static Leak _leak_allocator;
		return &_leak_allocator;
	}
}
