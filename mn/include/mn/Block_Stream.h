#pragma once

#include "mn/Exports.h"
#include "mn/Stream.h"

#include <string.h>

namespace mn
{
	struct Block_Stream;

	// just forward declaration because this language require this kind of thing
	inline static size_t
	block_stream_read(Block_Stream& self, Block data);

	struct Block_Stream final : IStream
	{
		Block data;
		int64_t cursor;

		virtual void dispose() override{}

		virtual size_t
		read(Block data) override
		{
			return block_stream_read(*this, data);
		}

		virtual size_t write(Block data) override { return 0; }
		virtual int64_t size() override { return data.size; }

		virtual int64_t
		cursor_op(STREAM_CURSOR_OP op, int64_t arg) override
		{
			switch (op)
			{
			case STREAM_CURSOR_GET:
				return this->cursor;
			case STREAM_CURSOR_MOVE:
				this->cursor += arg;
				return this->cursor;
			case STREAM_CURSOR_SET:
				this->cursor = arg;
				return this->cursor;
			case STREAM_CURSOR_START:
				this->cursor = 0;
				return 0;
			case STREAM_CURSOR_END:
				this->cursor = this->data.size;
				return this->cursor;
			default:
				assert(false && "unreachable");
				return STREAM_CURSOR_ERROR;
			}
		}
	};

	inline static Block_Stream
	block_stream_wrap(Block data)
	{
		Block_Stream self{};
		self.data = data;
		return self;
	}

	inline static size_t
	block_stream_read(Block_Stream& self, Block data)
	{
		if (self.cursor >= self.data.size)
			return 0;

		size_t available_size = self.data.size - self.cursor;
		size_t read_size = available_size > data.size ? data.size : available_size;
		::memcpy(data.ptr, (char*)self.data.ptr + self.cursor, read_size);
		self.cursor += read_size;
		return read_size;
	}

	inline static int64_t
	block_stream_size(const Block_Stream& self)
	{
		return self.data.size;
	}

	inline static int64_t
	block_stream_cursor_pos(const Block_Stream& self)
	{
		return self.cursor;
	}

	inline static void
	block_stream_cursor_move(Block_Stream& self, int64_t offset)
	{
		self.cursor += offset;
	}

	inline static void
	block_stream_cursor_set(Block_Stream& self, int64_t abs)
	{
		self.cursor = abs;
	}

	inline static void
	block_stream_cursor_to_start(Block_Stream& self)
	{
		self.cursor = 0;
	}

	inline static void
	block_stream_cursor_to_end(Block_Stream& self)
	{
		self.cursor = self.data.size;
	}
}
