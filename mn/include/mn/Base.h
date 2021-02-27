#pragma once

#include "mn/Exports.h"

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

namespace mn
{
	/**
	 * @brief      A Representation of a block of memory
	 */
	struct Block
	{
		//pointer to the memory block
		void*  ptr;

		//size of the memory block in bytes
		size_t size;
	};

	inline static bool
	block_is_empty(Block self)
	{
		return self.ptr == nullptr || self.size == 0;
	}

	/**
	 * @brief      Sets all the bytes in the block to 0
	 *
	 * @param[in]  block  The block to set
	 */
	MN_EXPORT void
	block_zero(Block block);

	/**
	 * @brief      Wraps a C string literal into a block
	 *
	 * @param[in]  str   The C string
	 *
	 * @return     A Block enclosing the string without the null termination
	 */
	MN_EXPORT Block
	block_lit(const char* str);

	/**
	 * @brief      Wraps any value T into a block
	 *
	 * @param[in]  value  The Value to be wraped
	 *
	 * @tparam     T          The Type of the value
	 *
	 * @return     A Block enclosing the value
	 */
	template<typename T>
	inline static Block
	block_from(const T& value)
	{
		return Block {(void*)&value, sizeof(T)};
	}

	/**
	 * @brief      Wraps a pointer type to a block (assumes pointer
	 * 			   is pointing to a single instance of type T)
	 *
	 * @param[in]  value  The Pointer to the value to be wraped
	 *
	 * @tparam     T      The Type of the pointer
	 *
	 * @return     A Block enclosing the object which the pointer points to (uses `sizeof(T)`)
	 */
	template<typename T>
	inline static Block
	block_from_ptr(const T* value)
	{
		return Block {(void*)value, sizeof(T)};
	}

	//offseting the block
	/**
	 * @brief      Advances the block pointer and decreases the size, think of the offset
	 * 			   as a point which divides the block in two and we always take the right half
	 *
	 * @param[in]  block   The block
	 * @param[in]  offset  The offset
	 *
	 * @return     The New shrunk block
	 */
	inline static Block
	operator+(const Block& block, size_t offset)
	{
		assert(block.size >= offset);
		return Block { (char*)block.ptr + offset, block.size - offset };
	}

	/**
	 * @brief      Advances the block pointer and decreases the size, think of the offset
	 * 			   as a point which divides the block in two and we always take the right half
	 *
	 * @param[in]  offset  The offset
	 * @param[in]  block   The block
	 *
	 * @return     The New shrunk block
	 */
	inline static Block
	operator+(size_t offset, const Block& block)
	{
		assert(block.size >= offset);
		return Block { (char*)block.ptr + offset, block.size - offset };
	}

	/**
	 * @brief      Moves the block pointer backwards and increases the size, think of
	 * 			   the offset as a point that lays at the left of the block and we then
	 * 			   return the new block which starts at the offset point and encloses till
	 * 			   the end of the block
	 *
	 * @param[in]  block   The block
	 * @param[in]  offset  The offset
	 *
	 * @return     The New enlarged block
	 */
	inline static Block
	operator-(const Block& block, size_t offset)
	{
		return Block { (char*)block.ptr - offset, block.size + offset };
	}

	/**
	 * @brief      Moves the block pointer backwards and increases the size, think of
	 * 			   the offset as a point that lays at the left of the block and we then
	 * 			   return the new block which starts at the offset point and encloses till
	 * 			   the end of the block
	 *
	 * @param[in]  offset  The offset
	 * @param[in]  block   The block
	 *
	 * @return     The New enlarged block
	 */
	inline static Block
	operator-(size_t offset, const Block& block)
	{
		return Block { (char*)block.ptr - offset, block.size + offset };
	}

	struct Timeout
	{
		uint64_t milliseconds;

		bool operator==(Timeout other) const { return milliseconds == other.milliseconds; }
		bool operator!=(Timeout other) const { return milliseconds != other.milliseconds; }
	};

	constexpr inline Timeout NO_TIMEOUT{ 0 };
	constexpr inline Timeout INFINITE_TIMEOUT{ 0xFFFFFFFFFFFFFFFF };

	// This is a tracy compatible source location struct that's used with mutexes
	struct Source_Location
	{
		const char* name;
		const char* function;
		const char* file;
		uint32_t line;
		uint32_t color;
	};
}
