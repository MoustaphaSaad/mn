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

	// OS Named Pipes
	typedef struct IPipe* Pipe;
	struct IPipe final : IStream
	{
		union
		{
			void* winos_handle;

			struct
			{
				int handle;
				bool owner;
			} linux_os;
		};
		mn::Str name;

		MN_EXPORT void
		dispose() override;

		MN_EXPORT size_t
		read(mn::Block data) override;

		MN_EXPORT size_t
		write(mn::Block data) override;

		MN_EXPORT int64_t
		size() override;
	};

	MN_EXPORT Pipe
	pipe_new(const mn::Str& name);

	inline static Pipe
	pipe_new(const char* name)
	{
		return pipe_new(mn::str_lit(name));
	}

	MN_EXPORT Pipe
	pipe_connect(const mn::Str& name);

	inline static Pipe
	pipe_connect(const char* name)
	{
		return pipe_connect(mn::str_lit(name));
	}

	MN_EXPORT void
	pipe_free(Pipe self);

	MN_EXPORT size_t
	pipe_read(Pipe self, mn::Block data);

	MN_EXPORT size_t
	pipe_write(Pipe self, mn::Block data);

	MN_EXPORT uint32_t
	pipe_listen(Pipe self);

	MN_EXPORT bool
	pipe_disconnect(Pipe self);
}