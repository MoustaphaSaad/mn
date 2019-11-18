#include "mn/Path.h"
#include "mn/File.h"
#include "mn/OS.h"
#include "mn/Scope.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mbstring.h>
#include <tchar.h>

#include "mn/Thread.h"
#include "mn/OS.h"
#include "mn/Defer.h"

#include <chrono>

#include <assert.h>

namespace mn
{
	Str
	file_content_str(const char* filename, Allocator allocator)
	{
		Str str = str_with_allocator(allocator);
		File f = file_open(filename, IO_MODE::READ, OPEN_MODE::OPEN_ONLY);
		if(file_valid(f) == false)
			panic("cannot read file \"{}\"", filename);

		buf_resize(str, file_size(f) + 1);
		--str.count;
		str.ptr[str.count] = '\0';

		[[maybe_unused]] size_t read_size = file_read(f, Block { str.ptr, str.count });
		assert(read_size == str.count);

		file_close(f);
		return str;
	}


	//File System api
	Str
	path_os_encoding(const char* path, Allocator allocator)
	{
		size_t str_len = ::strlen(path);
		Str res = str_with_allocator(allocator);
		buf_reserve(res, str_len + 1);

		for (size_t i = 0; i < str_len; ++i)
		{
			if (path[i] == '/')
				buf_push(res, '\\');
			else
				buf_push(res, path[i]);
		}

		str_null_terminate(res);
		return res;
	}

	Str
	path_sanitize(Str path)
	{
		int32_t prev = '\0';
		char *it_write = path.ptr;
		const char *it_read = path.ptr;
		//skip all the /, \ on front
		while (it_read && *it_read != '\0' && (*it_read == '/' || *it_read == '\\'))
			it_read = rune_next(it_read);

		while (it_read && *it_read != '\0')
		{
			int c = rune_read(it_read);
			if (c == '\\' && prev != '\\')
			{
				*it_write = '/';
			}
			else if (c == '\\' && prev == '\\')
			{
				while (it_read && *it_read != '\0' && *it_read == '\\')
					it_read = rune_next(it_read);
				continue;
			}
			else if (c == '/' && prev == '/')
			{
				while (it_read && *it_read != '\0' && *it_read == '/')
					it_read = rune_next(it_read);
				continue;
			}
			else
			{
				size_t size = rune_size(c);
				char* c_it = (char*)&c;
				for (size_t i = 0; i < size; ++i)
					*it_write = *c_it;
			}
			prev = c;
			it_read = rune_next(it_read);
			it_write = (char*)rune_next(it_write);
		}
		path.count = it_write - path.ptr;
		if (prev == '\\' || prev == '/')
			--path.count;
		str_null_terminate(path);
		return path;
	}

	Str
	path_normalize(Str path)
	{
		for (char& c : path)
		{
			if (c == '\\')
				c = '/';
		}
		return path;
	}

	bool
	path_exists(const char* path)
	{
		mn_scope();

		Block os_path = to_os_encoding(path_os_encoding(path, memory::tmp()), memory::tmp());
		DWORD attributes = GetFileAttributes((LPCWSTR)os_path.ptr);
		return attributes != INVALID_FILE_ATTRIBUTES;
	}

	bool
	path_is_folder(const char* path)
	{
		mn_scope();

		Block os_path = to_os_encoding(path_os_encoding(path, memory::tmp()), memory::tmp());
		DWORD attributes = GetFileAttributes((LPCWSTR)os_path.ptr);
		return (attributes != INVALID_FILE_ATTRIBUTES &&
				attributes &  FILE_ATTRIBUTE_DIRECTORY);
	}

	bool
	path_is_file(const char* path)
	{
		mn_scope();

		Block os_path = to_os_encoding(path_os_encoding(path, memory::tmp()), memory::tmp());
		DWORD attributes = GetFileAttributes((LPCWSTR)os_path.ptr);
		return (attributes != INVALID_FILE_ATTRIBUTES &&
				!(attributes &  FILE_ATTRIBUTE_DIRECTORY));
	}

	Str
	path_current(Allocator allocator)
	{
		DWORD required_size = GetCurrentDirectory(0, NULL);
		Block os_str = alloc(required_size * sizeof(TCHAR), alignof(TCHAR));
		mn_defer(free(os_str));
		[[maybe_unused]] DWORD written_size = GetCurrentDirectory((DWORD)(os_str.size/sizeof(TCHAR)), (LPWSTR)os_str.ptr);
		assert((size_t)(written_size+1) == (os_str.size / sizeof(TCHAR)) && "GetCurrentDirectory Failed");
		Str res = from_os_encoding(os_str, allocator);
		path_normalize(res);
		return res;
	}

	void
	path_current_change(const char* path)
	{
		mn_scope();

		Block os_path = to_os_encoding(path_os_encoding(path, memory::tmp()), memory::tmp());
		[[maybe_unused]] bool result = SetCurrentDirectory((LPCWSTR)os_path.ptr);

		assert(result && "SetCurrentDirectory Failed");
	}

	Str
	path_absolute(const char* path, Allocator allocator)
	{
		mn_scope();

		Block os_path = to_os_encoding(path_os_encoding(path, memory::tmp()), memory::tmp());

		DWORD required_size = GetFullPathName((LPCWSTR)os_path.ptr, 0, NULL, NULL);
		Block full_path = alloc_from(memory::tmp(), required_size * sizeof(TCHAR), alignof(TCHAR));

		[[maybe_unused]] DWORD written_size = GetFullPathName((LPCWSTR)os_path.ptr, required_size, (LPWSTR)full_path.ptr, NULL);
		assert(written_size+1 == required_size && "GetFullPathName failed");
		Str res = from_os_encoding(full_path, allocator);
		return path_normalize(res);
	}

	Str
	file_directory(const char* path, Allocator allocator)
	{
		Str result = str_from_c(path, allocator);
		path_sanitize(result);

		size_t i = 0;
		for(i = 1; i <= result.count; ++i)
		{
			char c = result[result.count - i];
			if(c == '/')
				break;
		}
		if (i > result.count)
			result.count = 0;
		else
			result.count -= i;
		str_null_terminate(result);
		return result;
	}

	Buf<Path_Entry>
	path_entries(const char* path, Allocator allocator)
	{
		mn_scope();

		//add the * at the end
		Str tmp_path = str_with_allocator(memory::tmp());
		buf_reserve(tmp_path, ::strlen(path) + 3);
		str_push(tmp_path, path);
		if (tmp_path.count && tmp_path[tmp_path.count - 1] != '/')
			buf_push(tmp_path, '/');
		buf_push(tmp_path, '*');
		str_null_terminate(tmp_path);

		Buf<Path_Entry> res = buf_with_allocator<Path_Entry>(allocator);
		Block os_path = to_os_encoding(path_os_encoding(tmp_path, memory::tmp()), memory::tmp());
		WIN32_FIND_DATA file_data{};
		HANDLE search_handle = FindFirstFileEx((LPCWSTR)os_path.ptr,
			FindExInfoBasic, &file_data, FindExSearchNameMatch, NULL, FIND_FIRST_EX_CASE_SENSITIVE);
		if (search_handle != INVALID_HANDLE_VALUE)
		{
			while (search_handle != INVALID_HANDLE_VALUE)
			{
				Path_Entry entry{};
				if (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					entry.kind = Path_Entry::KIND_FOLDER;
				else
					entry.kind = Path_Entry::KIND_FILE;
				entry.name = from_os_encoding(Block{
						(void*)file_data.cFileName,
						(_tcsclen(file_data.cFileName) + 1) * sizeof(TCHAR)
					}, allocator);
				path_normalize(entry.name);
				buf_push(res, entry);
				if (FindNextFile(search_handle, &file_data) == false)
					break;
			}
			[[maybe_unused]] bool result = FindClose(search_handle);
			assert(result && "FindClose failed");
		}
		return res;
	}

	//Tip 
	//Starting with Windows 10, version 1607, for the unicode version of this function (MoveFileW),
	//you can opt-in to remove the MAX_PATH limitation without prepending "\\?\". See the
	//"Maximum Path Length Limitation" section of Naming Files, Paths, and Namespaces for details.

	bool
	file_copy(const char* src, const char* dst)
	{
		mn_scope();

		Block os_src = to_os_encoding(path_os_encoding(src, memory::tmp()), memory::tmp());
		Block os_dst = to_os_encoding(path_os_encoding(dst, memory::tmp()), memory::tmp());
		return CopyFile((LPCWSTR)os_src.ptr, (LPCWSTR)os_dst.ptr, TRUE);
	}

	bool
	file_remove(const char* path)
	{
		mn_scope();

		Block os_path = to_os_encoding(path_os_encoding(path, memory::tmp()), memory::tmp());
		return DeleteFile((LPCWSTR)os_path.ptr);
	}

	bool
	file_move(const char* src, const char* dst)
	{
		mn_scope();

		Block os_src = to_os_encoding(path_os_encoding(src, memory::tmp()), memory::tmp());
		Block os_dst = to_os_encoding(path_os_encoding(dst, memory::tmp()), memory::tmp());
		return MoveFile((LPCWSTR)os_src.ptr, (LPCWSTR)os_dst.ptr);
	}

	Str
	file_tmp(const Str& base, const Str& ext, Allocator allocator)
	{
		mn_scope();

		Str _base;
		if (base.count != 0)
			_base = path_normalize(str_clone(base, memory::tmp()));
		else
			_base = folder_tmp(memory::tmp());

		Str res = str_clone(_base, allocator);
		while (true)
		{
			str_clear(res);

			auto duration_nanos = std::chrono::high_resolution_clock::now().time_since_epoch();
			uint64_t nanos = std::chrono::duration_cast<std::chrono::duration<uint64_t, std::nano>>(duration_nanos).count();
			if (ext.count != 0)
				res = path_join(res, str_tmpf("mn_file_tmp_{}.{}", nanos, ext));
			else
				res = path_join(res, str_tmpf("mn_file_tmp_{}", nanos));

			if (path_exists(res) == false)
				break;
		}
		return res;
	}

	bool
	folder_make(const char* path)
	{
		mn_scope();

		Block os_path = to_os_encoding(path_os_encoding(path, memory::tmp()), memory::tmp());
		DWORD attributes = GetFileAttributes((LPCWSTR)os_path.ptr);
		if (attributes != INVALID_FILE_ATTRIBUTES)
			return attributes & FILE_ATTRIBUTE_DIRECTORY;
		return CreateDirectory((LPCWSTR)os_path.ptr, NULL);
	}

	bool
	folder_remove(const char* path)
	{
		mn_scope();

		Block os_path = to_os_encoding(path_os_encoding(path, memory::tmp()), memory::tmp());
		DWORD attributes = GetFileAttributes((LPCWSTR)os_path.ptr);
		if (attributes == INVALID_FILE_ATTRIBUTES)
			return true;

		Buf<Path_Entry> files = path_entries(path, memory::tmp());
		Str tmp_path = str_with_allocator(memory::tmp());
		for (size_t i = 2; i < files.count; ++i)
		{
			str_clear(tmp_path);
			if (files[i].kind == Path_Entry::KIND_FILE)
			{
				tmp_path = path_join(tmp_path, path, files[i].name);
				if (file_remove(tmp_path) == false)
					return false;
			}
			else if (files[i].kind == Path_Entry::KIND_FOLDER)
			{
				tmp_path = path_join(tmp_path, path, files[i].name);
				if (folder_remove(tmp_path) == false)
					return false;
			}
			else
			{
				assert(false && "UNREACHABLE");
				return false;
			}
		}

		return RemoveDirectory((LPCWSTR)os_path.ptr);
	}

	bool
	folder_copy(const char* src, const char* dst)
	{
		mn_scope();

		Buf<Path_Entry> files = path_entries(src, memory::tmp());

		//create the folder no matter what
		if (folder_make(dst) == false)
			return false;

		//if the source folder is empty then exit with success
		if (files.count <= 2)
			return true;

		size_t i = 0;
		Str tmp_src = str_with_allocator(memory::tmp());
		Str tmp_dst = str_with_allocator(memory::tmp());
		for (i = 0; i < files.count; ++i)
		{
			if(files[i].name != "." && files[i].name != "..")
			{
				str_clear(tmp_src);
				str_clear(tmp_dst);

				if (files[i].kind == Path_Entry::KIND_FILE)
				{
					tmp_src = path_join(tmp_src, src, files[i].name);
					tmp_dst = path_join(tmp_dst, dst, files[i].name);
					if (file_copy(tmp_src, tmp_dst) == false)
						break;
				}
				else if (files[i].kind == Path_Entry::KIND_FOLDER)
				{
					tmp_src = path_join(tmp_src, src, files[i].name);
					tmp_dst = path_join(tmp_dst, dst, files[i].name);
					if (folder_copy(tmp_src, tmp_dst) == false)
						break;
				}
				else
				{
					assert(false && "UNREACHABLE");
					break;
				}
			}
		}

		return i == files.count;
	}

	Str
	folder_tmp(Allocator allocator)
	{
		mn_scope();

		DWORD len = GetTempPath(0, NULL);
		assert(len != 0);

		Block os_path = alloc_from(memory::tmp(), len*sizeof(TCHAR)+1, alignof(TCHAR));
		GetTempPath(len, (TCHAR*)os_path.ptr);
		return path_normalize(from_os_encoding(os_path, allocator));
	}
}
