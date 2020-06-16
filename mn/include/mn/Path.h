#pragma once

#include "mn/Exports.h"
#include "mn/Base.h"
#include "mn/Str.h"

namespace mn
{
	/**
	* @brief      A Helper function which given the filename will load the content of it
	* into the resulting string
	*
	* @param[in]  filename   The filename
	* @param[in]  allocator  The allocator to be used by the resulting string
	*
	* @return     A String containing the content of the file
	*/
	MN_EXPORT Str
	file_content_str(const char* filename, Allocator allocator = allocator_top());

	inline static Str
	file_content_str(const Str& filename, Allocator allocator = allocator_top())
	{
		return file_content_str(filename.ptr, allocator);
	}


	//File System api
	/**
	 * @brief      Converts from standard encoding(linux-like) to os-specific encoding
	 *
	 * @param[in]  path  The path
	 *
	 * @return     A String containing the encoded path
	 */
	MN_EXPORT Str
	path_os_encoding(const char* path, Allocator allocator = memory::tmp());

	/**
	 * @brief      Converts from standard encoding(linux-like) to os-specific encoding
	 *
	 * @param[in]  path  The path
	 *
	 * @return     A String containing the encoded path
	 */
	inline static Str
	path_os_encoding(const Str& path, Allocator allocator = memory::tmp())
	{
		return path_os_encoding(path.ptr, allocator);
	}

	/**
	 * Removes duplicate / and other sanitization stuff
	 */
	MN_EXPORT Str
	path_sanitize(Str path);

	/**
	 * Converts from the os-specific encoding to standard encoding(linux-like)
	 */
	MN_EXPORT Str
	path_normalize(Str path);

	inline static Str
	path_join(Str base)
	{
		return path_sanitize(base);
	}

	/**
	 * @brief      Joins multiple folders in the path
	 * base = "base_folder"
	 * base = path_join(base, "my_folder1", "my_folder2", "my_file") ->
	 * base_folder/my_folder1/my_folder2/my_file
	 */
	template<typename TFirst, typename ... TArgs>
	inline static Str
	path_join(Str base, TFirst&& first, TArgs&& ... args)
	{
		str_push(base, std::forward<TFirst>(first));
		str_push(base, "/");
		return path_join(base, std::forward<TArgs>(args)...);
	}

	/**
	 * @brief      Returns whether the path exists or not
	 */
	MN_EXPORT bool
	path_exists(const char* path);

	/**
	 * @brief      Returns whether the path exists or not
	 */
	inline static bool
	path_exists(const Str& path)
	{
		return path_exists(path.ptr);
	}

	/**
	 * @brief      Returns whether the given path is a folder or not
	 */
	MN_EXPORT bool
	path_is_folder(const char* path);

	/**
	 * @brief      Returns whether the given path is a folder or not
	 */
	inline static bool
	path_is_folder(const Str& path)
	{
		return path_is_folder(path.ptr);
	}

	/**
	 * @brief      Returns whether the given path is a file or not
	 */
	MN_EXPORT bool
	path_is_file(const char* path);

	/**
	 * @brief      Returns whether the given path is a file or not
	 */
	inline static bool
	path_is_file(const Str& path)
	{
		return path_is_file(path.ptr);
	}

	/**
	 * @brief      Returns the current path of the process
	 *
	 * @param[in]  allocator  The allocator to be used in the returned string
	 */
	MN_EXPORT Str
	path_current(Allocator allocator = allocator_top());

	/**
	 * @brief      Changes the current path of the process
	 *
	 * @param[in]  path  The path
	 */
	MN_EXPORT void
	path_current_change(const char* path);

	/**
	 * @brief      Changes the current path of the process
	 *
	 * @param[in]  path  The path
	 */
	inline static void
	path_current_change(const Str& path)
	{
		path_current_change(path.ptr);
	}

	/**
	 * @brief      Returns the absolute path of the given relative path
	 *
	 * @param[in]  path       The relative path
	 * @param[in]  allocator  The allocator to be used in the returns string
	 */
	MN_EXPORT Str
	path_absolute(const char* path, Allocator allocator = allocator_top());

	/**
	 * @brief      Returns the absolute path of the given relative path
	 *
	 * @param[in]  path       The relative path
	 * @param[in]  allocator  The allocator to be used in the returns string
	 */
	inline static Str
	path_absolute(const Str& path, Allocator allocator = allocator_top())
	{
		return path_absolute(path.ptr, allocator);
	}

	MN_EXPORT Str
	file_directory(const char* path, Allocator allocator = allocator_top());

	inline static Str
	file_directory(const Str& path, Allocator allocator = allocator_top())
	{
		return file_directory(path.ptr, allocator);
	}

	struct Path_Entry
	{
		enum KIND
		{
			KIND_FILE,
			KIND_FOLDER
		};

		KIND kind;
		Str  name;
	};

	inline static void
	path_entry_free(Path_Entry& self)
	{
		str_free(self.name);
	}

	inline static void
	destruct(Path_Entry& self)
	{
		path_entry_free(self);
	}

	/**
	 * @brief      Returns the names of the children files/folders of the given path
	 *
	 * @param[in]  path       The path
	 * @param[in]  allocator  The allocator to be used in the returned buf
	 */
	MN_EXPORT Buf<Path_Entry>
	path_entries(const char* path, Allocator allocator = allocator_top());

	/**
	 * @brief      Returns the names of the children files/folders of the given path
	 *
	 * @param[in]  path       The path
	 * @param[in]  allocator  The allocator to be used in the returned buf
	 */
	inline static Buf<Path_Entry>
	path_entries(const Str& path, Allocator allocator = allocator_top())
	{
		return path_entries(path.ptr, allocator);
	}

	// file_last_write_time returns the time of the last write to the given file
	// if path is not correct it will return 0
	MN_EXPORT int64_t
	file_last_write_time(const char* path);

	// file_last_write_time returns the time of the last write to the given file
	// if path is not correct it will return 0
	inline static int64_t
	file_last_write_time(const mn::Str& path)
	{
		return file_last_write_time(path.ptr);
	}


	/**
	 * @brief      Copies a file from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	MN_EXPORT bool
	file_copy(const char* src, const char* dst);

	/**
	 * @brief      Copies a file from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	file_copy(const Str& src, const char* dst)
	{
		return file_copy(src.ptr, dst);
	}

	/**
	 * @brief      Copies a file from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	file_copy(const char* src, const Str& dst)
	{
		return file_copy(src, dst.ptr);
	}

	/**
	 * @brief      Copies a file from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	file_copy(const Str& src, const Str& dst)
	{
		return file_copy(src.ptr, dst.ptr);
	}

	/**
	 * @brief      Removes a file
	 *
	 * @param[in]  path  The file path
	 */
	MN_EXPORT bool
	file_remove(const char* path);

	/**
	 * @brief      Removes a file
	 *
	 * @param[in]  path  The file path
	 */
	inline static bool
	file_remove(const Str& path)
	{
		return file_remove(path.ptr);
	}

	/**
	 * @brief      Moves a file from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	MN_EXPORT bool
	file_move(const char* src, const char* dst);

	/**
	 * @brief      Moves a file from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	file_move(const Str& src, const char* dst)
	{
		return file_move(src.ptr, dst);
	}

	/**
	 * @brief      Moves a file from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	file_move(const char* src, const Str& dst)
	{
		return file_move(src, dst.ptr);
	}

	/**
	 * @brief      Moves a file from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	file_move(const Str& src, const Str& dst)
	{
		return file_move(src.ptr, dst.ptr);
	}

	MN_EXPORT Str
	file_tmp(const Str& base, const Str& ext, Allocator allocator = allocator_top());

	inline static Str
	file_tmp(const char* base, const Str& ext, Allocator allocator = allocator_top())
	{
		return file_tmp(str_lit(base), ext, allocator);
	}

	inline static Str
	file_tmp(const Str& base, const char* ext, Allocator allocator = allocator_top())
	{
		return file_tmp(base, str_lit(ext), allocator);
	}

	inline static Str
	file_tmp(const char* base, const char* ext, Allocator allocator = allocator_top())
	{
		return file_tmp(str_lit(base), str_lit(ext), allocator);
	}

	/**
	 * @brief      Creates a new folder
	 *
	 * @param[in]  path  The folder path
	 */
	MN_EXPORT bool
	folder_make(const char* path);

	/**
	 * @brief      Creates a new folder
	 *
	 * @param[in]  path  The folder path
	 */
	inline static bool
	folder_make(const Str& path)
	{
		return folder_make(path.ptr);
	}

	/**
	 * @brief      Removes a folder and the contained files/folders
	 *
	 * @param[in]  path  The folder path
	 */
	MN_EXPORT bool
	folder_remove(const char* path);

	/**
	 * @brief      Removes a folder and the contained files/folders
	 *
	 * @param[in]  path  The folder path
	 */
	inline static bool
	folder_remove(const Str& path)
	{
		return folder_remove(path.ptr);
	}

	/**
	 * @brief      Copies a folder and the contained files/folders from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	MN_EXPORT bool
	folder_copy(const char* src, const char* dst);

	/**
	 * @brief      Copies a folder and the contained files/folders from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	folder_copy(const Str& src, const char* dst)
	{
		return folder_copy(src.ptr, dst);
	}

	/**
	 * @brief      Copies a folder and the contained files/folders from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	folder_copy(const char* src, const Str& dst)
	{
		return folder_copy(src, dst.ptr);
	}

	/**
	 * @brief      Copies a folder and the contained files/folders from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	folder_copy(const Str& src, const Str& dst)
	{
		return folder_copy(src.ptr, dst.ptr);
	}

	/**
	 * @brief      Moves a folder and the contained files/folders from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	folder_move(const char* src, const char* dst)
	{
		if (folder_copy(src, dst))
			return folder_remove(src);
		return false;
	}

	/**
	 * @brief      Moves a folder and the contained files/folders from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	folder_move(const Str& src, const char* dst)
	{
		return folder_move(src.ptr, dst);
	}

	/**
	 * @brief      Moves a folder and the contained files/folders from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	folder_move(const char* src, const Str& dst)
	{
		return folder_move(src, dst.ptr);
	}

	/**
	 * @brief      Moves a folder and the contained files/folders from src to dst
	 *
	 * @param[in]  src   The source
	 * @param[in]  dst   The destination
	 */
	inline static bool
	folder_move(const Str& src, const Str& dst)
	{
		return folder_move(src.ptr, dst.ptr);
	}

	MN_EXPORT Str
	folder_tmp(Allocator allocator = allocator_top());

	/**
	 * @brief      Returns the system config folder
	 *
	 * @param[in]  allocator   The allocator to be used in the returned str
	 */
	MN_EXPORT Str
	folder_config(Allocator allocator = allocator_top());
}
