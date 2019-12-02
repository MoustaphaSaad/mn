#include "mn/memory/CLib.h"
#include "mn/Context.h"

#include <stdlib.h>

namespace mn::memory
{
	Block
	CLib::alloc(size_t size, uint8_t)
	{
		Block res{};
		res.ptr = ::malloc(size);
		res.size = res.ptr ? size : 0;
		memory_profile_alloc(res.ptr, res.size);
		return res;
	}

	void
	CLib::free(Block block)
	{
		memory_profile_free(block.ptr, block.size);
		::free(block.ptr);
	}

	CLib*
	clib()
	{
		static CLib _clib_allocator;
		return &_clib_allocator;
	}
}
