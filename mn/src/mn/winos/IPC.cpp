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
		return sputnik_read(this, data, INFINITE_TIMEOUT);
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
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
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
			FILE_FLAG_OVERLAPPED,
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
		// this function doesn't map to anything on windows since in socket api it's used to change the state of a socket
		// to be able to accept connections, in named pipes on windows however this is unnecessary
		return true;
	}

	Sputnik
	sputnik_accept(Sputnik self, Timeout timeout)
	{
		// Wait for the Connection
		{
			OVERLAPPED overlapped{};
			overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			mn_defer(CloseHandle(overlapped.hEvent));

			worker_block_ahead();
			mn_defer(worker_block_clear());

			auto connected = ConnectNamedPipe((HANDLE)self->winos_named_pipe, &overlapped);
			auto last_error = GetLastError();
			if (connected == FALSE && last_error != ERROR_IO_PENDING && last_error != ERROR_PIPE_CONNECTED)
				return nullptr;

			DWORD milliseconds = 0;
			if (timeout == INFINITE_TIMEOUT)
				milliseconds = INFINITE;
			else if (timeout == NO_TIMEOUT)
				milliseconds = 0;
			else
				milliseconds = DWORD(timeout.milliseconds);

			auto wakeup = WaitForSingleObject(overlapped.hEvent, milliseconds);
			if (wakeup != WAIT_OBJECT_0)
				return nullptr;
		}

		// accept the connection
		auto pipename = to_os_encoding(mn::str_tmpf("\\\\.\\pipe\\{}", self->name));
		auto handle = CreateNamedPipe(
			(LPCWSTR)pipename.ptr,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
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
	sputnik_read(Sputnik self, mn::Block data, Timeout timeout)
	{
		DWORD bytes_read = 0;
		OVERLAPPED overlapped{};
		overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		worker_block_ahead();
		ReadFile(
			(HANDLE)self->winos_named_pipe,
			data.ptr,
			DWORD(data.size),
			&bytes_read,
			&overlapped
		);

		DWORD milliseconds = 0;
		if (timeout == INFINITE_TIMEOUT)
			milliseconds = INFINITE;
		else if (timeout == NO_TIMEOUT)
			milliseconds = 0;
		else
			milliseconds = DWORD(timeout.milliseconds);
		WaitForSingleObject(overlapped.hEvent, milliseconds);
		worker_block_clear();
		CloseHandle(overlapped.hEvent);
		return overlapped.InternalHigh;
	}

	size_t
	sputnik_write(Sputnik self, Block data)
	{
		DWORD bytes_written = 0;
		worker_block_ahead();
		WriteFile(
			(HANDLE)self->winos_named_pipe,
			data.ptr,
			DWORD(data.size),
			&bytes_written,
			NULL
		);
		worker_block_clear();
		return bytes_written;
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

	void
	sputnik_msg_write(Sputnik self, Block data)
	{
		uint64_t len = data.size;
		sputnik_write(self, block_from(len));
		sputnik_write(self, data);
	}

	Msg_Read_Return
	sputnik_msg_read(Sputnik self, Block data, Timeout timeout)
	{
		// if we don't have any remaining bytes in the message
		if(self->read_msg_size == 0)
		{
			uint8_t* it = (uint8_t*)&self->read_msg_size;
			size_t read_size = sizeof(self->read_msg_size);
			Timeout t = timeout;
			while(read_size > 0)
			{
				auto res = sputnik_read(self, {it, read_size}, t);
				if (res == 0)
					return Msg_Read_Return{};
				t = INFINITE_TIMEOUT;
				it += res;
				read_size -= res;
			}
		}

		// try reading a block of the message
		size_t read_size = data.size;
		if(data.size > self->read_msg_size)
			read_size = self->read_msg_size;
		auto res = sputnik_read(self, {data.ptr, read_size}, timeout);
		self->read_msg_size -= res;
		return {res, self->read_msg_size};
	}

	Str
	sputnik_msg_read_alloc(Sputnik self, Timeout timeout, Allocator allocator)
	{
		auto res = str_with_allocator(allocator);
		if(self->read_msg_size != 0)
			return res;

		auto [consumed, remaining] = sputnik_msg_read(self, {}, timeout);
		if (remaining == 0 && consumed == 0)
			return res;

		str_resize(res, remaining);
		auto block = block_from(res);
		while (remaining > 0)
		{
			auto [consumed2, remaining2] = sputnik_msg_read(self, block, timeout);
			remaining -= consumed2;
			block = block + consumed2;
		}
		assert(remaining == 0);
		return res;
	}
}
