#pragma once

#include "mn/Base.h"
#include "mn/Memory.h"
#include "mn/Buf.h"

#include <assert.h>

namespace mn
{
	//Key value section
	template<typename TKey, typename TValue>
	struct Key_Value
	{
		TKey key;
		TValue value;

		bool
		operator==(const Key_Value<TKey, TValue>& other) const
		{
			return key == other.key;
		}

		bool
		operator!=(const Key_Value<TKey, TValue>& other) const
		{
			return !operator==(other);
		}
	};


	//hash section

	/**
	 * @brief      Default hash functor
	 *
	 * @tparam     T     Type to be hashed
	 */
	template<typename T>
	struct Hash
	{
		inline size_t
		operator()(T) const
		{
			static_assert(sizeof(T) == 0, "there's no hash function defined for this type");
			return 0;
		}
	};

	template<typename T>
	struct Hash<T*>
	{
		inline size_t
		operator()(T* ptr) const
		{
			return size_t(ptr);
		}
	};

	#define TRIVIAL_HASH(TYPE)\
	template<>\
	struct Hash<TYPE>\
	{\
		inline size_t\
		operator()(TYPE value) const\
		{\
			return static_cast<size_t>(value);\
		}\
	}

	TRIVIAL_HASH(char);
	TRIVIAL_HASH(int8_t);
	TRIVIAL_HASH(int16_t);
	TRIVIAL_HASH(int32_t);
	TRIVIAL_HASH(int64_t);
	TRIVIAL_HASH(uint8_t);
	TRIVIAL_HASH(uint16_t);
	TRIVIAL_HASH(uint32_t);
	TRIVIAL_HASH(uint64_t);

	#undef TRIVIAL_HASH

	//MurmurHashUnaligned2
	inline static size_t
	_murmur_hash_unaligned2(const void* ptr, size_t len, size_t seed)
	{
		if constexpr (sizeof(void*) == 4)
		{
			const size_t m = 0x5bd1e995;
			size_t hash = seed ^ len;
			const unsigned char* buffer = static_cast<const unsigned char*>(ptr);

			while (len >= 4)
			{
				size_t k = *reinterpret_cast<const size_t*>(buffer);
				k *= m;
				k ^= k >> 24;
				k *= m;
				hash *= m;
				hash ^= k;
				buffer += 4;
				len -= 4;
			}

			if (len == 3)
			{
				hash ^= static_cast<unsigned char>(buffer[2]) << 16;
				--len;
			}

			if (len == 2)
			{
				hash ^= static_cast<unsigned char>(buffer[1]) << 8;
				--len;
			}

			if (len == 1)
			{
				hash ^= static_cast<unsigned char>(buffer[0]);
				hash *= m;
				--len;
			}

			hash ^= hash >> 13;
			hash *= m;
			hash ^= hash >> 15;
			return hash;
		}
		else if constexpr (sizeof(void*) == 8)
		{
			auto load_bytes = [](const unsigned char* p, intptr_t n) -> size_t
			{
				size_t result = 0;
				--n;
				do
					result = (result << 8) + static_cast<unsigned char>(p[n]);
				while (--n >= 0);
				return result;
			};

			auto shift_mix = [](size_t v) -> size_t
			{
				return v ^ (v >> 47);
			};

			static const size_t mul = (static_cast<size_t>(0xc6a4a793UL) << 32UL) + static_cast<size_t>(0x5bd1e995UL);

			const unsigned char* const buffer = static_cast<const unsigned char*>(ptr);

			const int32_t len_aligned = len & ~0x7;
			const unsigned char* const end = buffer + len_aligned;

			size_t hash = seed ^ (len * mul);
			for (const unsigned char* p = buffer;
				p != end;
				p += 8)
			{
				const size_t data = shift_mix((*reinterpret_cast<const size_t*>(p)) * mul) * mul;
				hash ^= data;
				hash *= mul;
			}

			if ((len & 0x7) != 0)
			{
				const size_t data = load_bytes(end, len & 0x7);
				hash ^= data;
				hash *= mul;
			}

			hash = shift_mix(hash) * mul;
			hash = shift_mix(hash);
			return hash;
		}
	}

	/**
	 * @brief      Given a block of memory does a murmur hash on it
	 *
	 * @param[in]  ptr   The pointer to memory
	 * @param[in]  len   The length(in bytes) of the memory
	 * @param[in]  seed  The seed (optional)
	 *
	 * @return     The hash value of the given memory
	 */
	inline static size_t
	murmur_hash(const void* ptr, size_t len, size_t seed = 0xc70f6907UL)
	{
		return _murmur_hash_unaligned2(ptr, len, seed);
	}

	/**
	 * @brief      Given a block of memory does a murmur hash on it
	 *
	 * @param[in]  block  The block to hash
	 * @param[in]  seed   The seed (optional)
	 *
	 * @return     The hash value of the given memory
	 */
	inline static size_t
	murmur_hash(const Block& block, size_t seed = 0xc70f6907UL)
	{
		return murmur_hash(block.ptr, block.size, seed);
	}

	template<>
	struct Hash<float>
	{
		inline size_t
		operator()(float value) const
		{
			return value != 0.0f ? murmur_hash(&value, sizeof(float)) : 0;
		}
	};

	template<>
	struct Hash<double>
	{
		inline size_t
		operator()(double value) const
		{
			return value != 0.0f ? murmur_hash(&value, sizeof(double)) : 0;
		}
	};

	template<typename TKey, typename TValue>
	struct Hash<Key_Value<TKey, TValue>>
	{
		inline size_t
		operator()(const Key_Value<TKey, TValue>& val) const
		{
			Hash<TKey> hasher;
			return hasher(val.key);
		}
	};

	/**
	 * @brief      Mixes two hash values together
	 *
	 * @param[in]  a     first hash value
	 * @param[in]  b     second hash value
	 */
	inline static size_t
	hash_mix(size_t a, size_t b)
	{
		if constexpr (sizeof(size_t) == 4)
		{
			return (b + 0x9e3779b9 + (a << 6) + (a >> 2));
		}
		else if constexpr (sizeof(size_t) == 8)
		{
			a ^= b;
			a *= 0xff51afd7ed558ccd;
			a ^= a >> 32;
			return a;
		}
	}


	//Map section

	enum HASH_FLAGS { HASH_EMPTY, HASH_USED, HASH_DELETED };

	/**
	 * @brief      Hash Map Structure
	 *
	 * @tparam     TKey    Key Type
	 * @tparam     TValue  Value Type
	 * @tparam     THash   Hasher Type
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	struct Map
	{
		Buf<HASH_FLAGS>					flags;
		Buf<Key_Value<TKey, TValue>> 	values;
		THash							hasher;
		size_t							count;
	};

	/**
	 * @brief      Creates a new hash map
	 *
	 * @tparam     TKey    Key Type
	 * @tparam     TValue  Value Type
	 * @tparam     THash   Hasher Type
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Map<TKey, TValue, THash>
	map_new()
	{
		Map<TKey, TValue, THash> result{};
		result.flags = buf_new<HASH_FLAGS>();
		result.values = buf_new<Key_Value<TKey, TValue>>();
		return result;
	}

	/**
	 * @brief      Creates a new hash map with the given allocator
	 *
	 * @param[in]  allocator  The allocator to be used by the map
	 *
	 * @tparam     TKey       Key Type
	 * @tparam     TValue     Value Type
	 * @tparam     THash      Hasher Type
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Map<TKey, TValue, THash>
	map_with_allocator(Allocator allocator)
	{
		Map<TKey, TValue, THash> result{};
		result.flags = buf_with_allocator<HASH_FLAGS>(allocator);
		result.values = buf_with_allocator<Key_Value<TKey, TValue>>(allocator);
		return result;
	}

	/**
	 * @brief      Frees the hash map
	 *
	 * @param      self    The Hash map
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static void
	map_free(Map<TKey, TValue, THash>& self)
	{
		buf_free(self.flags);
		buf_free(self.values);
		self.count = 0;
	}

	/**
	 * @brief      Destruct function overload for the key value pair
	 *
	 * @param      self    The key value pair
	 */
	template<typename TKey, typename TValue>
	inline static void
	destruct(Key_Value<TKey, TValue>& self)
	{
		destruct(self.key);
		destruct(self.value);
	}

	/**
	 * @brief      Destruct function overload for the hash map
	 * which frees the hash map and the internal elements as well
	 *
	 * @param      self    The hash map
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static void
	destruct(Map<TKey, TValue, THash>& self)
	{
		for(auto it = map_begin_mut(self);
			it != map_end_mut(self);
			it = map_next_mut(self, it))
		{
			destruct(*it);
		}
		map_free(self);
	}

	/**
	 * @brief      Clears the given map
	 *
	 * @param      self    The hash map
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static void
	map_clear(Map<TKey, TValue, THash>& self)
	{
		buf_fill(self.flags, HASH_FLAGS::HASH_EMPTY);
		self.count = 0;
	}

	/**
	 * @brief      Returns the capacity of the hash map
	 *
	 * @param      self    The hash map
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static size_t
	map_capacity(Map<TKey, TValue, THash>& self)
	{
		return self.flags.count;
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static size_t
	map__find_insertion_index(const THash& hasher,
		const Buf<HASH_FLAGS>& flags, const Buf<Key_Value<TKey, TValue>>& values,
		const TKey& key)
	{
		size_t cap = flags.count;
		if(cap == 0) return cap;

		size_t hash_value = hasher(key);
		size_t index = hash_value % cap;
		size_t ix = index;

		//linear probing
		while(true)
		{
			//this is an empty spot or deleted spot then we can use it
			if (flags[ix] == HASH_FLAGS::HASH_EMPTY ||
				flags[ix] == HASH_FLAGS::HASH_DELETED)
				return ix;

			//this position is not empty but if it's the same value then we return it
			if(values[ix].key == key)
				return ix;

			//the position is not empty and the key is not the same
			++ix;
			ix %= cap;

			//if we went a full circle then we just return the cap to signal no index has beend found
			if(ix == index)
				return cap;
		}

		return cap;
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static size_t
	map__find_insertion_index(const Map<TKey, TValue, THash>& self, const TKey& key)
	{
		return map__find_insertion_index(self.hasher, self.flags, self.values, key);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static size_t
	map__lookup_index(const Map<TKey, TValue, THash>& self, const TKey& key)
	{
		size_t cap = self.flags.count;
		if(cap == 0) return cap;

		size_t hash_value = self.hasher(key);
		size_t index = hash_value % cap;
		size_t ix = index;

		//linear probing goes here
		while(true)
		{
			//if the cell is empty then we didn't find the value
			if(self.flags[ix] == HASH_FLAGS::HASH_EMPTY)
				return cap;

			//if the cell is used and it's the same value then we found it
			if (self.flags[ix] == HASH_FLAGS::HASH_USED &&
				self.values[ix].key == key)
				return ix;

			//increment the index
			++ix;
			ix %= cap;

			//check if we went full circle
			if(ix == index)
				return cap;
		}

		return cap;
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static void
	map__maintain_space_complexity(Map<TKey, TValue, THash>& self)
	{
		if(self.flags.count == 0)
			map_reserve(self, 8);
		else if(self.count > (self.flags.count / 2))
			map_reserve(self, self.flags.count);
	}

	/**
	 * @brief      Inserts the given key into the map (doesn't initialize the value to anything)
	 *
	 * @param      self    The hash map
	 * @param[in]  key     The key
	 * 
	 * @return     A Pointer to the inserted element
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	map_insert(Map<TKey, TValue, THash>& self, const TKey& key)
	{
		map__maintain_space_complexity(self);

		auto index = map__find_insertion_index(self, key);
		self.count += self.flags[index] != HASH_FLAGS::HASH_USED;
		self.flags[index] = HASH_FLAGS::HASH_USED;
		self.values[index].key = key;
		return (Key_Value<const TKey, TValue>*)(self.values.ptr + index);
	}

	/**
	 * @brief      Inserts the given key value pair into the map
	 *
	 * @param      self    The hash map
	 * @param[in]  key     The key
	 * @param[in]  value   The value
	 * 
	 * @return     A Pointer to the inserted element
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	map_insert(Map<TKey, TValue, THash>& self, const TKey& key, const TValue& value)
	{
		map__maintain_space_complexity(self);

		auto index = map__find_insertion_index(self, key);
		self.count += self.flags[index] != HASH_FLAGS::HASH_USED;
		self.flags[index] = HASH_FLAGS::HASH_USED;
		self.values[index] = Key_Value<TKey, TValue> { key, value };
		return (Key_Value<const TKey, TValue>*)(self.values.ptr + index);
	}

	/**
	 * @brief      Searches for the given key in the hash map
	 *
	 * @param[in]  self    The hash map
	 * @param[in]  key     The key
	 *
	 * @return     A Pointer to the found element or nullptr in the case it doesn't exist
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static const Key_Value<const TKey, TValue>*
	map_lookup(const Map<TKey, TValue, THash>& self, const TKey& key)
	{
		auto index = map__lookup_index(self, key);
		if(index == self.flags.count)
			return nullptr;
		return (Key_Value<const TKey, TValue>*)(self.values.ptr + index);
	}

	/**
	 * @brief      Searches for the given key in the hash map
	 *
	 * @param[in]  self    The hash map
	 * @param[in]  key     The key
	 *
	 * @return     A Pointer to the found element or nullptr in the case it doesn't exist
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	map_lookup(Map<TKey, TValue, THash>& self, const TKey& key)
	{
		auto index = map__lookup_index(self, key);
		if(index == self.flags.count)
			return nullptr;
		return (Key_Value<const TKey, TValue>*)(self.values.ptr + index);
	}

	/**
	 * @brief      Removes the given key from the hash map
	 *
	 * @param      self    The hash map
	 * @param[in]  key     The key
	 *
	 * @return     True in case the removal was successful, false otherwise
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static bool
	map_remove(Map<TKey, TValue, THash>& self, const TKey& key)
	{
		auto index = map__lookup_index(self, key);
		if(index == self.flags.count)
			return false;
		self.flags[index] = HASH_FLAGS::HASH_DELETED;
		--self.count;
		return true;
	}

	/**
	 * @brief      Removes the given iterator from the hash map
	 *
	 * @param      self    The hash map
	 * @param[in]  it      The iterator
	 *
	 * @return     True in case the removal was successful, false otherwise
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static bool
	map_remove(Map<TKey, TValue, THash>& self, const Key_Value<const TKey, TValue>* it)
	{
		auto diff = it - (Key_Value<const TKey, TValue>*)self.values.ptr;
		assert(diff >= 0);
		size_t index = diff;
		assert(index < self.flags.count);
		if(self.flags[index] == HASH_FLAGS::HASH_USED)
		{
			self.flags[index] = HASH_FLAGS::HASH_DELETED;
			--self.count;
			return true;
		}
		return false;
	}

	/**
	 * @brief      Ensures that the map has the capacity for the expected count
	 *
	 * @param      self            The hash map
	 * @param[in]  expected_count  The expected count
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static void
	map_reserve(Map<TKey, TValue, THash>& self, size_t expected_count)
	{
		if(self.flags.count - self.count >= expected_count)
			return;

		size_t double_cap = (self.flags.count * 2);
		size_t fit = self.count + expected_count;
		size_t new_cap = double_cap > fit ? double_cap : fit;

		auto new_flags = buf_with_allocator<HASH_FLAGS>(self.flags.allocator);
		buf_resize(new_flags, new_cap);
		auto new_values = buf_with_allocator<Key_Value<TKey, TValue>>(self.values.allocator);
		buf_resize(new_values, new_cap);

		::memset(new_flags.ptr, 0, new_flags.count * sizeof(HASH_FLAGS));

		if (self.count != 0)
		{
			for (size_t i = 0; i < self.flags.count; ++i)
			{
				if (self.flags[i] == HASH_FLAGS::HASH_USED)
				{
					size_t new_index = map__find_insertion_index(self.hasher, new_flags, new_values, self.values[i].key);
					new_flags[new_index] = HASH_FLAGS::HASH_USED;
					new_values[new_index] = self.values[i];
				}
			}
		}

		buf_free(self.flags);
		buf_free(self.values);

		self.flags = new_flags;
		self.values = new_values;
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Map<TKey, TValue, THash>
	map_clone(const Map<TKey, TValue, THash>& other, Allocator allocator = allocator_top())
	{
		Map<TKey, TValue, THash> self{};
		self.flags = buf_clone(other.flags, allocator);
		self.values = buf_clone(other.values, allocator);
		self.hasher = other.hasher;
		self.count = other.count;
		return self;
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Map<TKey, TValue, THash>
	clone(const Map<TKey, TValue, THash>& other)
	{
		return clone(other);
	}

	/**
	 * @brief      Returns a iterator/pointer to the first element in the hash map
	 *
	 * @param[in]  self    The hash map
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static const Key_Value<const TKey, TValue>*
	map_begin(const Map<TKey, TValue, THash>& self)
	{
		for(size_t i = 0; i < self.flags.count; ++i)
			if(self.flags[i] == HASH_FLAGS::HASH_USED)
				return (Key_Value<const TKey, TValue>*)&self.values[i];
		return (Key_Value<const TKey, TValue>*)buf_end(self.values);
	}

	/**
	 * @brief      Returns a iterator/pointer to the first element in the hash map
	 *
	 * @param[in]  self    The hash map
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	map_begin(Map<TKey, TValue, THash>& self)
	{
		for(size_t i = 0; i < self.flags.count; ++i)
			if(self.flags[i] == HASH_FLAGS::HASH_USED)
				return (Key_Value<const TKey, TValue>*)&self.values[i];
		return (Key_Value<const TKey, TValue>*)buf_end(self.values);
	}

	/**
	 * @brief      Returns a iterator/pointer to the first element in the hash map
	 * where the key part is not const qualified so that it can be edited (not advised to use
	 * this unless you know what you're doing)
	 *
	 * @param[in]  self    The hash map
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<TKey, TValue>*
	map_begin_mut(Map<TKey, TValue, THash>& self)
	{
		for(size_t i = 0; i < self.flags.count; ++i)
			if(self.flags[i] == HASH_FLAGS::HASH_USED)
				return &self.values[i];
		return buf_end(self.values);
	}

	/**
	 * @brief      Advances the given iterator/pointer to the next element in the hash map
	 *
	 * @param[in]  self    The hash map
	 * @param[in]  val     The iterator
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static const Key_Value<const TKey, TValue>*
	map_next(const Map<TKey, TValue, THash>& self, const Key_Value<const TKey, TValue>* val)
	{
		auto diff = (Key_Value<TKey, TValue>*)val - self.values.ptr;
		assert(diff >= 0);
		size_t index = diff;
		assert(index < self.flags.count);
		++index;
		for(; index < self.flags.count; ++index)
		{
			if(self.flags[index] == HASH_FLAGS::HASH_USED)
				return (Key_Value<const TKey, TValue>*)&self.values[index];
		}
		return map_end(self);
	}

	/**
	 * @brief      Advances the given iterator/pointer to the next element in the hash map
	 *
	 * @param[in]  self    The hash map
	 * @param[in]  val     The iterator
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	map_next(Map<TKey, TValue, THash>& self, Key_Value<const TKey, TValue>* val)
	{
		auto diff = (Key_Value<TKey, TValue>*)val - self.values.ptr;
		assert(diff >= 0);
		size_t index = diff;
		assert(index < self.flags.count);
		++index;
		for(; index < self.flags.count; ++index)
		{
			if(self.flags[index] == HASH_FLAGS::HASH_USED)
				return (Key_Value<const TKey, TValue>*)&self.values[index];
		}
		return map_end(self);
	}

	/**
	 * @brief      Advances the given iterator/pointer to the next element in the hash map
	 * where the key part is not const qualified so that it can be edited (not advised to use
	 * this unless you know what you're doing)
	 * 
	 * @param[in]  self    The hash map
	 * @param[in]  val     The iterator
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<TKey, TValue>*
	map_next_mut(Map<TKey, TValue, THash>& self, Key_Value<TKey, TValue>* val)
	{
		auto diff = (Key_Value<TKey, TValue>*)val - self.values.ptr;
		assert(diff >= 0);
		size_t index = diff;
		assert(index < self.flags.count);
		++index;
		for(; index < self.flags.count; ++index)
		{
			if(self.flags[index] == HASH_FLAGS::HASH_USED)
				return (Key_Value<TKey, TValue>*)&self.values[index];
		}
		return map_end_mut(self);
	}

	/**
	 * @brief      Returns an iterator/pointer to the end of the hash map
	 *
	 * @param[in]  self    The hash map
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static const Key_Value<const TKey, TValue>*
	map_end(const Map<TKey, TValue, THash>& self)
	{
		return (Key_Value<const TKey, TValue>*)buf_end(self.values);
	}

	/**
	 * @brief      Returns an iterator/pointer to the end of the hash map
	 *
	 * @param[in]  self    The hash map
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	map_end(Map<TKey, TValue, THash>& self)
	{
		return (Key_Value<const TKey, TValue>*)buf_end(self.values);
	}

	/**
	 * @brief      Returns an iterator/pointer to the end of the hash map
	 * where the key part is not const qualified so that it can be edited (not advised to use
	 * this unless you know what you're doing)
	 *
	 * @param[in]  self    The hash map
	 */
	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<TKey, TValue>*
	map_end_mut(Map<TKey, TValue, THash>& self)
	{
		return (Key_Value<TKey, TValue>*)buf_end(self.values);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	struct Map_Iterator
	{
		Key_Value<const TKey, TValue>* it;
		Map<TKey, TValue, THash>* map;

		Map_Iterator&
		operator++()
		{
			it = map_next(*map, it);
			return *this;
		}

		Map_Iterator
		operator++(int)
		{
			auto r = *this;
			it = map_next(*map, it);
			return r;
		}

		bool
		operator==(const Map_Iterator& other) const
		{
			return it == other.it && map == other.map;
		}

		bool
		operator!=(const Map_Iterator& other) const
		{
			return !operator==(other);
		}

		Key_Value<const TKey, TValue>&
		operator*()
		{
			return *it;
		}

		const Key_Value<const TKey, TValue>&
		operator*() const
		{
			return *it;
		}

		Key_Value<const TKey, TValue>*
		operator->()
		{
			return it;
		}

		const Key_Value<const TKey, TValue>*
		operator->() const
		{
			return it;
		}
	};

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	struct Const_Map_Iterator
	{
		const Key_Value<const TKey, TValue>* it;
		const Map<TKey, TValue, THash>* map;

		Const_Map_Iterator&
		operator++()
		{
			it = map_next(*map, it);
			return *this;
		}

		Const_Map_Iterator
		operator++(int)
		{
			auto r = *this;
			it = map_next(*map, it);
			return r;
		}

		bool
		operator==(const Const_Map_Iterator& other) const
		{
			return it == other.it && map == other.map;
		}

		bool
		operator!=(const Const_Map_Iterator& other) const
		{
			return !operator==(other);
		}

		const Key_Value<const TKey, TValue>&
		operator*() const
		{
			return *it;
		}

		const Key_Value<const TKey, TValue>*
		operator->() const
		{
			return it;
		}
	};

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Map_Iterator<TKey, TValue, THash>
	begin(Map<TKey, TValue, THash>& self)
	{
		Map_Iterator<TKey, TValue, THash> r{};
		r.it = map_begin(self);
		r.map = &self;
		return r;
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Const_Map_Iterator<TKey, TValue, THash>
	begin(const Map<TKey, TValue, THash>& self)
	{
		Const_Map_Iterator<TKey, TValue, THash> r{};
		r.it = map_begin(self);
		r.map = &self;
		return r;
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Map_Iterator<TKey, TValue, THash>
	end(Map<TKey, TValue, THash>& self)
	{
		Map_Iterator<TKey, TValue, THash> r{};
		r.it = map_end(self);
		r.map = &self;
		return r;
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Const_Map_Iterator<TKey, TValue, THash>
	end(const Map<TKey, TValue, THash>& self)
	{
		Const_Map_Iterator<TKey, TValue, THash> r{};
		r.it = map_end(self);
		r.map = &self;
		return r;
	}
}
