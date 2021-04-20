#include "mn/File.h"
#include "mn/Defer.h"
#include "mn/Memory.h"
#include "mn/Thread.h"
#include "mn/Fabric.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mbstring.h>
#include <tchar.h>
#undef DELETE

namespace mn
{
	inline static Str
	_from_os_encoding(Block os_str, Allocator allocator)
	{
		[[maybe_unused]] auto ptr = (WCHAR*)os_str.ptr;
		assert(ptr[os_str.size / sizeof(WCHAR)] == 0);

		int size_needed = WideCharToMultiByte(CP_UTF8, NULL, (LPWSTR)os_str.ptr,
			int(os_str.size / sizeof(WCHAR)), NULL, 0, NULL, NULL);
		if (size_needed == 0)
			return str_with_allocator(allocator);

		Str buffer = str_with_allocator(allocator);
		buf_resize(buffer, size_needed);

		size_needed = WideCharToMultiByte(CP_UTF8, NULL, (LPWSTR)os_str.ptr,
			int(os_str.size / sizeof(WCHAR)), buffer.ptr, int(buffer.count), NULL, NULL);
		--buffer.count;
		return buffer;
	}

	inline static Block
	_to_os_encoding(Block utf8, Allocator allocator)
	{
		int size_needed = MultiByteToWideChar(CP_UTF8,
			0, (LPCCH)utf8.ptr, int(utf8.size), NULL, 0);

		//+1 for the null termination
		size_t required_size = (size_needed + 1) * sizeof(WCHAR);
		Block buffer = alloc_from(allocator, required_size, alignof(WCHAR));

		MultiByteToWideChar(CP_UTF8, 0, (LPCCH)utf8.ptr, int(utf8.size), (LPWSTR)buffer.ptr, size_needed);

		auto ptr = (WCHAR*)buffer.ptr;
		ptr[size_needed] = WCHAR(0);
		return buffer;
	}

	inline static File
	_file_stdout()
	{
		constexpr const uint32_t MY_ENABLE_VIRTUAL_TERMINAL_PROCESSING = 4;

		static IFile file{};
		file.winos_handle = GetStdHandle(STD_OUTPUT_HANDLE);

		DWORD mode;
		GetConsoleMode(file.winos_handle, &mode);
		mode |= MY_ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(file.winos_handle, mode);
		return &file;
	}

	inline static File
	_file_stderr()
	{
		static IFile file{};
		file.winos_handle = GetStdHandle(STD_ERROR_HANDLE);
		return &file;
	}

	inline static File
	_file_stdin()
	{
		static IFile file{};
		file.winos_handle = GetStdHandle(STD_INPUT_HANDLE);
		return &file;
	}

	inline static bool
	_is_std_file(void* h)
	{
		return (
			h == _file_stdout()->winos_handle ||
			h == _file_stderr()->winos_handle ||
			h == _file_stdin()->winos_handle
		);
	}


	//API
	void
	IFile::dispose()
	{
		if (winos_handle != INVALID_HANDLE_VALUE &&
			_is_std_file(winos_handle) == false)
		{
			CloseHandle(winos_handle);
		}
		free(this);
	}

	size_t
	IFile::read(Block data)
	{
		DWORD bytes_read = 0;

		auto win_stdin = file_stdin();

		worker_block_ahead();
		if (winos_handle == win_stdin->winos_handle)
		{
			constexpr size_t BUFFER_SIZE = 2048;
			WCHAR static_buffer[BUFFER_SIZE];
			WCHAR* wide_ptr = static_buffer;

			Block dynamic_buffer{};
			if (data.size / 2 >= BUFFER_SIZE)
			{
				dynamic_buffer = alloc(data.size, alignof(WCHAR));
				wide_ptr = (WCHAR*)dynamic_buffer.ptr;
			}
			mn_defer(free(dynamic_buffer));

			size_t wide_read = (data.size / sizeof(WCHAR)) < (BUFFER_SIZE / 2) ? (data.size / sizeof(WCHAR)) : (BUFFER_SIZE / 2);
			DWORD read_chars_count = 0;
			ReadConsoleW(winos_handle, wide_ptr, (DWORD)wide_read, &read_chars_count, NULL);
			bytes_read = WideCharToMultiByte(CP_UTF8, NULL, wide_ptr, read_chars_count, (LPSTR)data.ptr, (int)data.size, NULL, NULL);
		}
		else
		{
			ReadFile(winos_handle, data.ptr, DWORD(data.size), &bytes_read, NULL);
		}
		worker_block_clear();

		return bytes_read;
	}

	size_t
	IFile::write(Block data)
	{
		DWORD bytes_written = 0;

		auto win_stdout = file_stdout();
		auto win_stderr = file_stderr();

		worker_block_ahead();
		if (winos_handle == win_stdout->winos_handle ||
			winos_handle == win_stderr->winos_handle)
		{
			// check whether it's a console or a redirected stuff
			DWORD mode;
			if(GetConsoleMode(winos_handle, &mode) == 0)
			{
				// in case it's redirected then write to buffer
				WriteFile(winos_handle, data.ptr, DWORD(data.size), &bytes_written, NULL);
			}
			else
			{
				auto os_str = _to_os_encoding(data, allocator_top());
				mn_defer(free(os_str));
				WriteConsoleW(winos_handle, os_str.ptr, (DWORD)(os_str.size / sizeof(WCHAR)), &bytes_written, NULL);
			}
		}
		else
		{
			WriteFile(winos_handle, data.ptr, DWORD(data.size), &bytes_written, NULL);
		}
		worker_block_clear();

		return bytes_written;
	}

	int64_t
	IFile::size()
	{
		LARGE_INTEGER size;
		if(GetFileSizeEx(winos_handle, &size))
		{
			return *(int64_t*)(&size);
		}
		return -1;
	}

	//helpers
	Block
	to_os_encoding(const Str& utf8, Allocator allocator)
	{
		return _to_os_encoding(block_from(utf8), allocator);
	}

	Block
	to_os_encoding(const char* utf8, Allocator allocator)
	{
		return to_os_encoding(str_lit(utf8), allocator);
	}

	Str
	from_os_encoding(Block os_str, Allocator allocator)
	{
		return _from_os_encoding(os_str, allocator);
	}


	//std files
	File
	file_stdout()
	{
		static File _stdout = _file_stdout();
		return _stdout;
	}

	File
	file_stderr()
	{
		static File _stderr = _file_stderr();
		return _stderr;
	}

	File
	file_stdin()
	{
		static File _stdin = _file_stdin();
		return _stdin;
	}


	//files
	File
	file_open(const char* filename, IO_MODE io_mode, OPEN_MODE open_mode, SHARE_MODE share_mode)
	{
		//translate the io mode
		DWORD desired_access;
		switch(io_mode)
		{
			case IO_MODE::READ:
				desired_access = GENERIC_READ;
				break;

			case IO_MODE::WRITE:
				desired_access = GENERIC_WRITE;
				break;

			case IO_MODE::READ_WRITE:
			default:
				desired_access = GENERIC_READ | GENERIC_WRITE;
				break;
		}

		//translate the open mode
		DWORD creation_disposition;
		switch(open_mode)
		{
			case OPEN_MODE::CREATE_ONLY:
				creation_disposition = CREATE_NEW;
				break;

			case OPEN_MODE::OPEN_OVERWRITE:
				creation_disposition = TRUNCATE_EXISTING;
				break;

			case OPEN_MODE::OPEN_ONLY:
			case OPEN_MODE::OPEN_APPEND:
				creation_disposition = OPEN_EXISTING;
				break;

			case OPEN_MODE::CREATE_APPEND:
				creation_disposition = OPEN_ALWAYS;
				break;

			case OPEN_MODE::CREATE_OVERWRITE:
			default:
				creation_disposition = CREATE_ALWAYS;
				break;
		}

		DWORD sharing_disposition;
		switch (share_mode)
		{
		case SHARE_MODE_READ:
			sharing_disposition = FILE_SHARE_READ;
			break;
		case SHARE_MODE_WRITE:
			sharing_disposition = FILE_SHARE_WRITE;
			break;
		case SHARE_MODE_DELETE:
			sharing_disposition = FILE_SHARE_DELETE;
			break;
		case SHARE_MODE_READ_WRITE:
			sharing_disposition = FILE_SHARE_READ | FILE_SHARE_WRITE;
			break;
		case SHARE_MODE_READ_DELETE:
			sharing_disposition = FILE_SHARE_READ | FILE_SHARE_DELETE;
			break;
		case SHARE_MODE_WRITE_DELETE:
			sharing_disposition = FILE_SHARE_WRITE | FILE_SHARE_DELETE;
			break;
		case SHARE_MODE_ALL:
			sharing_disposition = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
			break;
		case SHARE_MODE_NONE:
		default:
			sharing_disposition = NULL;
			break;
		}

		Block os_str = to_os_encoding(filename, allocator_top());
		mn_defer(mn::free(os_str));

		LPWSTR win_filename = (LPWSTR)os_str.ptr;
		HANDLE windows_handle = CreateFile(
			win_filename,
			desired_access,
			sharing_disposition,
			NULL,
			creation_disposition,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

		if(windows_handle == INVALID_HANDLE_VALUE)
			return nullptr;

		if(open_mode == OPEN_MODE::CREATE_APPEND ||
		   open_mode == OPEN_MODE::OPEN_APPEND)
		{
			SetFilePointer (windows_handle,	//file handle
							NULL,					//distance to move low part
				 			NULL,					//ditance to mvoe high part
				 			FILE_END); 				//movement point of reference
		}

		File self = alloc_construct<IFile>();
		self->winos_handle = windows_handle;
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
		return self->winos_handle != INVALID_HANDLE_VALUE;
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
		LARGE_INTEGER size;
		if(GetFileSizeEx(self->winos_handle, &size))
		{
			return *(int64_t*)(&size);
		}
		return -1;
	}

	int64_t
	file_cursor_pos(File self)
	{
		LARGE_INTEGER position, offset;
		offset.QuadPart = 0;
		if(SetFilePointerEx(self->winos_handle, offset, &position, FILE_CURRENT))
		{
			return *(int64_t*)(&position);
		}
		return -1;
	}

	bool
	file_cursor_move(File self, int64_t offset)
	{
		LARGE_INTEGER position, win_offset;
		win_offset.QuadPart = offset;
		return SetFilePointerEx(self->winos_handle, win_offset, &position, FILE_CURRENT);
	}

	bool
	file_cursor_set(File self, int64_t absolute)
	{
		LARGE_INTEGER position, win_offset;
		win_offset.QuadPart = absolute;
		return SetFilePointerEx(self->winos_handle, win_offset, &position, FILE_BEGIN);
	}

	bool
	file_cursor_move_to_start(File self)
	{
		LARGE_INTEGER position, offset;
		offset.QuadPart = 0;
		return SetFilePointerEx(self->winos_handle, offset, &position, FILE_BEGIN);
	}

	bool
	file_cursor_move_to_end(File self)
	{
		LARGE_INTEGER position, offset;
		offset.QuadPart = 0;
		return SetFilePointerEx(self->winos_handle, offset, &position, FILE_END);
	}

	bool
	file_write_try_lock(File self, int64_t offset, int64_t size)
	{
		assert(offset >= 0 && size >= 0);

		DWORD offset_low  = (DWORD)(offset & (0x00000000FFFFFFFF));
		DWORD offset_high = (DWORD)(offset & (0xFFFFFFFF00000000));
		DWORD size_low  = (DWORD)(size & (0x00000000FFFFFFFF));
		DWORD size_high = (DWORD)(size & (0xFFFFFFFF00000000));

		OVERLAPPED ov{};
		ov.Offset = offset_low;
		ov.OffsetHigh = offset_high;

		return LockFileEx(self->winos_handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, size_low, size_high, &ov);
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

		DWORD offset_low  = (DWORD)(offset & (0x00000000FFFFFFFF));
		DWORD offset_high = (DWORD)(offset & (0xFFFFFFFF00000000));
		DWORD size_low  = (DWORD)(size & (0x00000000FFFFFFFF));
		DWORD size_high = (DWORD)(size & (0xFFFFFFFF00000000));

		OVERLAPPED ov{};
		ov.Offset = offset_low;
		ov.OffsetHigh = offset_high;

		return UnlockFileEx(self->winos_handle, 0, size_low, size_high, &ov);
	}

	bool
	file_read_try_lock(File self, int64_t offset, int64_t size)
	{
		assert(offset >= 0 && size >= 0);

		DWORD offset_low  = (DWORD)(offset & (0x00000000FFFFFFFF));
		DWORD offset_high = (DWORD)(offset & (0xFFFFFFFF00000000));
		DWORD size_low  = (DWORD)(size & (0x00000000FFFFFFFF));
		DWORD size_high = (DWORD)(size & (0xFFFFFFFF00000000));

		OVERLAPPED ov{};
		ov.Offset = offset_low;
		ov.OffsetHigh = offset_high;

		return LockFileEx(self->winos_handle, LOCKFILE_FAIL_IMMEDIATELY, 0, size_low, size_high, &ov);
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

		DWORD offset_low  = (DWORD)(offset & (0x00000000FFFFFFFF));
		DWORD offset_high = (DWORD)(offset & (0xFFFFFFFF00000000));
		DWORD size_low  = (DWORD)(size & (0x00000000FFFFFFFF));
		DWORD size_high = (DWORD)(size & (0xFFFFFFFF00000000));

		OVERLAPPED ov{};
		ov.Offset = offset_low;
		ov.OffsetHigh = offset_high;

		return UnlockFileEx(self->winos_handle, 0, size_low, size_high, &ov);
	}
}