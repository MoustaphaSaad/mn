#pragma once

#include "mn/Exports.h"
#include "mn/memory/Interface.h"
#include "mn/memory/Virtual.h"

namespace mn::memory
{
	struct Buddy : Interface
	{
		struct Node
		{
			Node* prev;
			Node* next;
		};

		Interface* meta;
		Block memory;

		// maximum allocation size is set to (2**max_alloc_log2)
		size_t max_alloc_log2;
		size_t max_alloc;

		// Allocations are done in powers of two starting from MIN_ALLOC and ending at
		// MAX_ALLOC inclusive. Each allocation size has a bucket that stores the free
		// list for that allocation size.
		//
		// Given a bucket index, the size of the allocations in that bucket can be
		// found with "(size_t)1 << (MAX_ALLOC_LOG2 - bucket)"
		size_t bucket_max;

		// Each bucket corresponds to a certain allocation size and stores a free list
		// for that size. The bucket at index 0 corresponds to an allocation size of
		// MAX_ALLOC (i.e. the whole address space)
		Node*  buckets;
		// Free lists are stored as circular doubly-linked lists. Every possible
		// allocation size has an associated free list that is threaded through all
		// currently free blocks of that size.
		//
		// We could initialize the allocator by giving it one free block the size of
		// the entire address space. However, this would cause us to instantly reserve
		// half of the entire address space on the first allocation, since the first
		// split would store a free list entry at the start of the right child of the
		// root. Instead, we have the tree start out small and grow the size of the
		// tree as we use more memory. The size of the tree is tracked by this value
		size_t bucket_count;

		// This array represents a linearized binary tree of bits. Every possible
		// allocation larger than MIN_ALLOC has a node in this tree (and therefore a
		// bit in this array).
		//
		// Given the index for a node, lineraized binary trees allow you to traverse to
		// the parent node or the child nodes just by doing simple arithmetic on the
		// index:
		// - Move to parent:         index = (index - 1) / 2;
		// - Move to left child:     index = index * 2 + 1;
		// - Move to right child:    index = index * 2 + 2;
		// - Move to sibling:        index = ((index - 1) ^ 1) + 1;
		//
		// Each node in this tree can be in one of several states:
		//
		// - UNUSED (both children are UNUSED)
		// - SPLIT (one child is UNUSED and the other child isn't)
		// - USED (neither children are UNUSED)
		//
		// These states take two bits to store. However, it turns out we have enough
		// information to distinguish between UNUSED and USED from context, so we only
		// need to store SPLIT or not, which only takes a single bit.
		//
		// Note that we don't need to store any nodes for allocations of size MIN_ALLOC
		// since we only ever care about parent nodes.
		uint8_t* node_is_split;

		// This is the starting address of the address range for this allocator. Every
		// returned allocation will be an offset of this pointer from 0 to MAX_ALLOC.
		uint8_t* base_ptr;

		// This is the maximum address that has ever been used by the allocator. It's
		// used to know when to call "brk" to request more memory from the kernel.
		uint8_t* max_ptr;

		MN_EXPORT
		Buddy(size_t heap_size, Interface* meta = virtual_mem());

		MN_EXPORT
		~Buddy();

		MN_EXPORT Block
		alloc(size_t size, uint8_t alignment) override;

		MN_EXPORT void
		free(Block block) override;
	};
}
