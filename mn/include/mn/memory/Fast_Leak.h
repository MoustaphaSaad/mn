#pragma once

#include "mn/Exports.h"
#include "mn/memory/Interface.h"
#include "mn/Base.h"
#include "mn/Str.h"
#include "mn/Thread.h"

#include <atomic>
#include <stdint.h>

namespace mn::memory
{
	struct Fast_Leak: Interface
	{
		std::atomic<size_t> atomic_size;
		std::atomic<size_t> atomic_count;

		MN_EXPORT
		Fast_Leak();

		MN_EXPORT
		~Fast_Leak() override;

		MN_EXPORT Block
		alloc(size_t size, uint8_t alignment) override;

		MN_EXPORT void
		free(Block block) override;
	};

	MN_EXPORT Fast_Leak*
	fast_leak();
}
