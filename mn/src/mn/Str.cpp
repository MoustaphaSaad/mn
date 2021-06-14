#include "mn/Str.h"

namespace mn
{
	struct Rabin_Karp_State
	{
		uint32_t hash, pow;
	};

	constexpr uint32_t PRIME_RABIN_KARP = 16777619;

	inline static Rabin_Karp_State
	_hash_str_rabin_karp(const Str& str)
	{
		Rabin_Karp_State res{0, 1};

		for (size_t i = 0; i < str.count; ++i)
			res.hash = res.hash * PRIME_RABIN_KARP + uint32_t(str.ptr[i]);
		auto sq = PRIME_RABIN_KARP;
		for (size_t i = str.count; i > 0; i >>= 1)
		{
			if ((i & 1) != 0)
				res.pow *= sq;
			sq *= sq;
		}
		return res;
	}

	inline static Rabin_Karp_State
	_hash_str_rabin_karp_reverse(const Str& str)
	{
		assert(str.count > 0);

		Rabin_Karp_State res{0, 1};

		for (size_t i = 0; i < str.count; ++i)
		{
			auto rev_i = str.count - i - 1;
			res.hash = res.hash * PRIME_RABIN_KARP + uint32_t(str.ptr[rev_i]);
		}
		auto sq = PRIME_RABIN_KARP;
		for (size_t i = str.count; i > 0; i >>= 1)
		{
			if ((i & 1) != 0)
				res.pow *= sq;
			sq *= sq;
		}
		return res;
	}

	// API
	Str
	str_new()
	{
		return buf_new<char>();
	}

	Str
	str_with_allocator(Allocator allocator)
	{
		return buf_with_allocator<char>(allocator);
	}

	Str
	str_from_c(const char* str, Allocator allocator)
	{
		if (str == nullptr) return str_with_allocator(allocator);
		Str self = str_with_allocator(allocator);
		buf_resize(self, ::strlen(str) + 1);
		--self.count;
		::memcpy(self.ptr, str, self.count);
		self.ptr[self.count] = '\0';
		return self;
	}

	Str
	str_from_substr(const char* begin, const char* end, Allocator allocator)
	{
		assert(end >= begin &&
			   "Invalid substring");

		Str self = str_with_allocator(allocator);

		//this is an empty string
		if(end == begin)
			return self;

		buf_resize(self, (end - begin) + 1);
		--self.count;
		::memcpy(self.ptr, begin, self.count);
		self.ptr[self.count] = '\0';
		return self;
	}

	Str
	str_lit(const char* lit)
	{
		Str self{};
		self.ptr = (char*)lit;
		if(lit)
		{
			self.cap = ::strlen(lit) + 1;
			self.count = self.cap - 1;
		}
		return self;
	}

	void
	str_free(Str& self)
	{
		buf_free(self);
	}

	void
	str_push(Str& self, const char* str)
	{
		if (str == nullptr)
			return;
		size_t str_len = ::strlen(str);
		size_t self_len = self.count;
		buf_resize(self, self.count + str_len + 1);
		--self.count;
		::memcpy(self.ptr + self_len, str, str_len);
		self.ptr[self.count] = '\0';
	}

	void
	str_block_push(Str& self, Block block)
	{
		size_t self_len = self.count;
		buf_resize(self, self.count + block.size + 1);
		--self.count;
		::memcpy(self.ptr + self_len, block.ptr, block.size);
		self.ptr[self.count] = '\0';
	}

	void
	str_push(Str& self, Rune r)
	{
		auto old_count = self.count;
		// +5 = 4 for the rune + 1 for the null termination
		buf_resize(self, self.count + 5);

		auto width = rune_encode(r, Block{ self.ptr + old_count, 4 });
		assert(width <= 4);
		// adjust the size back
		self.count -= (4 - width) + 1;
		self.ptr[self.count] = '\0';
	}

	void
	str_null_terminate(Str& self)
	{
		if (self.count == 0)
		{
			if (self.cap > 0)
				self.ptr[self.count] = '\0';
			return;
		}
		buf_reserve(self, 1);
		self.ptr[self.count] = '\0';
	}

	size_t
	str_find(const Str& input, const Str& target, size_t start)
	{
		if (start >= input.count || input.count - start < target.count)
		{
			return size_t(-1);
		}

		// make a local dummy copy and advance the pointers etc...
		auto self = input;
		self.ptr += start;
		self.count -= start;

		if (target.count == 0)
		{
			return 0 + start;
		}
		else if (target.count == 1)
		{
			for (size_t i = 0; i < self.count; ++i)
				if (self.ptr[i] == target.ptr[0])
					return i + start;
			return size_t(-1);
		}
		else if (target.count == self.count)
		{
			if (target == self)
				return 0 + start;
			return size_t(-1);
		}
		else if (target.count > self.count)
		{
			return size_t(-1);
		}

		auto [hash, pow] = _hash_str_rabin_karp(target);

		uint32_t h{};
		for (size_t i = 0; i < target.count; ++i)
		{
			h = h * PRIME_RABIN_KARP + uint32_t(self.ptr[i]);
		}

		if (h == hash && ::memcmp(self.ptr, target.ptr, target.count) == 0)
		{
			return 0 + start;
		}

		for (size_t i = target.count; i < self.count;)
		{
			h *= PRIME_RABIN_KARP;
			h += uint32_t(self.ptr[i]);
			h -= pow * uint32_t(self.ptr[i - target.count]);
			i += 1;
			if (h == hash && ::memcmp(self.ptr + i - target.count, target.ptr, target.count) == 0)
			{
				return i - target.count + start;
			}
		}
		return size_t(-1);
	}

	size_t
	str_find_last(const Str& input, const Str& target, size_t index)
	{
		// make a local dummy copy
		auto self = input;

		if (index < self.count)
		{
			self.count = index + 1;
		}

		if (target.count == 0)
		{
			return self.count;
		}
		else if (target.count == 1)
		{
			for (size_t i = 0; i < self.count; ++i)
			{
				auto rev_i = self.count - i - 1;
				if (self.ptr[rev_i] == target.ptr[0])
					return rev_i;
			}
			return size_t(-1);
		}
		else if (target.count == self.count)
		{
			if (::memcmp(self.ptr, target.ptr, target.count) == 0)
				return 0;
			return size_t(-1);
		}
		else if (target.count > self.count)
		{
			return size_t(-1);
		}

		auto [hash, pow] = _hash_str_rabin_karp_reverse(target);
		auto last = self.count - target.count;

		uint32_t h{};
		for (size_t i = self.count - 1; i >= last; --i)
			h = h * PRIME_RABIN_KARP + uint32_t(self.ptr[i]);
		if (h == hash && ::memcmp(self.ptr + last, target.ptr, target.count) == 0)
			return last;

		for (size_t i = 0; i < last; i++)
		{
			auto rev_i = last - i - 1;
			h *= PRIME_RABIN_KARP;
			h += uint32_t(self.ptr[rev_i]);
			h -= pow * uint32_t(self.ptr[rev_i + target.count]);
			if (h == hash && ::memcmp(self.ptr + rev_i, target.ptr, target.count) == 0)
				return rev_i;
		}
		return size_t(-1);
	}

	size_t
	str_find(const Str& self, Rune r, size_t start_in_bytes)
	{
		assert(start_in_bytes < self.count);
		for(auto it = begin(self) + start_in_bytes; it != end(self); it = rune_next(it))
		{
			Rune c = rune_read(it);
			if(c == r)
				return it - self.ptr;
		}
		return size_t(-1);
	}

	void
	str_replace(Str& self, char to_remove, char to_add)
	{
		for(size_t i = 0; i < self.count; ++i)
			if(self[i] == to_remove)
				self[i] = to_add;
	}

	void
	str_replace(Str& self, const Str& search, const Str& replace)
	{
		auto out = str_with_allocator(self.allocator);
		buf_reserve(out, self.count);
		// find the first pattern or -1
		size_t search_it = str_find(self, search, 0);
		size_t it		 = 0;
		// while we didn't finish the string
		while (it < self.count)
		{
			// if search_str is not found then put search_it to the end of string
			if (search_it == size_t(-1))
			{
				// push the remaining content
				if (it < self.count)
					str_block_push(out, Block{self.ptr + it, self.count - it});

				// exit we finished the string
				break;
			}

			// push the preceding content
			if (search_it > it)
				str_block_push(out, Block{self.ptr + it, search_it - it});

			// push the replacement string
			str_block_push(out, block_from(replace));

			// advance content iterator by search string
			it = search_it + search.count;

			// find for next pattern
			search_it = str_find(self, search, it);
		}
		str_free(self);
		self = out;
	}

	Buf<Str>
	str_split(const Str& self, const Str& delim, bool skip_empty, Allocator allocator)
	{
		Buf<Str> result = buf_with_allocator<Str>(allocator);

		size_t current_index = 0;

		while (true)
		{
			if (current_index + delim.count > self.count)
				break;

			size_t delim_index = str_find(self, delim, current_index);

			if (delim_index == size_t(-1))
				break;

			bool skip = skip_empty && current_index == delim_index;

			if (!skip)
				buf_push(result, str_from_substr(self.ptr + current_index, self.ptr + delim_index, allocator));

			current_index = delim_index + delim.count;

			if (current_index == self.count)
				break;
		}

		if (current_index != self.count)
			buf_push(result, str_from_substr(self.ptr + current_index, self.ptr + self.count, allocator));
		else if (!skip_empty && current_index == self.count)
			buf_push(result, str_with_allocator(allocator));

		return result;
	}

	bool
	str_prefix(const Str& self, const Str& prefix)
	{
		if(self.count < prefix.count)
			return false;
		return ::memcmp(self.ptr, prefix.ptr, prefix.count) == 0;
	}

	bool
	str_suffix(const Str& self, const Str& suffix)
	{
		if(self.count < suffix.count)
			return false;
		return ::memcmp(self.ptr + self.count - suffix.count, suffix.ptr, suffix.count) == 0;
	}

	void
	str_resize(Str& self, size_t size)
	{
		buf_resize(self, size + 1);
		--self.count;
		self.ptr[self.count] = '\0';
	}

	void
	str_reserve(Str& self, size_t size)
	{
		buf_reserve(self, size);
	}

	void
	str_clear(Str& self)
	{
		buf_clear(self);
		if (self.cap > 0)
			self.ptr[self.count] = '\0';
	}

	Str
	str_clone(const Str& other, Allocator allocator)
	{
		Str self = str_with_allocator(allocator);
		buf_resize(self, other.count + 1);
		--self.count;
		::memcpy(self.ptr, other.ptr, self.count);
		self.ptr[self.count] = '\0';
		return self;
	}

	void
	str_lower(Str& self)
	{
		auto new_str = str_with_allocator(self.allocator);
		str_reserve(new_str, self.count);
		for(const char* it = begin(self); it != end(self); it = rune_next(it))
			str_push(new_str, rune_lower(rune_read(it)));
		str_free(self);
		self = new_str;
	}

	void
	str_upper(Str& self)
	{
		auto new_str = str_with_allocator(self.allocator);
		str_reserve(new_str, self.count);
		for(const char* it = begin(self); it != end(self); it = rune_next(it))
			str_push(new_str, rune_upper(rune_read(it)));
		str_free(self);
		self = new_str;
	}
}
