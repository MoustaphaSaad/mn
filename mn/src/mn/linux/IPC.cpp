#include "mn/IPC.h"
#include "mn/Str.h"
#include "mn/Memory.h"
#include "mn/Fabric.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
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
	IPipe::dispose()
	{
		pipe_free(this);
	}

	size_t
	IPipe::read(mn::Block data)
	{
		return pipe_read(this, data);
	}

	size_t
	IPipe::write(mn::Block data)
	{
		return pipe_write(this, data);
	}

	int64_t
	IPipe::size()
	{
		return 0;
	}

	Pipe
	pipe_new(const mn::Str& name)
	{
		auto res = ::mkfifo(name.ptr, 0666);
		if(res == -1 && errno != EEXIST)
			return nullptr;
		auto handle = ::open(name.ptr, O_RDWR);
		if(handle == -1)
			return nullptr;
		auto self = mn::alloc_construct<IPipe>();
		self->linux_os.handle = handle;
		self->linux_os.owner = true;
		self->name = clone(name);
		return self;
	}

	Pipe
	pipe_connect(const mn::Str& name)
	{
		auto handle = ::open(name.ptr, O_RDWR);
		if(handle == -1)
			return nullptr;

		auto self = mn::alloc_construct<IPipe>();
		self->linux_os.handle = handle;
		self->linux_os.owner = false;
		self->name = clone(name);

		// send the process id on connect
		worker_block_ahead();
		auto pid = ::getpid();
		assert(sizeof(pid) == 4 && "pid is bigger than 4 bytes");
		uint32_t pid_number = uint32_t(pid);
		size_t write_size = sizeof(pid_number);
		uint8_t* it = (uint8_t*)&pid_number;
		while(write_size > 0)
		{
			auto res = ::write(self->linux_os.handle, it, write_size);
			if(res == -1)
				continue;
			it += res;
			write_size -= res;
		}
		worker_block_clear();

		return self;
	}

	void
	pipe_free(Pipe self)
	{
		::close(self->linux_os.handle);
		if(self->linux_os.owner)
			::unlink(self->name.ptr);
		mn::str_free(self->name);
		mn::free_destruct(self);
	}

	size_t
	pipe_read(Pipe self, mn::Block data)
	{
		worker_block_ahead();
		auto res = ::read(self->linux_os.handle, data.ptr, data.size);
		worker_block_clear();
		return res;
	}

	size_t
	pipe_write(Pipe self, mn::Block data)
	{
		worker_block_ahead();
		auto res = ::write(self->linux_os.handle, data.ptr, data.size);
		worker_block_clear();
		return res;
	}

	uint32_t
	pipe_listen(Pipe self)
	{
		// wait for process id
		uint32_t pid = 0;
		size_t read_size = sizeof(pid);
		uint8_t* it = (uint8_t*)&pid;
		worker_block_ahead();
		while(read_size > 0)
		{
			auto res = ::read(self->linux_os.handle, it, read_size);
			if(res == -1)
				continue;
			assert(res >= 0);
			it += res;
			read_size -= res;
		}
		worker_block_clear();
		return pid;
	}

	bool
	pipe_disconnect(Pipe self)
	{
		if(self->linux_os.owner == false)
			return false;
		// actually do nothing
		return true;
	}
}
