#include "mn/IPC.h"
#include "mn/Str.h"
#include "mn/Memory.h"
#include "mn/Fabric.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <assert.h>

namespace mn::ipc
{
	bool
	_mutex_try_lock(Mutex self, int64_t offset, int64_t size)
	{
		assert(offset >= 0 && size >= 0);
		struct flock fl{};
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(intptr_t(self), F_SETLK, &fl) != -1;
	}

	bool
	_mutex_unlock(Mutex self, int64_t offset, int64_t size)
	{
		assert(offset >= 0 && size >= 0);
		struct flock fl{};
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(intptr_t(self), F_SETLK, &fl) != -1;
	}

	// API
	Mutex
	mutex_new(const Str& name)
	{
		int flags = O_WRONLY | O_CREAT | O_APPEND;

		int handle = ::open(name.ptr, flags, S_IRWXU);
		if(handle == -1)
			return nullptr;

		return Mutex(handle);
	}

	void
	mutex_free(Mutex mtx)
	{
		::close(intptr_t(mtx));
	}

	void
	mutex_lock(Mutex mtx)
	{
		worker_block_ahead();

		worker_block_on([&]{
			return _mutex_try_lock(mtx, 0, 0);
		});

		worker_block_clear();
	}

	bool
	mutex_try_lock(Mutex mtx)
	{
		return _mutex_try_lock(mtx, 0, 0);
	}

	void
	mutex_unlock(Mutex mtx)
	{
		_mutex_unlock(mtx, 0, 0);
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
		sockaddr_un addr{};
		addr.sun_family = AF_LOCAL;
		assert(name.count < sizeof(addr.sun_path) && "name is too long");
		size_t name_length = name.count;
		if(name.count >= sizeof(addr.sun_path))
			name_length = sizeof(addr.sun_path);
		::memcpy(addr.sun_path, name.ptr, name_length);

		int handle = ::socket(AF_UNIX, SOCK_STREAM, 0);
		if(handle < 0)
			return nullptr;

		::unlink(addr.sun_path);
		auto res = ::bind(handle, (sockaddr*)&addr, SUN_LEN(&addr));
		if(res < 0)
		{
			::close(handle);
			return nullptr;
		}
		auto self = mn::alloc_construct<ISputnik>();
		self->linux_domain_socket = handle;
		self->name = mn::str_from_substr(name.ptr, name.ptr + name_length);
		return self;
	}

	Sputnik
	sputnik_connect(const mn::Str& name)
	{
		sockaddr_un addr{};
		addr.sun_family = AF_LOCAL;
		assert(name.count < sizeof(addr.sun_path) && "name is too long");
		size_t name_length = name.count;
		if(name.count >= sizeof(addr.sun_path))
			name_length = sizeof(addr.sun_path);
		::memcpy(addr.sun_path, name.ptr, name_length);

		int handle = ::socket(AF_UNIX, SOCK_STREAM, 0);
		if(handle < 0)
			return nullptr;

		worker_block_ahead();
		if(::connect(handle, (sockaddr*)&addr, sizeof(sockaddr_un)) < 0)
		{
			::close(handle);
			return nullptr;
		}
		worker_block_clear();

		auto self = mn::alloc_construct<ISputnik>();
		self->linux_domain_socket = handle;
		self->name = mn::str_from_substr(name.ptr, name.ptr + name_length);
		return self;
	}

	void
	sputnik_free(Sputnik self)
	{
		::close(self->linux_domain_socket);
		mn::str_free(self->name);
		mn::free_destruct(self);
	}

	bool
	sputnik_listen(Sputnik self)
	{
		worker_block_ahead();
		int res = ::listen(self->linux_domain_socket, SOMAXCONN);
		worker_block_clear();
		if (res == -1)
			return false;
		return true;
	}

	Sputnik
	sputnik_accept(Sputnik self)
	{
		auto handle = ::accept(self->linux_domain_socket, 0, 0);
		if(handle == -1)
			return nullptr;
		auto other = mn::alloc_construct<ISputnik>();
		other->linux_domain_socket = handle;
		other->name = clone(self->name);
		return other;
	}

	size_t
	sputnik_read(Sputnik self, Block data)
	{
		worker_block_ahead();
		auto res = ::read(self->linux_domain_socket, data.ptr, data.size);
		worker_block_clear();
		return res;
	}

	size_t
	sputnik_write(Sputnik self, Block data)
	{
		worker_block_ahead();
		auto res = ::write(self->linux_domain_socket, data.ptr, data.size);
		worker_block_clear();
		return res;
	}

	bool
	sputnik_disconnect(Sputnik self)
	{
		return ::unlink(self->name.ptr) == 0;
	}

	void
	sputnik_msg_write(Sputnik self, Block data)
	{
		uint64_t len = data.size;
		sputnik_write(self, block_from(len));
		sputnik_write(self, data);
	}

	Msg_Read_Return
	sputnik_msg_read(Sputnik self, Block data)
	{
		// if we don't have any remaining bytes in the message
		if(self->read_msg_size == 0)
		{
			uint8_t* it = (uint8_t*)&self->read_msg_size;
			size_t read_size = sizeof(self->read_msg_size);
			while(read_size > 0)
			{
				auto res = sputnik_read(self, {it, read_size});
				it += res;
				read_size -= res;
			}
		}

		// try reading a block of the message
		size_t read_size = data.size;
		if(data.size > self->read_msg_size)
			read_size = self->read_msg_size;
		auto res = sputnik_read(self, {data.ptr, read_size});
		self->read_msg_size -= res;
		return {res, self->read_msg_size};
	}

	Str
	sputnik_msg_read_alloc(Sputnik self, Allocator allocator)
	{
		auto block = str_with_allocator(allocator);
		if(self->read_msg_size != 0)
			return block;

		auto [consumed, remaining] = sputnik_msg_read(self, {});
		assert(consumed == 0);

		str_resize(block, remaining);
		auto [consumed2, remaining2] = sputnik_msg_read(self, block_from(block));
		assert(consumed2 == remaining && remaining2 == 0);
		return block;
	}
}
