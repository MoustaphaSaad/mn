#include "mn/Path.h"
#include "mn/OS.h"
#include "mn/IO.h"
#include "mn/Defer.h"

#define _LARGEFILE64_SOURCE 1
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <linux/limits.h>
#include <libgen.h>

#include <assert.h>

#include <chrono>

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

	Str
	path_os_encoding(const char* path, Allocator allocator)
	{
		return str_from_c(path, allocator);
	}

	Str
	path_sanitize(Str path)
	{
		char prev = '\0';
		char *it_write = path.ptr;
		const char *it_read = path.ptr;

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
		for(char& c: path)
		{
			if(c == '\\')
				c = '/';
		}
		return path;
	}

	bool
	path_exists(const char* path)
	{
		struct stat sb{};
		return ::stat(path, &sb) == 0;
	}

	bool
	path_is_folder(const char* path)
	{
		struct stat sb{};
		if(::stat(path, &sb) == 0)
			return S_ISDIR(sb.st_mode);
		return false;
	}

	bool
	path_is_file(const char* path)
	{
		struct stat sb{};
		if(::stat(path, &sb) == 0)
			return S_ISREG(sb.st_mode);
		return false;
	}

	Str
	path_current(Allocator allocator)
	{
		char cwd[PATH_MAX] = {0};
		char* res = ::getcwd(cwd, PATH_MAX);
		return str_from_c(res, allocator);
	}

	void
	path_current_change(const char* path)
	{
		[[maybe_unused]] int result = ::chdir(path);
		assert(result == 0 && "chdir failed");
	}

	Str
	path_absolute(const char* path, Allocator allocator)
	{
		char absolute[PATH_MAX] = {0};
		char* res = ::realpath(path, absolute);
		if(res)
			return str_from_c(res, allocator);
		//here we fallback to windows-like interface and just concatencate cwd with path
		Str cwd = path_current(allocator);
		cwd = strf(cwd, "/{}", path);
		return cwd;
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
		Buf<Path_Entry> res = buf_with_allocator<Path_Entry>(allocator);

		DIR *d;
		struct dirent *dir = nullptr;
		d = ::opendir(path);
		if(d)
		{
			while((dir = ::readdir(d)) != NULL)
			{
				Path_Entry entry{};
				if(dir->d_type == DT_DIR)
				{
					entry.kind = Path_Entry::KIND_FOLDER;
				}
				else if(dir->d_type == DT_REG)
				{
					entry.kind = Path_Entry::KIND_FILE;
				}
				else
				{
					assert(false && "UNREACHABLE");
				}
				entry.name = str_from_c(dir->d_name, allocator);
				buf_push(res, entry);
			}
			::closedir(d);
		}
		return res;
	}

	Str
	path_executable(Allocator allocator)
	{
		char path[PATH_MAX + 1];
		::memset(path, 0, sizeof(path));

		char absolute_path[PATH_MAX + 1];
		::memset(absolute_path, 0, sizeof(absolute_path));

		[[maybe_unused]] auto dir_length = readlink("/proc/self/exe", path, sizeof(path));
		assert(dir_length > -1 && dir_length < (ssize_t)sizeof(path));

		[[maybe_unused]] auto realpath_res = realpath(path, absolute_path);
		assert(realpath_res == absolute_path);

		return mn::str_from_c(absolute_path, allocator);
	}

	int64_t
	file_last_write_time(const char* path)
	{
		struct stat sb{};
		if(::stat(path, &sb) != 0)
			return 0;
		return int64_t(sb.st_mtime);
	}

	bool
	file_copy(const char* src, const char* dst)
	{
		char buf[4096];
		ssize_t nread = -1;
		int fd_src = ::open(src, O_RDONLY);
		if(fd_src < 0)
			return false;

		int fd_dst = ::open(dst, O_WRONLY | O_CREAT | O_EXCL, 0666);
		if(fd_dst < 0)
			goto FAILURE;

		while(nread = ::read(fd_src, buf, sizeof(buf)), nread > 0)
		{
			char *out_ptr = buf;
			ssize_t nwritten = 0;
			do
			{
				nwritten = ::write(fd_dst, out_ptr, nread);
				if(nwritten >= 0)
				{
					nread -= nwritten;
					out_ptr += nwritten;
				}
				else if(errno != EINTR)
				{
					goto FAILURE;
				}
			} while(nread > 0);
		}

	FAILURE:
		::close(fd_src);
		if(fd_dst >= 0)
			::close(fd_dst);
		return nread == 0;
	}

	bool
	file_remove(const char* path)
	{
		return ::unlink(path) == 0;
	}

	bool
	file_move(const char* src, const char* dst)
	{
		return ::rename(src, dst) == 0;
	}

	Str
	file_name(const Str& path, Allocator allocator)
	{
		Str path_copy = str_clone(path);
		mn_defer(str_free(path_copy));

		char* filename = ::basename(path_copy.ptr);
		return str_from_c(filename, allocator);
	}

	Str
	file_tmp(const Str &base, const Str &ext, Allocator allocator)
	{
		Str _base;
		if (base.count != 0)
			_base = path_normalize(str_clone(base));
		else
			_base = folder_tmp();
		mn_defer(str_free(_base));

		Str res = str_clone(_base, allocator);
		while (true)
		{
			str_resize(res, _base.count);

			auto duration_nanos = std::chrono::high_resolution_clock::now().time_since_epoch();
			uint64_t nanos =
				std::chrono::duration_cast<std::chrono::duration<uint64_t, std::nano>>(duration_nanos).count();
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
		return ::mkdir(path, 0777) == 0;
	}

	bool
	folder_remove(const char* path)
	{
		Buf<Path_Entry> files = path_entries(path);
		mn_defer(destruct(files));

		auto tmp_path = str_new();
		mn_defer(str_free(tmp_path));

		for(size_t i = 2; i < files.count; ++i)
		{
			str_clear(tmp_path);
			if(files[i].kind == Path_Entry::KIND_FILE)
			{
				tmp_path = path_join(tmp_path, path, files[i].name);
				if(file_remove(tmp_path) == false)
					return false;
			}
			else if(files[i].kind == Path_Entry::KIND_FOLDER)
			{
				tmp_path = path_join(tmp_path, path, files[i].name);
				if(folder_remove(tmp_path) == false)
					return false;
			}
			else
			{
				assert(false && "UNREACHABLE");
				return false;
			}
		}

		return ::rmdir(path) == 0;
	}

	bool
	folder_copy(const char* src, const char* dst)
	{
		Buf<Path_Entry> files = path_entries(src);
		mn_defer(destruct(files));

		//create the folder no matter what
		if(folder_make(dst) == false)
			return false;

		//if the source folder is empty then exit with success
		if(files.count <= 2)
			return true;

		size_t i = 0;
		auto tmp_src = str_new();
		mn_defer(str_free(tmp_src));

		auto tmp_dst = str_new();
		mn_defer(str_free(tmp_dst));

		for(i = 0; i < files.count; ++i)
		{
			if(files[i].name != "." && files[i].name != "..")
			{
				str_clear(tmp_src);
				str_clear(tmp_dst);
				if(files[i].kind == Path_Entry::KIND_FILE)
				{
					tmp_src = path_join(tmp_src, src, files[i].name);
					tmp_dst = path_join(tmp_dst, dst, files[i].name);
					if(file_copy(tmp_src, tmp_dst) == false)
						break;
				}
				else if(files[i].kind == Path_Entry::KIND_FOLDER)
				{
					tmp_src = path_join(tmp_src, src, files[i].name);
					tmp_dst = path_join(tmp_dst, dst, files[i].name);
					if(folder_copy(tmp_src, tmp_dst) == false)
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
		if (auto p = secure_getenv("TMPDIR"))
			return str_from_c(p, allocator);
		else if (auto p = secure_getenv("TMP"))
			return str_from_c(p, allocator);
		else if (auto p = secure_getenv("TEMP"))
			return str_from_c(p, allocator);
		else if (auto p = secure_getenv("TEMPDIR"))
			return str_from_c(p, allocator);
		else
			return str_from_c("/tmp", allocator);
	}

	Str
	folder_config(Allocator allocator)
	{
		char * os_str = secure_getenv("XDG_CONFIG_HOME");
		if (os_str && ::strlen(os_str) > 0)
			return str_from_c(os_str, allocator);

		os_str = secure_getenv("HOME");
		if (os_str && ::strlen(os_str) > 0)
			return path_join(mn::str_with_allocator(allocator), os_str, ".config");

		return str_from_c("~/.config", allocator);
	}
}
