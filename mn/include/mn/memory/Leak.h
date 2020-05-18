#pragma once

#include "mn/Exports.h"
#include "mn/memory/Interface.h"
#include "mn/Base.h"
#include "mn/Str.h"
#include "mn/Thread.h"

#include <stdint.h>
#include <stddef.h>

namespace mn::memory
{
	struct Leak: Interface
	{
		constexpr static inline int CALLSTACK_MAX_FRAMES = 20;
		struct Node
		{
			size_t size;
			void* callstack[CALLSTACK_MAX_FRAMES];
			Node* next;
			Node* prev;
		};

		Node* head;
		Mutex mtx;

		MN_EXPORT
		Leak();

		MN_EXPORT
		~Leak() override;

		MN_EXPORT Block
		alloc(size_t size, uint8_t alignment) override;

		MN_EXPORT void
		free(Block block) override;
	};

	MN_EXPORT Leak*
	leak();
}
