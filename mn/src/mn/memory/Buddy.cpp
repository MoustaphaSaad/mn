#include "mn/memory/Buddy.h"

#include <math.h>
#include <string.h>

namespace mn::memory
{
	constexpr size_t BUDDY_HEADER_SIZE = 8;
	constexpr size_t BUDDY_MIN_ALLOC_LOG2 = 4;
	constexpr size_t BUDDY_MIN_ALLOC = 16;

	inline static void
	node_init(Buddy::Node* n)
	{
		n->prev = n;
		n->next = n;
	}

	inline static void
	node_push(Buddy::Node* list, Buddy::Node* entry)
	{
		auto prev = list->prev;
		entry->prev = prev;
		entry->next = list;
		prev->next = entry;
		list->prev = entry;
	}

	inline static void
	node_remove(Buddy::Node* entry)
	{
		auto prev = entry->prev;
		auto next = entry->next;
		prev->next = next;
		next->prev = prev;
	}

	inline static Buddy::Node*
	node_pop(Buddy::Node* list)
	{
		auto back = list->prev;
		if (back == list)
			return nullptr;
		node_remove(back);
		return back;
	}

	inline static size_t
	next_power_of_2(size_t v)
	{
		--v;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		if constexpr(sizeof(size_t) == 8)
		{
			v |= v >> 32;
		}
		++v;
		return v;
	}

	inline static size_t
	bucket_for_request(Buddy* self, size_t request)
	{
		size_t bucket = self->bucket_max - 1;
		size_t size = BUDDY_MIN_ALLOC;
		while(size < request) {
			--bucket;
			size *= 2;
		}
		return bucket;
	}

	inline static uint8_t*
	ptr_for_node(Buddy* self, size_t index, size_t bucket)
	{
		return self->base_ptr + ((index - (1 << bucket) + 1) << (self->max_alloc_log2 - bucket));
	}

	inline static size_t
	node_for_ptr(Buddy* self, uint8_t* ptr, size_t bucket)
	{
		return ((ptr - self->base_ptr) >> (self->max_alloc_log2 - bucket)) + (1 << bucket) - 1;
	}

	inline static bool
	parent_is_split(Buddy* self, size_t index)
	{
		index = (index - 1) / 2;
		return (bool) ((self->node_is_split[index/8] >> (index % 8)) & 1);
	}

	inline static void
	flip_parent_is_split(Buddy* self, size_t index)
	{
		index = (index - 1) / 2;
		self->node_is_split[index/8] ^= 1 << (index % 8);
	}

	inline static void
	update_max_ptr(Buddy* self, uint8_t* new_ptr)
	{
		if (new_ptr > self->max_ptr)
			self->max_ptr = new_ptr;
	}

	inline static bool
	lower_bucket_count(Buddy* self, size_t bucket)
	{
		while (bucket < self->bucket_count)
		{
			auto root = node_for_ptr(self, self->base_ptr, self->bucket_count);

			/*
			* If the parent isn't SPLIT, that means the node at the current bucket
			* limit is UNUSED and our address space is entirely free. In that case,
			* clear the root free list, increase the bucket limit, and add a single
			* block with the newly-expanded address space to the new root free
			* list.
			*/
			if (parent_is_split(self, root) == false)
			{
				node_remove((Buddy::Node*)self->base_ptr);
				node_init(&self->buckets[--self->bucket_count]);
				node_push(&self->buckets[self->bucket_count], (Buddy::Node*)self->base_ptr);
				continue;
			}

			/*
			* Otherwise, the tree is currently in use. Create a parent node for the
			* current root node in the SPLIT state with a right child on the free
			* list. Make sure to reserve the memory for the free list entry before
			* writing to it. Note that we do not need to flip the "is split" flag
			* for our current parent because it's already on (we know because we
			* just checked it above).
			*/
			auto right_child = ptr_for_node(self, root + 1, self->bucket_count);
			update_max_ptr(self, right_child + sizeof(Buddy::Node));
			node_push(&self->buckets[self->bucket_count], (Buddy::Node*)right_child);
			node_init(&self->buckets[--self->bucket_count]);

			/*
			* Set the grandparent's SPLIT flag so if we need to lower the bucket
			* limit again, we'll know that the new root node we just added is in
			* use.
			*/
			root = (root - 1) / 2;
			if (root != 0)
				flip_parent_is_split(self, root);
		}
		return true;
	}

	Buddy::Buddy(size_t heap_size, Interface* meta_)
	{
		meta = meta_;

		heap_size = next_power_of_2(heap_size);
		max_alloc = heap_size;
		max_alloc_log2 = (size_t)log2(heap_size);

		bucket_max = max_alloc_log2 - BUDDY_MIN_ALLOC_LOG2 + 1;
		buckets = nullptr;

		bucket_count = bucket_max - 1;

		node_is_split = nullptr;

		base_ptr = nullptr;
		max_ptr = nullptr;

		// increase size for buckets
		size_t buckets_size = sizeof(Node) * bucket_max;
		// increase size for node_is_split lookup
		size_t node_is_split_size = (1 << (bucket_max - 1)) / 8;
		size_t total_size = heap_size + buckets_size + node_is_split_size;

		memory = meta->alloc(total_size, alignof(int));

		base_ptr = (uint8_t*)memory.ptr;
		max_ptr = (uint8_t*)memory.ptr;

		buckets = (Node*)((uint8_t*)memory.ptr + heap_size);
		::memset(buckets, 0, buckets_size);

		node_is_split = (uint8_t*)memory.ptr + heap_size + buckets_size;
		::memset(node_is_split, 0, node_is_split_size);

		// init the buckets list with the entire allocation
		node_init(&buckets[bucket_max - 1]);
		node_push(&buckets[bucket_max - 1], (Buddy::Node*)base_ptr);

		update_max_ptr(this, base_ptr + sizeof(Buddy::Node));
	}

	Buddy::~Buddy()
	{
		meta->free(memory);
	}

	Block
	Buddy::alloc(size_t request, uint8_t)
	{
		if (request == 0 || request + BUDDY_HEADER_SIZE > max_alloc)
			return {};

		// find the smallest bucket that will fit this request
		size_t bucket = bucket_for_request(this, request + BUDDY_HEADER_SIZE);
		size_t original_bucket = bucket;

		// search for a bucket with a non empty free list that's as large or larger
		// if there isn't an exact match, we'll need to split a larger one
		while (bucket + 1 != 0)
		{
			// We may need to grow the tree to be able to fit an allocation of this
			// size. Try to grow the tree and stop here if we can't.
			if (lower_bucket_count(this, bucket) == false)
				return {};

			// Try to pop a block off the free list for this bucket. If the free
			// list is empty, we're going to have to split a larger block instead.
			uint8_t* ptr = (uint8_t*)node_pop(&buckets[bucket]);
			if (ptr == nullptr)
			{
				// If we're not at the root of the tree or it's impossible to grow
				// the tree any more, continue on to the next bucket.
				if (bucket != bucket_count || bucket == 0)
				{
					--bucket;
					continue;
				}

				// Otherwise, grow the tree one more level and then pop a block off
				// the free list again. Since we know the root of the tree is used
				// (because the free list was empty), this will add a parent above
				// this node in the SPLIT state and then add the new right child
				// node to the free list for this bucket. Popping the free list will
				// give us this right child.
				if (lower_bucket_count(this, bucket - 1) == false)
					return {};
				ptr = (uint8_t*)node_pop(&buckets[bucket]);
			}

			// Try to expand the address space first before going any further. If we
			// have run out of space, put this block back on the free list and fail.
			size_t size = (size_t) 1 << (max_alloc_log2 - bucket);
			size_t bytes_needed = bucket < original_bucket ? size / 2 + sizeof(Buddy::Node) : size;
			update_max_ptr(this, ptr + bytes_needed);

			// If we got a node off the free list, change the node from UNUSED to
			// USED. This involves flipping our parent's "is split" bit because that
			// bit is the exclusive-or of the UNUSED flags of both children, and our
			// UNUSED flag (which isn't ever stored explicitly) has just changed.
			//
			// Note that we shouldn't ever need to flip the "is split" bit of our
			// grandparent because we know our buddy is USED so it's impossible for
			// our grandparent to be UNUSED (if our buddy chunk was UNUSED, our
			// parent wouldn't ever have been split in the first place).
			size_t i = node_for_ptr(this, ptr, bucket);
			if (i != 0)
				flip_parent_is_split(this, i);

			// If the node we got is larger than we need, split it down to the
			// correct size and put the new unused child nodes on the free list in
			// the corresponding bucket. This is done by repeatedly moving to the
			// left child, splitting the parent, and then adding the right child to
			// the free list.
			while (bucket < original_bucket)
			{
				i = i * 2 + 1;
				++bucket;
				flip_parent_is_split(this, i);
				node_push(&buckets[bucket], (Buddy::Node*)ptr_for_node(this, i + 1, bucket));
			}

			// Now that we have a memory address, write the block header (just the
			// size of the allocation) and return the address immediately after the
			// header.
			*(size_t*)ptr = request;
			return Block{ptr + BUDDY_HEADER_SIZE, request};
		}
		return {};
	}

	void
	Buddy::free(Block block)
	{
		// ignore any attempts to free null pointer
		if (block_is_empty(block))
			return;

		// We were given the address returned by "malloc" so get back to the actual
		// address of the node by subtracting off the size of the block header. Then
		// look up the index of the node corresponding to this address.
		uint8_t* ptr = (uint8_t *) block.ptr - BUDDY_HEADER_SIZE;
		size_t bucket = bucket_for_request(this, *(size_t *) ptr + BUDDY_HEADER_SIZE);
		size_t i = node_for_ptr(this, (uint8_t *) ptr, bucket);

		// Traverse up to the root node, flipping USED blocks to UNUSED and merging
		// UNUSED buddies together into a single UNUSED parent.
		while (i != 0)
		{
			// Change this node from UNUSED to USED. This involves flipping our
			// parent's "is split" bit because that bit is the exclusive-or of the
			// UNUSED flags of both children, and our UNUSED flag (which isn't ever
			// stored explicitly) has just changed.
			flip_parent_is_split(this, i);

			// If the parent is now SPLIT, that means our buddy is USED, so don't
			// merge with it. Instead, stop the iteration here and add ourselves to
			// the free list for our bucket.
			//
			// Also stop here if we're at the current root node, even if that root
			// node is now UNUSED. Root nodes don't have a buddy so we can't merge
			// with one.
			if (parent_is_split(this, i) || bucket == bucket_count)
				break;

			// If we get here, we know our buddy is UNUSED. In this case we should
			// merge with that buddy and continue traversing up to the root node. We
			// need to remove the buddy from its free list here but we don't need to
			// add the merged parent to its free list yet. That will be done once
			// after this loop is finished.
			node_remove((Buddy::Node*)ptr_for_node(this, ((i - 1) ^ 1) + 1, bucket));
			i = (i - 1) / 2;
			--bucket;
		}

		// Add ourselves to the free list for our bucket. We add to the back of the
		// list because "malloc" takes from the back of the list and we want a
		// "free" followed by a "malloc" of the same size to ideally use the same
		// address for better memory locality.
		node_push(&buckets[bucket], (Buddy::Node*)ptr_for_node(this, i, bucket));
	}
}
