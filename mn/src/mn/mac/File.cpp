#include "mn/File.h"
#include "mn/OS.h"
#include "mn/Fabric.h"

#define _LARGEFILE64_SOURCE 1
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>

#include <assert.h>

namespace mn
{
	File
	_file_stdout()
	{
		static IFile _stdout{};
		_stdout.macos_handle = STDOUT_FILENO;
		return &_stdout;
	}

	File
	_file_stderr()
	{
		static IFile _stderr{};
		_stderr.macos_handle = STDERR_FILENO;
		return &_stderr;
	}

	File
	_file_stdin()
	{
		static IFile _stdin{};
		_stdin.macos_handle = STDIN_FILENO;
		return &_stdin;
	}

	inline static bool
	_is_std_file(int h)
	{
		return (
			h == _file_stdout()->macos_handle ||
			h == _file_stderr()->macos_handle ||
			h == _file_stdin()->macos_handle
		);
	}

	//API
	void
	IFile::dispose()
	{
		if (macos_handle != -1 &&
			_is_std_file(macos_handle) == false)
		{
			::close(macos_handle);
		}
		free(this);
	}

	size_t
	IFile::read(Block data)
	{
		worker_block_ahead();
		auto res = ::read(macos_handle, data.ptr, data.size);
		worker_block_clear();
		return res;
	}

	size_t
	IFile::write(Block data)
	{
		worker_block_ahead();
		auto res = ::write(macos_handle, data.ptr, data.size);
		worker_block_clear();
		return res;
	}

	int64_t
	IFile::size()
	{
		struct stat file_stats;
		if(::fstat(macos_handle, &file_stats) == 0)
		{
			return file_stats.st_size;
		}
		return -1;
	}

	Block
	to_os_encoding(const Str& utf8, Allocator allocator)
	{
		return block_clone(block_from(utf8), allocator);
	}

	Block
	to_os_encoding(const char* utf8, Allocator allocator)
	{
		return to_os_encoding(str_lit(utf8), allocator);
	}

	Str
	from_os_encoding(Block os_str, Allocator allocator)
	{
		return str_from_c((char*)os_str.ptr, allocator);
	}

	File
	file_stdout()
	{
		static File _f = _file_stdout();
		return _f;
	}

	File
	file_stderr()
	{
		static File _f = _file_stderr();
		return _f;
	}

	File
	file_stdin()
	{
		static File _f = _file_stdin();
		return _f;
	}

	File
	file_open(const char* filename, IO_MODE io_mode, OPEN_MODE open_mode, SHARE_MODE share_mode)
	{
		int flags = 0;

		//translate the io mode
		switch(io_mode)
		{
			case IO_MODE::READ:
				flags |= O_RDONLY;
				break;

			case IO_MODE::WRITE:
				flags |= O_WRONLY;
				break;

			case IO_MODE::READ_WRITE:
			default:
				flags |= O_RDWR;
				break;
		}

		//translate the open mode
		switch(open_mode)
		{
			case OPEN_MODE::CREATE_ONLY:
				flags |= O_CREAT;
				flags |= O_EXCL;
				break;

			case OPEN_MODE::CREATE_APPEND:
				flags |= O_CREAT;
				flags |= O_APPEND;
				break;

			case OPEN_MODE::OPEN_ONLY:
				//do nothing
				break;

			case OPEN_MODE::OPEN_OVERWRITE:
				flags |= O_TRUNC;
				break;

			case OPEN_MODE::OPEN_APPEND:
				flags |= O_APPEND;
				break;

			case OPEN_MODE::CREATE_OVERWRITE:
			default:
				flags |= O_CREAT;
				flags |= O_TRUNC;
				break;
		}

		// Linux doesn't support the granularity of file sharing like windows so we only support
		// NONE which is available only in O_CREAT mode
		switch(share_mode)
		{
			case SHARE_MODE_NONE:
				if(flags & O_CREAT)
					flags |= O_EXCL;
				break;
			
			default:
				break;
		}

		int macos_handle = ::open(filename, flags, S_IRWXU);
		if(macos_handle == -1)
			return nullptr;

		File self = alloc_construct<IFile>();
		self->macos_handle = macos_handle;
		return self;
	}

	void
	file_close(File self)
	{
		self->dispose();
	}

	bool
	file_valid(File self)
	{
		return self->macos_handle != -1;
	}

	size_t
	file_write(File self, Block data)
	{
		return self->write(data);
	}

	size_t
	file_read(File self, Block data)
	{
		return self->read(data);
	}

	int64_t
	file_size(File self)
	{
		struct stat file_stats;
		if(::fstat(self->macos_handle, &file_stats) == 0)
		{
			return file_stats.st_size;
		}
		return -1;
	}

	int64_t
	file_cursor_pos(File self)
	{
		off_t offset = 0;
		return ::lseek(self->macos_handle, offset, SEEK_CUR);
	}

	bool
	file_cursor_move(File self, int64_t move_offset)
	{
		off_t offset = move_offset;
		return ::lseek(self->macos_handle, offset, SEEK_CUR) != -1;
	}

	bool
	file_cursor_set(File self, int64_t absolute)
	{
		off_t offset = absolute;
		return ::lseek(self->macos_handle, offset, SEEK_SET) != -1;
	}

	bool
	file_cursor_move_to_start(File self)
	{
		off_t offset = 0;
		return ::lseek(self->macos_handle, offset, SEEK_SET) != -1;
	}

	bool
	file_cursor_move_to_end(File self)
	{
		off_t offset = 0;
		return ::lseek(self->macos_handle, offset, SEEK_END) != -1;
	}

	bool
	file_write_try_lock(File self, int64_t offset, int64_t size)
	{
		assert(offset >= 0 && size >= 0);
		struct flock fl{};
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(self->macos_handle, F_SETLK, &fl) != -1;
	}

	void
	file_write_lock(File handle, int64_t offset, int64_t size)
	{
		worker_block_on([&]{
			return file_write_try_lock(handle, offset, size);
		});
	}

	bool
	file_write_unlock(File self, int64_t offset, int64_t size)
	{
		assert(offset >= 0 && size >= 0);
		struct flock fl{};
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(self->macos_handle, F_SETLK, &fl) != -1;
	}

	bool
	file_read_try_lock(File self, int64_t offset, int64_t size)
	{
		assert(offset >= 0 && size >= 0);
		struct flock fl{};
		fl.l_type = F_RDLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(self->macos_handle, F_SETLK, &fl) != -1;
	}

	void
	file_read_lock(File handle, int64_t offset, int64_t size)
	{
		worker_block_on([&]{
			return file_read_try_lock(handle, offset, size);
		});
	}

	bool
	file_read_unlock(File self, int64_t offset, int64_t size)
	{
		assert(offset >= 0 && size >= 0);
		struct flock fl{};
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = size;
		return fcntl(self->macos_handle, F_SETLK, &fl) != -1;
	}
}
