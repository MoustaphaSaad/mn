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

	MN_EXPORT Sputnik
	sputnik_new(const mn::Str& name);

	inline static Sputnik
	sputnik_new(const char* name)
	{
		return sputnik_new(mn::str_lit(name));
	}

	MN_EXPORT Sputnik
	sputnik_connect(const mn::Str& name);

	inline static Sputnik
	sputnik_connect(const char* name)
	{
		return sputnik_connect(mn::str_lit(name));
	}

	MN_EXPORT void
	sputnik_free(Sputnik self);

	MN_EXPORT bool
	sputnik_listen(Sputnik self);

	MN_EXPORT Sputnik
	sputnik_accept(Sputnik self);

	MN_EXPORT size_t
	sputnik_read(Sputnik self, mn::Block data);

	MN_EXPORT size_t
	sputnik_write(Sputnik self, mn::Block data);

	MN_EXPORT bool
	sputnik_disconnect(Sputnik self);
}