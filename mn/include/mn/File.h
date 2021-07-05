#pragma once

#include "mn/Exports.h"
#include "mn/Stream.h"
#include "mn/Str.h"

namespace mn
{
	// a file handle
	typedef struct IFile* File;

	// some forward declarations because this language require this kind of thing

	MN_EXPORT int64_t
	file_cursor_pos(File handle);

	MN_EXPORT bool
	file_cursor_move(File handle, int64_t offset);

	MN_EXPORT bool
	file_cursor_set(File handle, int64_t absolute);

	MN_EXPORT bool
	file_cursor_move_to_start(File handle);

	MN_EXPORT bool
	file_cursor_move_to_end(File handle);

	struct IFile final: IStream
	{
		union
		{
			void* winos_handle;
			int linux_handle;
			int macos_handle;
		};

		MN_EXPORT virtual void
		dispose() override;

		MN_EXPORT virtual size_t
		read(Block data) override;

		MN_EXPORT virtual size_t
		write(Block data) override;

		MN_EXPORT virtual int64_t
		size() override;

		virtual int64_t
		cursor_operation(STREAM_CURSOR_OP op, int64_t arg) override
		{
			switch (op)
			{
			case STREAM_CURSOR_GET:
				return file_cursor_pos(this);
			case STREAM_CURSOR_MOVE:
				file_cursor_move(this, arg);
				return file_cursor_pos(this);
			case STREAM_CURSOR_SET:
				file_cursor_set(this, arg);
				return file_cursor_pos(this);
			case STREAM_CURSOR_START:
				file_cursor_move_to_start(this);
				return 0;
			case STREAM_CURSOR_END:
				file_cursor_move_to_end(this);
				return file_cursor_pos(this);
			default:
				assert(false && "unreachable");
				return STREAM_CURSOR_ERROR;
			}
		}
	};


	// converts a string from utf-8 to os specific encoding it will use tmp allocator by default
	// you probably won't need to use this function because mn takes care of utf-8 conversion internally
	// it might be of use if you're writing os specific code outside of mn
	MN_EXPORT Block
	to_os_encoding(const Str& utf8, Allocator allocator = memory::tmp());

	// converts a string from utf-8 to os specific encoding it will use tmp allocator by default
	// you probably won't need to use this function because mn takes care of utf-8 conversion internally
	// it might be of use if you're writing os specific code outside of mn
	MN_EXPORT Block
	to_os_encoding(const char* utf8, Allocator allocator = memory::tmp());

	// converts a string from os specific encoding to utf-8 it will use tmp allocator by default
	// you probably won't need to use this function because mn takes care of utf-8 conversion internally
	// it might be of use if you're writing os specific code outside of mn
	MN_EXPORT Str
	from_os_encoding(Block os_str, Allocator allocator = memory::tmp());

	// file open mode options
	enum OPEN_MODE
	{
		// creates the file if it doesn't exist, if it exists the function will fail
		OPEN_MODE_CREATE_ONLY,
		// creates the file if it doesn't exist, if it exists it will be overwritten
		OPEN_MODE_CREATE_OVERWRITE,
		// creates the file if it doesn't exist, if it exists it will be append to its end
		OPEN_MODE_CREATE_APPEND,
		// opens the file if it exists, fails otherwise
		OPEN_MODE_OPEN_ONLY,
		// opens the file if it exists and its content will be overwritten, if it doesn't exist the function will fail
		OPEN_MODE_OPEN_OVERWRITE,
		// opens the file if it exists and new writes will append to the file, if it doesn't exist the function will fail
		OPEN_MODE_OPEN_APPEND
	};

	// file share mode options
	enum SHARE_MODE
	{
		// enables subsequent open operations on a file or device to request read access
		SHARE_MODE_READ,
		// enables subsequent open operations on a file or device to request write access
		SHARE_MODE_WRITE,
		// enables subsequent open operations on a file or device to request delete access
		SHARE_MODE_DELETE,
		// combines the read write options from above
		SHARE_MODE_READ_WRITE,
		// combines the read delete options from above
		SHARE_MODE_READ_DELETE,
		// combines the write delete options from above
		SHARE_MODE_WRITE_DELETE,
		// combines all the options
		SHARE_MODE_ALL,
		// no sharing is allowed so any subsequent opens will fail
		SHARE_MODE_NONE,
	};

	// file io mode options
	enum IO_MODE
	{
		// only read operations are allowed
		IO_MODE_READ,
		// only write operations are allowed
		IO_MODE_WRITE,
		// read and write operations are allowed
		IO_MODE_READ_WRITE
	};

	// returns a file handle pointing to the standard output
	MN_EXPORT File
	file_stdout();

	// returns a file handle pointing to the standard error
	MN_EXPORT File
	file_stderr();

	// returns a file handle pointing to the standard input
	MN_EXPORT File
	file_stdin();

	// opens a file, if it fails it will return null handle
	MN_EXPORT File
	file_open(const char* filename, IO_MODE io_mode, OPEN_MODE open_mode, SHARE_MODE share_mode = SHARE_MODE_ALL);

	// opens a file, if it fails it will return null handle
	inline static File
	file_open(const Str& filename, IO_MODE io_mode, OPEN_MODE open_mode, SHARE_MODE share_mode = SHARE_MODE_ALL)
	{
		return file_open(filename.ptr, io_mode, open_mode, share_mode);
	}

	// closes an open file handle
	MN_EXPORT void
	file_close(File handle);

	// checks if the given file handle is valid
	MN_EXPORT bool
	file_valid(File handle);

	// writes the given block of bytes to the given file, and returns the written amount of bytes
	MN_EXPORT size_t
	file_write(File handle, Block data);

	// reads from the file into the given block of bytes, and returns the read amount of bytes
	MN_EXPORT size_t
	file_read(File handle, Block data);

	// returns the size of the file in bytes
	MN_EXPORT int64_t
	file_size(File handle);

	// returns the cursor position of the given file
	MN_EXPORT int64_t
	file_cursor_pos(File handle);

	// moves the file cursor by the given offset, and returns whether it succeeded
	MN_EXPORT bool
	file_cursor_move(File handle, int64_t offset);

	// sets the file cursor to the given absolute position, and returns whether it succeeded
	MN_EXPORT bool
	file_cursor_set(File handle, int64_t absolute);

	// moves the file cursor to the start of the file, and returns whether it succeeded
	MN_EXPORT bool
	file_cursor_move_to_start(File handle);

	// sets the file cursor to the end of the file, and returns whether it succeeded
	MN_EXPORT bool
	file_cursor_move_to_end(File handle);

	// locks the specified region of the file, locks can't overlap otherwise the locking operation will fail
	// you can lock a region beyond EOF to coordinate additions to a file
	// returns whether the lock operation has succeeded
	MN_EXPORT bool
	file_write_try_lock(File handle, int64_t offset, int64_t size);

	// locks the specified region of the file, locks can't overlap otherwise the locking operation will fail
	// you can lock a region beyond EOF to coordinate additions to a file
	// this function will block until it can acquire the lock
	MN_EXPORT void
	file_write_lock(File handle, int64_t offset, int64_t size);

	// unlocks the specified region of the file
	MN_EXPORT bool
	file_write_unlock(File handle, int64_t offset, int64_t size);

	// specifies a region of the file to be locked, read locks allow multiple readers and zero writers
	// returns whether the lock operation has succeeded
	MN_EXPORT bool
	file_read_try_lock(File handle, int64_t offset, int64_t size);

	// specifies a region of the file to be locked, read locks allow multiple readers and zero writers
	// this function will block until it can acquire the lock
	MN_EXPORT void
	file_read_lock(File handle, int64_t offset, int64_t size);

	// unlocks the specified region of the file
	MN_EXPORT bool
	file_read_unlock(File handle, int64_t offset, int64_t size);

	// represents a memory mapped file
	struct Mapped_File
	{
		Block data;
	};

	// memory maps a given file region into memory and returns a mapped file structure
	// if size = 0 this will map the file starting from the offset to the end of the file
	// returns a nullptr in case of failure
	MN_EXPORT Mapped_File*
	file_mmap(File file, int64_t offset, int64_t size, IO_MODE io_mode);

	// tries to open a file and maps the specified region into memory
	// if size = 0 this will map the file starting from the offset to the end of the file
	// returns a nullptr in case of failure
	MN_EXPORT Mapped_File*
	file_mmap(const Str& filename, int64_t offset, int64_t size, IO_MODE io_mode, OPEN_MODE open_mode, SHARE_MODE share_mode = SHARE_MODE_ALL);

	// tries to open a file and maps the specified region into memory
	// if size = 0 this will map the file starting from the offset to the end of the file
	// returns a nullptr in case of failure
	inline static Mapped_File*
	file_mmap(const char* filename, int64_t offset, int64_t size, IO_MODE io_mode, OPEN_MODE open_mode, SHARE_MODE share_mode = SHARE_MODE_ALL)
	{
		return file_mmap(str_lit(filename), offset, size, io_mode, open_mode, share_mode);
	}

	// unmaps the given mapped file, and returns whether the unmap was successful
	MN_EXPORT bool
	file_unmap(Mapped_File* self);

	// destruct overload for mapped file
	inline static void
	destruct(Mapped_File* self)
	{
		file_unmap(self);
	}
}