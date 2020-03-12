#include "mn/IPC.h"
#include "mn/File.h"
#include "mn/Defer.h"
#include "mn/Fabric.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <mbstring.h>
#include <tchar.h>

namespace mn::ipc
{
	// API
	Mutex
	mutex_new(const Str& name)
	{
		auto os_str = to_os_encoding(name, allocator_top());
		mn_defer(mn::free(os_str));

		auto handle = CreateMutex(0, false, (LPCWSTR)os_str.ptr);
		if (handle == INVALID_HANDLE_VALUE)
			return nullptr;

		return (Mutex)handle;
	}

	void
	mutex_free(Mutex self)
	{
		[[maybe_unused]] auto res = CloseHandle((HANDLE)self);
		assert(res == TRUE);
	}

	void
	mutex_lock(Mutex mtx)
	{
		auto self = (HANDLE)mtx;
		worker_block_ahead();
		WaitForSingleObject(self, INFINITE);
		worker_block_clear();
	}

	bool
	mutex_try_lock(Mutex mtx)
	{
		auto self = (HANDLE)mtx;
		auto res = WaitForSingleObject(self, 0);
		switch(res)
		{
		case WAIT_OBJECT_0:
		case WAIT_ABANDONED:
			return true;
		default:
			return false;
		}
	}

	void
	mutex_unlock(Mutex mtx)
	{
		auto self = (HANDLE)mtx;
		[[maybe_unused]] BOOL res = ReleaseMutex(self);
		assert(res == TRUE);
	}


	void
	ISputnik::dispose()
	{
		sputnik_free(this);
	}

	size_t
	ISputnik::read(mn::Block data)
	{
		return sputnik_read(this, data);
	}

	size_t
	ISputnik::write(mn::Block data)
	{
		return sputnik_write(this, data);
	}

	int64_t
	ISputnik::size()
	{
		return 0;
	}

	Sputnik
	sputnik_new(const mn::Str& name)
	{
		auto pipename = to_os_encoding(mn::str_tmpf("\\\\.\\pipe\\{}", name));
		auto handle = CreateNamedPipe(
			(LPCWSTR)pipename.ptr,
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
			PIPE_UNLIMITED_INSTANCES,
			4ULL * 1024ULL,
			4ULL * 1024ULL,
			NMPWAIT_USE_DEFAULT_WAIT,
			NULL
		);
		if (handle == INVALID_HANDLE_VALUE)
			return nullptr;
		auto self = mn::alloc_construct<ISputnik>();
		self->winos_named_pipe = handle;
		self->name = clone(name);
		return self;
	}

	Sputnik
	sputnik_connect(const mn::Str& name)
	{
		auto pipename = to_os_encoding(mn::str_tmpf("\\\\.\\pipe\\{}", name));
		auto handle = CreateFile(
			(LPCWSTR)pipename.ptr,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			0,
			NULL
		);
		if (handle == INVALID_HANDLE_VALUE)
			return nullptr;

		auto self = mn::alloc_construct<ISputnik>();
		self->winos_named_pipe = handle;
		self->name = clone(name);
		return self;
	}

	void
	sputnik_free(Sputnik self)
	{
		[[maybe_unused]] auto res = CloseHandle((HANDLE)self->winos_named_pipe);
		assert(res == TRUE);
		mn::str_free(self->name);
		mn::free_destruct(self);
	}

	bool
	sputnik_listen(Sputnik self)
	{
		worker_block_ahead();
		auto res = ConnectNamedPipe((HANDLE)self->winos_named_pipe, NULL);
		worker_block_clear();
		return res;
	}

	Sputnik
	sputnik_accept(Sputnik self)
	{
		auto pipename = to_os_encoding(mn::str_tmpf("\\\\.\\pipe\\{}", self->name));
		auto handle = CreateNamedPipe(
			(LPCWSTR)pipename.ptr,
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
			PIPE_UNLIMITED_INSTANCES,
			4ULL * 1024ULL,
			4ULL * 1024ULL,
			NMPWAIT_USE_DEFAULT_WAIT,
			NULL
		);
		if (handle == INVALID_HANDLE_VALUE)
			return nullptr;
		auto other = mn::alloc_construct<ISputnik>();
		other->winos_named_pipe = self->winos_named_pipe;
		other->name = clone(self->name);

		self->winos_named_pipe = handle;

		return other;
	}

	size_t
	sputnik_read(Sputnik self, mn::Block data)
	{
		DWORD res = 0;
		worker_block_ahead();
		ReadFile(
			(HANDLE)self->winos_named_pipe,
			data.ptr,
			DWORD(data.size),
			&res,
			NULL
		);
		worker_block_clear();
		return res;
	}

	size_t
	sputnik_write(Sputnik self, mn::Block data)
	{
		DWORD res = 0;
		worker_block_ahead();
		WriteFile(
			(HANDLE)self->winos_named_pipe,
			data.ptr,
			DWORD(data.size),
			&res,
			NULL
		);
		worker_block_clear();
		return res;
	}

	bool
	sputnik_disconnect(Sputnik self)
	{
		worker_block_ahead();
		FlushFileBuffers((HANDLE)self->winos_named_pipe);
		auto res = DisconnectNamedPipe((HANDLE)self->winos_named_pipe);
		worker_block_clear();
		return res;
	}
}
