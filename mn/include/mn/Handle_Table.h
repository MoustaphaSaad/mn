#pragma once

#include <mn/Buf.h>

namespace mn
{
	struct Handle_Table_Index
	{
		// higher 32 bit
		uint32_t generation;
		// lower 32 bit
		uint32_t index;
	};

	inline static Handle_Table_Index
	handle_table_index_from_uint64(uint64_t v)
	{
		Handle_Table_Index self{};
		self.index = v & 0x00000000FFFFFFFF;
		self.generation = (v >> 32) & 0x00000000FFFFFFFF;
		return self;
	}

	inline static uint64_t
	handle_table_index_to_uint64(Handle_Table_Index self)
	{
		uint64_t v = 0;
		v = self.index;
		v |= ((uint64_t(self.generation) << 32) & 0xFFFFFFFF00000000);
		return v;
	}

	struct Handle_Table_Entry
	{
		uint32_t generation;
		uint32_t items_index;
	};

	template<typename T>
	struct Handle_Table_Item
	{
		T item;
		uint32_t map_index;
	};

	template<typename T>
	struct Handle_Table
	{
		mn::Buf<Handle_Table_Item<T>> items;
		mn::Buf<Handle_Table_Entry> _map;
		// used to index the index free list in the map
		uint32_t _free_list_head;
	};
	constexpr inline uint64_t HANDLE_TABLE_INVLAID_INDEX = UINT64_MAX;

	template<typename T>
	inline static Handle_Table<T>
	handle_table_new()
	{
		Handle_Table<T> self{};
		self.items = mn::buf_new<Handle_Table_Item<T>>();
		self._map = mn::buf_new<Handle_Table_Entry>();
		self._free_list_head = UINT32_MAX;
		return self;
	}

	template<typename T>
	inline static void
	handle_table_free(Handle_Table<T>& self)
	{
		mn::buf_free(self.items);
		mn::buf_free(self._map);
	}

	template<typename T>
	inline static void
	destruct(Handle_Table<T>& self)
	{
		handle_table_free(self);
	}

	template<typename T>
	inline static uint64_t
	handle_table_insert(Handle_Table<T>& self, T v)
	{
		Handle_Table_Index h{UINT32_MAX, UINT32_MAX};
		// no already free indices so create new one
		if (self._free_list_head == UINT32_MAX)
		{
			h.index = uint32_t(self._map.count);
			h.generation = 0;

			Handle_Table_Entry entry;
			entry.items_index = uint32_t(self.items.count);
			entry.generation = 0;
			mn::buf_push(self._map, entry);
		}
		else
		{
			// load the item from the map
			auto& entry = self._map[self._free_list_head];
			
			// fill out the result index
			h.index = self._free_list_head;
			h.generation = entry.generation;

			// pop the free list head
			self._free_list_head = entry.items_index;

			//update the items_index
			entry.items_index = uint32_t(self.items.count);
		}
		mn::buf_push(self.items, Handle_Table_Item<T>{v, h.index});
		return handle_table_index_to_uint64(h);
	}

	template<typename T>
	inline static void
	handle_table_remove(Handle_Table<T>& self, uint64_t v)
	{
		auto h = handle_table_index_from_uint64(v);
		auto& entry = self._map[h.index];
		assert(entry.generation == h.generation);
		if (entry.generation != h.generation)
			return;

		if (entry.items_index + 1 != self.items.count)
		{
			// load the item to be removed
			auto& item = self.items[entry.items_index];
			// replace it with the last item
			item = self.items[self.items.count - 1];
			// update the last item index in the map
			self._map[item.map_index].items_index = entry.items_index;
		}

		// increment the generation of the removed item
		++self._map[h.index].generation;
		// push the free list head to the removed item
		self._map[h.index].items_index = self._free_list_head;
		// replace the free list head with the item index
		self._free_list_head = h.index;

		mn::buf_pop(self.items);
	}

	template<typename T>
	inline static T
	handle_table_get(Handle_Table<T>& self, uint64_t v)
	{
		auto h = handle_table_index_from_uint64(v);
		return self.items[self._map[h.index].items_index].item;
	}
}