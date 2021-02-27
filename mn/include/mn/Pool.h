#pragma once

#include "mn/Exports.h"
#include "mn/Base.h"
#include "mn/Thread.h"
#include "mn/Memory.h"

namespace mn
{
	/**
	 * Pool handle
	 */
	typedef struct IPool* Pool;

	/**
	 * @brief      Creates a new memory pool
	 * A Memory pool is a list of buckets with `bucket_size` capacity which it uses to hand
	 * over a memory piece with the given `element_size` every time you call `pool_get`
	 *
	 * @param[in]  element_size    The element size (in bytes)
	 * @param[in]  bucket_size     The bucket size (in elements count)
	 * @param[in]  meta_allocator  The meta allocator for the pool to allocator from
	 */
	MN_EXPORT Pool
	pool_new(size_t element_size, size_t bucket_size, Allocator meta_allocator = allocator_top());

	/**
	 * @brief      Frees the memory pool
	 *
	 * @param[in]  pool  The pool
	 */
	MN_EXPORT void
	pool_free(Pool pool);

	/**
	 * @brief      Destruct function overload for the pool
	 *
	 * @param[in]  pool  The pool
	 */
	inline static void
	destruct(Pool pool)
	{
		pool_free(pool);
	}

	/**
	 * @brief      Returns a memory suitable to write an object of size `element_size` in
	 *
	 * @param[in]  pool  The pool
	 */
	MN_EXPORT void*
	pool_get(Pool pool);

	/**
	 * @brief      Puts back the given memory into the pool to be reused later
	 *
	 * @param[in]  pool  The pool
	 * @param      ptr   The pointer to memory
	 */
	MN_EXPORT void
	pool_put(Pool pool, void* ptr);
}