#pragma once

#include "mn/Base.h"
#include "mn/Memory.h"

#include <string.h>
#include <assert.h>

#include <initializer_list>

namespace mn
{
	/**
	 * @brief      A Buf is a growable area of memory
	 *
	 * @tparam     T     Element type
	 */
	template<typename T>
	struct Buf
	{
		Allocator allocator;
		T* ptr;
		size_t count;
		size_t cap;

		T&
		operator[](size_t ix)
		{
			assert(ix < count);
			return ptr[ix];
		}

		const T&
		operator[](size_t ix) const
		{
			assert(ix < count);
			return ptr[ix];
		}
	};

	/**
	 * @brief      Creates a new buf using the top allocator
	 */
	template<typename T>
	inline static Buf<T>
	buf_new()
	{
		Buf<T> self{};
		self.allocator = allocator_top();
		return self;
	}

	/**
	 * @brief      Creates a new buf using the given allocator
	 *
	 * @param[in]  allocator  The allocator
	 */
	template<typename T>
	inline static Buf<T>
	buf_with_allocator(Allocator allocator)
	{
		Buf<T> self{};
		self.allocator = allocator;
		return self;
	}

	/**
	 * @brief      Creates a new buf using the given values initializer_list
	 *
	 * @param[in]  values  The values
	 */
	template<typename T>
	inline static Buf<T>
	buf_lit(const std::initializer_list<T>& values)
	{
		Buf<T> self = buf_new<T>();
		buf_reserve(self, values.size());
		for(const auto& val: values)
			self.ptr[self.count++] = val;
		return self;
	}

	/**
	 * @brief      Creates a buf with resized to the given count
	 *
	 * @param[in]  count  The count
	 */
	template<typename T>
	inline static Buf<T>
	buf_with_count(size_t count)
	{
		Buf<T> self = buf_new<T>();
		buf_resize(self, count);
		return self;
	}

	/**
	 * @brief      Creates a buf with capacity to hold the given count
	 *
	 * @param[in]  cap  The capacity
	 */
	template<typename T>
	inline static Buf<T>
	buf_with_capacity(size_t cap)
	{
		Buf<T> self = buf_new<T>();
		buf_reserve(self, cap);
		return self;
	}

	/**
	 * @brief      Frees the given buf
	 */
	template<typename T>
	inline static void
	buf_free(Buf<T>& self)
	{
		if(self.cap && self.allocator)
			free_from(self.allocator, Block{ self.ptr, self.cap * sizeof(T) });
		self.cap = 0;
		self.count = 0;
		self.ptr = nullptr;
	}

	/**
	 * @brief      A General destruct function which calls the destructor of the given value
	 *
	 * @param      value  The value
	 */
	template<typename T>
	inline static void
	destruct(T& value)
	{
		value.~T();
	}

	inline static void destruct(char&) {}
	inline static void destruct(uint8_t&) {}
	inline static void destruct(uint16_t&) {}
	inline static void destruct(uint32_t&) {}
	inline static void destruct(uint64_t&) {}
	inline static void destruct(int8_t&) {}
	inline static void destruct(int16_t&) {}
	inline static void destruct(int32_t&) {}
	inline static void destruct(int64_t&) {}
	inline static void destruct(float&) {}
	inline static void destruct(double&) {}
	inline static void destruct(long double&) {}

	/**
	 * @brief      A Custom destruct function for the buf which iterates over all the elements
	 * and calls destruct on each element thus useful for destructing a big hierarchy 
	 * without memory leaks
	 *
	 * @param      self  The buf
	 */
	template<typename T>
	inline static void
	destruct(Buf<T>& self)
	{
		for(size_t i = 0; i < self.count; ++i)
			destruct(self[i]);
		buf_free(self);
	}

	/**
	 * @brief      indexes the buffer with signed integers like python
	 * buf_of(self, 0) = self[0]
	 * buf_of(self, 1) = self[1]
	 * buf_of(self, -1) = self[self.count - 1]
	 *
	 * @param      self  The buffer
	 * @param[in]  ix    The index
	 */
	template<typename T>
	inline static T&
	buf_of(Buf<T>& self, int ix)
	{
		if(ix < 0)
			ix = self.count + ix;
		assert(ix >= 0);
		assert(ix < self.count);
		return self.ptr[ix];
	}

	/**
	 * @brief      indexes the buffer with signed integers like python
	 * buf_of(self, 0) = self[0]
	 * buf_of(self, 1) = self[1]
	 * buf_of(self, -1) = self[self.count - 1]
	 *
	 * @param      self  The buffer
	 * @param[in]  ix    The index
	 */
	template<typename T>
	inline static const T&
	buf_of(const Buf<T>& self, int ix)
	{
		if(ix < 0)
			ix = self.count + ix;
		assert(ix >= 0);
		assert(ix < self.count);
		return self.ptr[ix];
	}

	/**
	 * @brief      Clears the given buf
	 */
	template<typename T>
	inline static void
	buf_clear(Buf<T>& self)
	{
		self.count = 0;
	}

	/**
	 * @brief      Fills all the values of the given buf with the given value
	 *
	 * @param      self   The buf
	 * @param[in]  value  The value
	 */
	template<typename T, typename R>
	inline static void
	buf_fill(Buf<T>& self, const R& value)
	{
		for(size_t i = 0; i < self.count; ++i)
			self.ptr[i] = value;
	}

	template<typename T>
	inline static void
	_buf_reserve_exact(Buf<T>& self, size_t new_count)
	{
		if (self.allocator == nullptr)
			self.allocator = allocator_top();

		Block new_block = alloc_from(self.allocator,
									 new_count * sizeof(T),
									 alignof(T));
		if(self.count)
			::memcpy(new_block.ptr, self.ptr, self.count * sizeof(T));
		if(self.cap)
			free_from(self.allocator, Block{ self.ptr, self.cap * sizeof(T) });
		self.ptr = (T*)new_block.ptr;
		self.cap = new_count;
	}

	/**
	 * @brief      Ensures the given buf has the capacity to hold the added size
	 *
	 * @param      self        The buf
	 * @param[in]  added_size  The added size
	 */
	template<typename T>
	inline static void
	buf_reserve(Buf<T>& self, size_t added_size)
	{
		if(self.count + added_size <= self.cap)
			return;

		size_t next_cap = size_t(self.cap * 1.5f);
		size_t accurate_cap = self.count + added_size;
		size_t request_cap = next_cap > accurate_cap ? next_cap : accurate_cap;
		_buf_reserve_exact(self, request_cap);
	}

	/**
	 * @brief      Resizes the given buf to the new size
	 *
	 * @param      self      The buf
	 * @param[in]  new_size  The new size
	 */
	template<typename T>
	inline static void
	buf_resize(Buf<T>& self, size_t new_size)
	{
		if(new_size > self.count)
			buf_reserve(self, new_size - self.count);
		self.count = new_size;
	}

	/**
	 * @brief      Resizes the given buf to the new size and in case the new size is bigger than
	 * the current size it fills the new element with the given fill_val
	 *
	 * @param      self      The buf
	 * @param[in]  new_size  The new size
	 * @param[in]  fill_val  The fill value
	 */
	template<typename T, typename U>
	inline static void
	buf_resize_fill(Buf<T>& self, size_t new_size, const U& fill_val)
	{
		if(new_size > self.count)
		{
			buf_reserve(self, new_size - self.count);
			for(size_t i = self.count; i < new_size; ++i)
				self.ptr[i] = fill_val;
		}
		self.count = new_size;
	}

	/**
	 * @brief      Shrinks the given buf to exactly the size of the contained elements
	 */
	template<typename T>
	inline static void
	buf_shrink_to_fit(Buf<T>& self)
	{
		if(self.cap == self.count || self.count == 0)
			return;

		if (self.allocator == nullptr)
			self.allocator = allocator_top();

		Block new_block = alloc_from(self.allocator, self.count * sizeof(T), alignof(T));
		::memcpy(new_block.ptr, self.ptr, self.count * sizeof(T));
		free_from(self.allocator, Block { self.ptr, self.cap * sizeof(T) });
		self.ptr = (T*)new_block.ptr;
		self.cap = self.count;
	}

	/**
	 * @brief      Pushes a new value to the given buf
	 *
	 * @param      self   The buf
	 * @param[in]  value  The value
	 * 
	 * @return     A Pointer to the added element
	 */
	template<typename T, typename R>
	inline static T*
	buf_push(Buf<T>& self, const R& value)
	{
		if(self.count == self.cap)
			buf_reserve(self, self.cap ? self.cap * 2 : 8);

		self.ptr[self.count] = T(value);
		++self.count;
		return self.ptr + self.count - 1;
	}

	/**
	 * @brief      Pushes the given value to the end of the buf n times
	 *
	 * @param      self   The buf
	 * @param[in]  count  The repeating count
	 * @param[in]  value  The value
	 */
	template<typename T>
	inline static void
	buf_pushn(Buf<T>& self, size_t count, const T& value)
	{
		size_t i = self.count;
		buf_resize(self, self.count + count);
		for(; i < self.count; ++i)
			self.ptr[i] = value;
	}

	/**
	 * @brief      insert a new value to the given buf at given index
	 *
	 * @param      self   The buf
	 * @param      index  The insertion position
	 * @param[in]  value  The value
	 *
	 * @return     A Pointer to the added element
	 */
	template<typename T, typename R>
	inline static T*
	buf_insert(Buf<T>& self, size_t index, const R& value)
	{
		assert(index < self.count);
		if (self.count == self.cap)
			buf_reserve(self, self.cap ? self.cap * 2 : 8);
		::memmove(self.ptr + index + 1, self.ptr + index, (self.count - index) * sizeof(T));
		++self.count;
		self.ptr[index] = T(value);
		return self.ptr + index;
	}

	/**
	 * @brief      remove a value to the given buf at given index
	 *
	 * @param      self   The buf
	 * @param      index  The deletion position
	 * @param[in]  value  The value
	 *
	 * @return     void
	 */
	template<typename T>
	inline static void
	buf_remove_ordered(Buf<T>& self, size_t index)
	{
		assert(index < self.count);
		::memmove(self.ptr + index, self.ptr + index + 1, (self.count - index - 1) * sizeof(T));
		--self.count;
	}

	/**
	 * @brief      Concats two Bufs together
	 *
	 * @param      self   The buf to concatenate to
	 * @param[in]  begin  The begin
	 * @param[in]  end    The end
	 */
	template<typename T>
	inline static void
	buf_concat(Buf<T>& self, const T* begin, const T* end)
	{
		size_t added_count = end - begin;
		size_t old_count = self.count;
		buf_resize(self, old_count + added_count);
		::memcpy(self.ptr + old_count, begin, added_count * sizeof(T));
	}

	/**
	 * @brief  update self buffer after concatenating other.
	 *
	 * @param[in]  first buffer to concat.
	 * @param[in]  second buffer to concat.
	 */
	template<typename T>
	inline static void
	buf_concat(Buf<T>& self, const Buf<T>& other)
	{
		buf_concat(self, other.ptr, other.ptr + other.count);
	}


	/**
	 * @brief      Pops the last element of the buf
	 */
	template<typename T>
	inline static void
	buf_pop(Buf<T>& self)
	{
		assert(self.count > 0);
		--self.count;
	}

	/**
	 * @brief      Removes any data which the predicate returns true on
	 *
	 * @param      self     The buf
	 * @param      pred     The predicate
	 */
	template<typename T, typename TLambda>
	inline static void
	buf_remove_if(Buf<T>& self, TLambda&& pred)
	{
		T* begin_it = self.ptr;
		T* end_it = self.ptr + self.count;
		T* front_it = self.ptr;

		for(T* it = begin_it; it != end_it; ++it)
		{
			if(pred(*it) == false)
			{
				*front_it = *it;
				++front_it;
			}
		}

		self.count -= end_it - front_it;
	}

	/**
	 * @brief      Removes the element in the given index (will not keep order)
	 *
	 * @param      self  The buf
	 * @param[in]  ix    The index
	 */
	template<typename T>
	inline static void
	buf_remove(Buf<T>& self, size_t ix)
	{
		assert(ix < self.count);
		if(ix + 1 != self.count)
		{
			T tmp = self.ptr[self.count - 1];
			self.ptr[self.count - 1] = self.ptr[ix];
			self.ptr[ix] = tmp;
		}
		--self.count;
	}

	/**
	 * @brief      Removes the element the iterator points to
	 *
	 * @param      self  The buf
	 * @param      it    The iterator
	 */
	template<typename T>
	inline static void
	buf_remove(Buf<T>& self, T* it)
	{
		buf_remove(self, it - self.ptr);
	}

	/**
	 * @brief      Returns a const ref to the last element off the buf
	 */
	template<typename T>
	inline static const T&
	buf_top(const Buf<T>& self)
	{
		assert(self.count > 0);
		return self.ptr[self.count - 1];
	}

	/**
	 * @brief      Returns a ref to the last element off the buf
	 */
	template<typename T>
	inline static T&
	buf_top(Buf<T>& self)
	{
		assert(self.count > 0);
		return self.ptr[self.count - 1];
	}

	/**
	 * @brief      Returns whether the buff is empty or not
	 */
	template<typename T>
	inline static bool
	buf_empty(const Buf<T>& self)
	{
		return self.count == 0;
	}

	/**
	 * @brief      Returns a const pointer to the start of the buf
	 */
	template<typename T>
	inline static const T*
	buf_begin(const Buf<T>& self)
	{
		return self.ptr;
	}

	/**
	 * @brief      Returns a pointer to the start of the buf
	 */
	template<typename T>
	inline static T*
	buf_begin(Buf<T>& self)
	{
		return self.ptr;
	}

	/**
	 * @brief      Returns a const pointer to the end of the buf
	 */
	template<typename T>
	inline static const T*
	buf_end(const Buf<T>& self)
	{
		return self.ptr + self.count;
	}

	/**
	 * @brief      Returns a pointer to the end of the buf
	 */
	template<typename T>
	inline static T*
	buf_end(Buf<T>& self)
	{
		return self.ptr + self.count;
	}

	template<typename T>
	inline static const T*
	begin(const Buf<T>& self)
	{
		return buf_begin(self);
	}

	template<typename T>
	inline static T*
	begin(Buf<T>& self)
	{
		return buf_begin(self);
	}

	template<typename T>
	inline static const T*
	end(const Buf<T>& self)
	{
		return buf_end(self);
	}

	template<typename T>
	inline static T*
	end(Buf<T>& self)
	{
		return buf_end(self);
	}

	/**
	 * @brief      General clone function which returns a new instance of the given value
	 *
	 * @param[in]  value  The value
	 *
	 * @tparam     T      Instance type
	 *
	 * @return     Copy of this object.
	 */
	template<typename T>
	inline static T
	clone(const T& value)
	{
		if constexpr (std::is_integral_v<T>)
		{
			return value;
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			return value;
		}
		else if constexpr (std::is_enum_v<T>)
		{
			return value;
		}
		else if constexpr (std::is_pointer_v<T>)
		{
			return value;
		}
		else if constexpr (std::is_same_v<T, bool>)
		{
			return value;
		}
		else if constexpr (std::is_same_v<T, char>)
		{
			return value;
		}
		else
		{
			static_assert(sizeof(T) == 0, "no clone function found for this type");
			return value;
		}
	}

	/**
	 * @brief      A Custom clone function which iterates over the buf and calls the clone function
	 * over each element thus making a deep copy of the complex structure
	 *
	 * @param[in]  other      The buf to be cloned
	 * @param[in]  allocator  The allocator to be used by the cloned instance
	 *
	 * @return     The newly cloned buf
	 */
	template<typename T>
	inline static Buf<T>
	buf_clone(const Buf<T>& other, Allocator allocator = allocator_top())
	{
		Buf<T> buf = buf_with_allocator<T>(allocator);
		buf_resize(buf, other.count);
		for(size_t i = 0; i < other.count; ++i)
			buf[i] = clone(other[i]);
		return buf;
	}

	/**
	 * @brief      An overload of the general clone function which uses the custom clone
	 * function of the buf
	 *
	 * @param[in]  other  The buf to be cloned
	 *
	 * @return     Copy of this object.
	 */
	template<typename T>
	inline static Buf<T>
	clone(const Buf<T>& other)
	{
		return buf_clone(other);
	}

	// buf_memcpy_clone is used to memcpy the underlying memory of the buf, this is useful as a fast copy for
	// trivially copyable types
	template<typename T>
	inline static Buf<T>
	buf_memcpy_clone(const Buf<T>& other, Allocator allocator = allocator_top())
	{
		Buf<T> buf = buf_with_allocator<T>(allocator);
		buf_resize(buf, other.count);
		::memcpy(buf.ptr, other.ptr, buf.count * sizeof(T));
		return buf;
	}

	#define TRIVIAL_MEMCPY_CLONE(TYPE) \
	inline static Buf<TYPE> \
	clone(const Buf<TYPE>& other) \
	{ \
		return buf_memcpy_clone(other); \
	}

	TRIVIAL_MEMCPY_CLONE(bool)
	TRIVIAL_MEMCPY_CLONE(int8_t)
	TRIVIAL_MEMCPY_CLONE(int16_t)
	TRIVIAL_MEMCPY_CLONE(int32_t)
	TRIVIAL_MEMCPY_CLONE(int64_t)
	TRIVIAL_MEMCPY_CLONE(uint8_t)
	TRIVIAL_MEMCPY_CLONE(uint16_t)
	TRIVIAL_MEMCPY_CLONE(uint32_t)
	TRIVIAL_MEMCPY_CLONE(uint64_t)
	TRIVIAL_MEMCPY_CLONE(float)
	TRIVIAL_MEMCPY_CLONE(double)

	#undef TRIVIAL_MEMCPY_CLONE

	template<typename T>
	inline static Block
	block_from(const Buf<T>& b)
	{
		return Block{ b.ptr, b.count * sizeof(T) };
	}
}