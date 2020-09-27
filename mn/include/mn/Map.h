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

	template<typename TKey, typename TValue>
	inline static void
	destruct(Key_Value<TKey, TValue>& self)
	{
		destruct(self.key);
		destruct(self.value);
	}

	template<typename TKey, typename TValue>
	inline static Key_Value<TKey, TValue>
	clone(const Key_Value<TKey, TValue>& self)
	{
		Key_Value<TKey, TValue> res{};
		res.key = clone(self.key);
		res.value = clone(self.value);
		return res;
	}


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

	TRIVIAL_HASH(bool);
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

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	struct Key_Value_Hash
	{
		inline size_t
		operator()(const Key_Value<TKey, TValue>& val) const
		{
			THash hasher;
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
	enum HASH_FLAGS: uint8_t { HASH_EMPTY, HASH_USED, HASH_DELETED };

	struct Hash_Slot
	{
		// most signficant 2 bits = HASH_FLAGS enum
		// remaining bits = index
		size_t index;
		size_t hash;
	};

	inline static HASH_FLAGS
	hash_slot_flags(Hash_Slot s)
	{
		if constexpr (sizeof(size_t) == 8)
			return HASH_FLAGS((s.index & 0xC000000000000000) >> 62);
		else if constexpr (sizeof(size_t) == 4)
			return HASH_FLAGS((s.index & 0xC0000000) >> 30);
	}

	inline static size_t
	hash_slot_index(Hash_Slot s)
	{
		if constexpr (sizeof(size_t) == 8)
			return size_t(s.index & 0x3FFFFFFFFFFFFFFF);
		else if constexpr (sizeof(size_t) == 4)
			return size_t(s.index & 0x3FFFFFFF);
	}

	inline static Hash_Slot
	hash_slot_set_flags(Hash_Slot s, HASH_FLAGS f)
	{
		if constexpr (sizeof(size_t) == 8)
		{
			s.index &= ~0xC000000000000000;
			s.index |= (size_t(f) << 62);
		}
		else if constexpr (sizeof(size_t) == 4)
		{
			s.index &= ~0xC0000000;
			s.index |= (size_t(f) << 30);
		}
		return s;
	}

	inline static Hash_Slot
	hash_slot_set_index(Hash_Slot s, size_t index)
	{
		if constexpr (sizeof(size_t) == 8)
		{
			s.index &= ~0x3FFFFFFFFFFFFFFF;
			s.index |= (size_t(index) & 0x3FFFFFFFFFFFFFFF);
		}
		else if constexpr (sizeof(size_t) == 4)
		{
			s.index &= ~0x3FFFFFFF;
			s.index |= (size_t(index) & 0x3FFFFFFF);
		}
		return s;
	}

	template<typename T, typename THash = Hash<T>>
	struct Set
	{
		Buf<Hash_Slot> _slots;
		Buf<T> values;
		size_t count;
		size_t _deleted_count;
		size_t _used_count_threshold;
		size_t _used_count_shrink_threshold;
		size_t _deleted_count_threshold;
	};

	template<typename T, typename THash = Hash<T>>
	inline static Set<T, THash>
	set_new()
	{
		Set<T, THash> self{};
		self._slots = buf_new<Hash_Slot>();
		self.values = buf_new<T>();
		return self;
	}

	template<typename T, typename THash = Hash<T>>
	inline static Set<T, THash>
	set_with_allocator(Allocator allocator)
	{
		Set<T, THash> self{};
		self._slots = buf_with_allocator<Hash_Slot>(allocator);
		self.values = buf_with_allocator<T>(allocator);
		return self;
	}

	template<typename T, typename THash = Hash<T>>
	inline static void
	set_free(Set<T, THash>& self)
	{
		buf_free(self._slots);
		buf_free(self.values);
		self.count = 0;
		self._deleted_count = 0;
	}

	template<typename T, typename THash = Hash<T>>
	inline static void
	destruct(Set<T, THash>& self)
	{
		buf_free(self._slots);
		destruct(self.values);
		self.count = 0;
		self._deleted_count = 0;
	}

	template<typename T, typename THash = Hash<T>>
	inline static void
	set_clear(Set<T, THash>& self)
	{
		buf_fill(self._slots, Hash_Slot{});
		buf_clear(self.values);
		self.count = 0;
		self._deleted_count = 0;
	}

	template<typename T, typename THash = Hash<T>>
	inline static void
	set_capacity(Set<T, THash>& self)
	{
		return self._slots.count;
	}

	struct _Hash_Search_Result
	{
		size_t hash;
		size_t index;
	};

	template<typename T, typename THash = Hash<T>>
	inline static _Hash_Search_Result
	_set_find_slot_for_insert(const Buf<Hash_Slot>& _slots, const Buf<T>& values, const T& key, size_t* external_hash)
	{
		_Hash_Search_Result res{};
		if (external_hash)
			res.hash = *external_hash;
		else
			res.hash = THash()(key);

		auto cap = _slots.count;
		if (cap == 0) return res;

		auto index = res.hash & (cap - 1);
		auto ix = index;

		size_t first_deleted_slot_index = 0;
		bool found_first_deleted_slot = false;

		// linear probing
		while(true)
		{
			auto slot = _slots[ix];
			auto slot_hash = slot.hash;
			auto slot_index = hash_slot_index(slot);
			auto slot_flags = hash_slot_flags(slot);
			switch (slot_flags)
			{
			// this position is not empty but if it's the same value then we return it
			case HASH_FLAGS::HASH_USED:
			{
				if (slot_hash == res.hash && values[slot_index] == key)
				{
					res.index = ix;
					return res;
				}
				break;
			}
			// this is an empty slot then we can use it
			case HASH_FLAGS::HASH_EMPTY:
			{
				// we didn't find the key, so check the first deleted slot if we found one then
				// reuse it
				if (found_first_deleted_slot)
				{
					res.index = first_deleted_slot_index;
				}
				// we didn't find the key, so use the first empty slot
				else
				{
					res.index = ix;
				}
				return res;
				break;
			}
			// this is a deleted slot we'll remember it just in case we wanted to resuse it
			case HASH_FLAGS::HASH_DELETED:
			{
				if (found_first_deleted_slot == false)
				{
					first_deleted_slot_index = ix;
					found_first_deleted_slot = true;
				}
				break;
			}
			default: assert(false && "unreachable"); return _Hash_Search_Result{};
			}

			// the position is not empty and the key is not the same
			++ix;
			ix &= (cap - 1);

			// if we went full circle then we just return the cap to signal no index has been found
			if (ix == index)
			{
				ix = cap;
				break;
			}
		}

		res.index = ix;
		return res;
	}

	template<typename T, typename THash = Hash<T>>
	inline static _Hash_Search_Result
	_set_find_slot_for_lookup(const Set<T, THash>& self, const T& key)
	{
		_Hash_Search_Result res{};
		res.hash = THash()(key);

		auto cap = self._slots.count;
		if (cap == 0) return res;

		auto index = res.hash & (cap - 1);
		auto ix = index;

		// linear probing
		while(true)
		{
			auto slot = self._slots[ix];
			auto slot_hash = slot.hash;
			auto slot_index = hash_slot_index(slot);
			auto slot_flags = hash_slot_flags(slot);

			// if the cell is empty then we didn't find the value
			if (slot_flags == HASH_FLAGS::HASH_EMPTY)
			{
				ix = cap;
				break;
			}

			// if the cell is used and it's the same value then we found it
			if (slot_flags == HASH_FLAGS::HASH_USED &&
				slot_hash == res.hash &&
				self.values[slot_index] == key)
			{
				break;
			}

			// the position is not used or the key is not the same
			++ix;
			ix &= (cap - 1);

			// if we went full circle then we just return the cap to signal no index has been found
			if (ix == index)
			{
				ix = cap;
				break;
			}
		}

		res.index = ix;
		return res;
	}

	template<typename T, typename THash = Hash<T>>
	inline static void
	_set_reserve_exact(Set<T, THash>& self, size_t new_count)
	{
		auto new_slots = buf_with_allocator<Hash_Slot>(self._slots.allocator);
		buf_resize_fill(new_slots, new_count, Hash_Slot{});

		self._deleted_count = 0;
		// if 12/16th of table is occupied, grow
		self._used_count_threshold = new_count - (new_count >> 2);
		// if deleted count is 3/16th of table, rebuild
		self._deleted_count_threshold = (new_count >> 3) + (new_count >> 4);
		// if table is only 4/16th full, shrink
		self._used_count_shrink_threshold = new_count >> 2;

		// do a rehash
		if (self.count != 0)
		{
			for (size_t i = 0; i < self._slots.count; ++i)
			{
				auto slot = self._slots[i];
				auto flags = hash_slot_flags(slot);
				if (flags == HASH_FLAGS::HASH_USED)
				{
					auto index = hash_slot_index(slot);
					auto res = _set_find_slot_for_insert<T, THash>(new_slots, self.values, self.values[index], &slot.hash);
					new_slots[res.index] = slot;
				}
			}
		}

		buf_free(self._slots);
		self._slots = new_slots;
	}

	template<typename T, typename THash = Hash<T>>
	inline static void
	_set_maintain_space_complexity(Set<T, THash>& self)
	{
		if (self._slots.count == 0)
		{
			_set_reserve_exact(self, 8);
		}
		else if (self.count + 1 > self._used_count_threshold)
		{
			_set_reserve_exact(self, self._slots.count * 2);
		}
	}

	template<typename T, typename THash = Hash<T>>
	inline static void
	set_reserve(Set<T, THash>& self, size_t added_count)
	{
		if (added_count == 0)
			return;

		auto new_cap = self.count + added_count;
		new_cap *= 4;
		new_cap = new_cap / 3 + 1;
		if (new_cap > self._used_count_threshold)
		{
			_set_reserve_exact(self, new_cap);
		}
	}

	template<typename T, typename THash = Hash<T>>
	inline static T*
	set_insert(Set<T, THash>& self, const T& key)
	{
		_set_maintain_space_complexity(self);

		auto res = _set_find_slot_for_insert<T, THash>(self._slots, self.values, key, nullptr);

		auto& slot = self._slots[res.index];
		auto flags = hash_slot_flags(slot);
		switch(flags)
		{
		case HASH_FLAGS::HASH_EMPTY:
		{
			slot = hash_slot_set_flags(slot, HASH_FLAGS::HASH_USED);
			slot = hash_slot_set_index(slot, self.count);
			slot.hash = res.hash;
			++self.count;
			return buf_push(self.values, key);
		}
		case HASH_FLAGS::HASH_DELETED:
		{
			slot = hash_slot_set_flags(slot, HASH_FLAGS::HASH_USED);
			slot = hash_slot_set_index(slot, self.count);
			slot.hash = res.hash;
			++self.count;
			--self._deleted_count;
			return buf_push(self.values, key);
		}
		case HASH_FLAGS::HASH_USED:
		{
			auto index = hash_slot_index(slot);
			return &self.values[index];
		}
		default:
		{
			assert(false && "unreachable");
			return nullptr;
		}
		}
	}

	template<typename T, typename THash = Hash<T>>
	inline static const T*
	set_lookup(const Set<T, THash>& self, const T& key)
	{
		auto res = _set_find_slot_for_lookup(self, key);
		if (res.index == self._slots.count)
			return nullptr;
		auto& slot = self._slots[res.index];
		auto index = hash_slot_index(slot);
		return (const T*)(self.values.ptr + index);
	}

	template<typename T, typename THash = Hash<T>>
	inline static bool
	set_remove(Set<T, THash>& self, const T& key)
	{
		auto res = _set_find_slot_for_lookup(self, key);
		if (res.index == self._slots.count)
			return false;
		auto& slot = self._slots[res.index];
		auto index = hash_slot_index(slot);
		slot = hash_slot_set_flags(slot, HASH_FLAGS::HASH_DELETED);

		if (index == self.count - 1)
		{
			buf_remove(self.values, index);
		}
		else
		{
			// fixup the index of the last element after swap
			auto last_res = _set_find_slot_for_lookup(self, self.values[self.count - 1]);
			self._slots[last_res.index] = hash_slot_set_index(self._slots[last_res.index], index);
			buf_remove(self.values, index);
		}
		
		--self.count;
		++self._deleted_count;

		// rehash because of size is too low
		if (self.count < self._used_count_shrink_threshold && self._slots.count > 8)
		{
			_set_reserve_exact(self, self._slots.count >> 1);
			buf_shrink_to_fit(self.values);
		}
		// rehash because of too many deleted values
		else if (self._deleted_count > self._deleted_count_threshold)
		{
			_set_reserve_exact(self, self._slots.count);
		}
		return true;
	}

	template<typename T, typename THash = Hash<T>>
	inline static Set<T, THash>
	set_clone(const Set<T, THash>& other, Allocator allocator = allocator_top())
	{
		Set<T, THash> self = other;
		self._slots = buf_memcpy_clone(other._slots, allocator);
		self.values = buf_clone(other.values, allocator);
		return self;
	}

	template<typename T, typename THash = Hash<T>>
	inline static Set<T, THash>
	set_memcpy_clone(const Set<T, THash>& other, Allocator allocator = allocator_top())
	{
		Set<T, THash> self = other;
		self._slots = buf_memcpy_clone(other._slots, allocator);
		self.values = buf_memcpy_clone(other.values, allocator);
		return self;
	}

	template<typename T, typename THash = Hash<T>>
	inline static Set<T, THash>
	clone(const Set<T, THash>& other)
	{
		return set_clone(other);
	}

	template<typename T, typename THash = Hash<T>>
	inline static const T*
	set_begin(const Set<T, THash>& self)
	{
		return buf_begin(self.values);
	}

	template<typename T, typename THash = Hash<T>>
	inline static const T*
	set_end(const Set<T, THash>& self)
	{
		return buf_end(self.values);
	}

	template<typename T, typename THash = Hash<T>>
	inline static const T*
	begin(const Set<T, THash>& self)
	{
		return buf_begin(self.values);
	}

	template<typename T, typename THash = Hash<T>>
	inline static const T*
	end(const Set<T, THash>& self)
	{
		return buf_end(self.values);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	using Map = Set<Key_Value<TKey, TValue>, Key_Value_Hash<TKey, TValue, THash>>;

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Map<TKey, TValue, THash>
	map_new()
	{
		return set_new<Key_Value<TKey, TValue>, Key_Value_Hash<TKey, TValue, THash>>();
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Map<TKey, TValue, THash>
	map_with_allocator(Allocator allocator)
	{
		return set_with_allocator<Key_Value<TKey, TValue>, Key_Value_Hash<TKey, TValue, THash>>(allocator);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static void
	map_free(Map<TKey, TValue, THash>& self)
	{
		set_free(self);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static void
	map_clear(Map<TKey, TValue, THash>& self)
	{
		set_clear(self);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static size_t
	map_capacity(Map<TKey, TValue, THash>& self)
	{
		return set_capacity(self);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	map_insert(Map<TKey, TValue, THash>& self, const TKey& key)
	{
		return (Key_Value<const TKey, TValue>*)set_insert(self, Key_Value<TKey, TValue>{key});
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	map_insert(Map<TKey, TValue, THash>& self, const TKey& key, const TValue& value)
	{
		return (Key_Value<const TKey, TValue>*)set_insert(self, Key_Value<TKey, TValue>{key, value});
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static const Key_Value<const TKey, TValue>*
	map_lookup(const Map<TKey, TValue, THash>& self, const TKey& key)
	{
		return (const Key_Value<const TKey, TValue>*)set_lookup(self, Key_Value<TKey, TValue>{key});
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	map_lookup(Map<TKey, TValue, THash>& self, const TKey& key)
	{
		return (Key_Value<const TKey, TValue>*)set_lookup(self, Key_Value<TKey, TValue>{key});
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static bool
	map_remove(Map<TKey, TValue, THash>& self, const TKey& key)
	{
		return set_remove(self, Key_Value<TKey, TValue>{key});
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static void
	map_reserve(Map<TKey, TValue, THash>& self, size_t added_count)
	{
		set_reserve(self, added_count);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Map<TKey, TValue, THash>
	map_clone(const Map<TKey, TValue, THash>& other, Allocator allocator = allocator_top())
	{
		return set_clone(other, allocator);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Map<TKey, TValue, THash>
	map_memcpy_clone(const Map<TKey, TValue, THash>& other, Allocator allocator = allocator_top())
	{
		return set_memcpy_clone(other, allocator);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static const Key_Value<const TKey, TValue>*
	map_begin(const Map<TKey, TValue, THash>& self)
	{
		return (const Key_Value<const TKey, TValue>*)set_begin(self);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	map_begin(Map<TKey, TValue, THash>& self)
	{
		return (Key_Value<const TKey, TValue>*)set_begin(self);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static const Key_Value<const TKey, TValue>*
	map_end(const Map<TKey, TValue, THash>& self)
	{
		return (Key_Value<const TKey, TValue>*)set_end(self);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	map_end(Map<TKey, TValue, THash>& self)
	{
		return (Key_Value<const TKey, TValue>*)set_end(self);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static const Key_Value<const TKey, TValue>*
	begin(const Map<TKey, TValue, THash>& self)
	{
		return (const Key_Value<const TKey, TValue>*)set_begin(self);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	begin(Map<TKey, TValue, THash>& self)
	{
		return (Key_Value<const TKey, TValue>*)set_begin(self);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static const Key_Value<const TKey, TValue>*
	end(const Map<TKey, TValue, THash>& self)
	{
		return (Key_Value<const TKey, TValue>*)set_end(self);
	}

	template<typename TKey, typename TValue, typename THash = Hash<TKey>>
	inline static Key_Value<const TKey, TValue>*
	end(Map<TKey, TValue, THash>& self)
	{
		return (Key_Value<const TKey, TValue>*)set_end(self);
	}
}
