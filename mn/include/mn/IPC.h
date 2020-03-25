#pragma once

#include "mn/Exports.h"
#include "mn/Str.h"
#include "mn/Stream.h"

namespace mn::ipc
{
	typedef struct IIPC_Mutex *Mutex;

	MN_EXPORT Mutex
	mutex_new(const Str& name);

	inline static Mutex
	mutex_new(const char* name)
	{
		return mutex_new(str_lit(name));
	}

	MN_EXPORT void
	mutex_free(Mutex self);

	inline static void
	destruct(Mutex self)
	{
		mn::ipc::mutex_free(self);
	}

	MN_EXPORT void
	mutex_lock(Mutex self);

	MN_EXPORT bool
	mutex_try_lock(Mutex self);

	MN_EXPORT void
	mutex_unlock(Mutex self);

	// OS communication primitives
	typedef struct ISputnik* Sputnik;
	struct ISputnik final : IStream
	{
		union
		{
			void* winos_named_pipe;
			int linux_domain_socket;
		};
		Str name;
		uint64_t read_msg_size;

		MN_EXPORT void
		dispose() override;

		MN_EXPORT size_t
		read(Block data) override;

		MN_EXPORT size_t
		write(Block data) override;

		MN_EXPORT int64_t
		size() override;
	};

	MN_EXPORT Sputnik
	sputnik_new(const Str& name);

	inline static Sputnik
	sputnik_new(const char* name)
	{
		return sputnik_new(str_lit(name));
	}

	MN_EXPORT Sputnik
	sputnik_connect(const Str& name);

	inline static Sputnik
	sputnik_connect(const char* name)
	{
		return sputnik_connect(str_lit(name));
	}

	MN_EXPORT void
	sputnik_free(Sputnik self);

	MN_EXPORT bool
	sputnik_listen(Sputnik self);

	MN_EXPORT Sputnik
	sputnik_accept(Sputnik self, Timeout timeout);

	MN_EXPORT size_t
	sputnik_read(Sputnik self, Block data, Timeout timeout);

	MN_EXPORT size_t
	sputnik_write(Sputnik self, Block data);

	MN_EXPORT bool
	sputnik_disconnect(Sputnik self);

	// sputnik message protocol

	// sputnik_msg_write writes a message unit to sputnik which is {len: 8 bytes, the message}
	MN_EXPORT void
	sputnik_msg_write(Sputnik self, Block data);

	struct Msg_Read_Return
	{
		size_t consumed;
		uint64_t remaining;
	};

	// sputnik_msg_read reads a message unit from sputnik
	MN_EXPORT Msg_Read_Return
	sputnik_msg_read(Sputnik self, Block data, Timeout timeout);

	// sputnik_msg_read_alloc allocates and reads a single message
	MN_EXPORT Str
	sputnik_msg_read_alloc(Sputnik self, Timeout timeout, Allocator allocator = allocator_top());
}