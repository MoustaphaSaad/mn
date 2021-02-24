#include "mn/RAD.h"
#include "mn/Buf.h"
#include "mn/Str.h"
#include "mn/Library.h"
#include "mn/Defer.h"
#include "mn/Thread.h"
#include "mn/Path.h"
#include "mn/IO.h"
#include "mn/Log.h"
#include "mn/UUID.h"

#include <chrono>

RAD* rad_global = nullptr;

typedef void* Load_Func(void*, bool);

struct RAD_Module
{
	mn::Str original_file;
	mn::Str loaded_file;
	mn::Str name;
	mn::Library library;
	int64_t last_write;
	void* api;
	int load_counter;
};

inline static void
destruct(RAD_Module& self)
{
	mn::library_close(self.library);
	mn::file_remove(self.loaded_file);

	mn::str_free(self.original_file);
	mn::str_free(self.loaded_file);
	mn::str_free(self.name);
}

struct RAD
{
	mn::Mutex mtx;
	mn::Map<mn::Str, RAD_Module> modules;
	mn::UUID uuid;
	RAD_Settings settings;
};

// API
RAD*
rad_new(RAD_Settings settings)
{
	mn::allocator_push(mn::memory::clib());
	mn_defer(mn::allocator_pop());

	auto self = mn::alloc<RAD>();
	self->mtx = mn::mutex_new("Root Mutex");
	self->modules = mn::map_new<mn::Str, RAD_Module>();
	self->uuid = mn::uuid_generate();
	self->settings = settings;
	return self;
}

void
rad_free(RAD* self)
{
	mn::allocator_push(mn::memory::clib());
	mn_defer(mn::allocator_pop());

	for (const auto &module : self->modules)
	{
		if (mn::path_is_file(module.value.loaded_file))
			mn::file_remove(module.value.loaded_file);
	}

	destruct(self->modules);
	mn::mutex_free(self->mtx);
	mn::free(self);
}

bool
rad_register(RAD* self, const char* name, const char* filepath)
{
	mn::allocator_push(mn::memory::clib());
	mn_defer(mn::allocator_pop());

	auto os_filepath = mn::str_new();
	#if OS_WINDOWS
		if (mn::str_suffix(filepath, ".dll"))
			os_filepath = mn::str_from_c(filepath);
		else
			os_filepath = mn::strf("{}.dll", filepath);
	#elif OS_LINUX
		if (mn::str_suffix(filepath, ".so"))
			os_filepath = mn::str_from_c(filepath);
		else
			os_filepath = mn::strf("{}.so", filepath);
	#elif OS_MACOS
		if (mn::str_suffix(filepath, ".dylib"))
			os_filepath = mn::str_from_c(filepath);
		else
			os_filepath = mn::strf("{}.dylib", filepath);
	#endif
	mn_defer(mn::str_free(os_filepath));

	if (mn::path_is_file(os_filepath) == false)
		return false;

	// file name
	mn::Str loaded_filepath{};
	if (self->settings.disable_hot_reload)
		loaded_filepath = mn::strf("{}", os_filepath);
	else
		loaded_filepath = mn::strf("{}-{}.loaded-0", os_filepath, self->uuid);

	if (self->settings.disable_hot_reload == false)
	{
		if (mn::path_is_file(loaded_filepath))
		{
			if (mn::file_remove(loaded_filepath) == false)
				return false;
		}
		if (mn::file_copy(os_filepath, loaded_filepath) == false)
			return false;
	}

	auto library = mn::library_open(loaded_filepath);
	if (library == nullptr)
	{
		// try another guess
		#if OS_LINUX
		auto os_filepath2 = mn::strf("./{}", os_filepath);
		auto loaded_filepath2 = mn::strf("./{}", loaded_filepath);
		mn_defer({
			mn::str_free(os_filepath2);
			mn::str_free(loaded_filepath2);
		});
		library = mn::library_open(loaded_filepath2);
		if (library == nullptr)
			return false;
		// success then replace the old strings
		auto tmp = os_filepath;
		os_filepath = os_filepath2;
		os_filepath2 = tmp;

		tmp = loaded_filepath;
		loaded_filepath = loaded_filepath2;
		loaded_filepath2 = tmp;
		#else
			return false;
		#endif
	}

	auto load_func = (Load_Func*)mn::library_proc(library, "rad_api");
	if (load_func == nullptr)
	{
		mn::library_close(library);
		return false;
	}

	RAD_Module mod{};
	mod.original_file = os_filepath;
	mod.loaded_file = loaded_filepath;
	mod.name = mn::str_from_c(name);
	mod.library = library;
	mod.last_write = mn::file_last_write_time(os_filepath);
	mod.api = load_func(nullptr, false);
	mod.load_counter = 0;
	{
		mn::mutex_lock(self->mtx);
		mn_defer(mn::mutex_unlock(self->mtx));

		if (auto it = mn::map_lookup(self->modules, mn::str_lit(name)))
		{
			destruct(it->value);
			it->value = mod;
		}
		else
		{
			mn::map_insert(self->modules, mn::str_from_c(name), mod);
		}
	}

	mn::log_info("rad loaded '{}' into '{}", os_filepath, loaded_filepath);
	os_filepath = mn::str_new();
	return true;
}

void*
rad_ptr(RAD* self, const char* name)
{
	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	if (auto it = mn::map_lookup(self->modules, mn::str_lit(name)))
		return it->value.api;
	return nullptr;
}

bool
rad_update(RAD* self)
{
	if (self->settings.disable_hot_reload)
		return false;

	mn::allocator_push(mn::memory::clib());
	mn_defer(mn::allocator_pop());

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	// hey hey trying to update
	bool overall_result = true;
	for(auto&[_, mod]: self->modules)
	{
		auto last_write = mn::file_last_write_time(mod.original_file);
		if (mod.last_write < last_write)
		{
			mn::log_info("module '{}' changed", mod.original_file);
			mod.load_counter++;

			auto loaded_filepath = mn::strf("{}-{}.loaded-{}", mod.original_file, self->uuid, mod.load_counter);
			if (mn::path_is_file(loaded_filepath))
			{
				if (mn::file_remove(loaded_filepath) == false)
				{
					overall_result &= false;
					mn::log_error("failed to remove '{}'", loaded_filepath);
					continue;
				}
			}

			if (mn::file_copy(mod.original_file, loaded_filepath) == false)
			{
				overall_result &= false;
				mn::log_error("failed to copy '{}' into '{}'", mod.original_file, loaded_filepath);
				continue;
			}

			auto library = mn::library_open(loaded_filepath);
			if (library == nullptr)
			{
				overall_result &= false;
				mn::log_error("module '{}' failed to open", loaded_filepath);
				mn::file_remove(loaded_filepath);
				continue;
			}

			auto load_func = (Load_Func*)mn::library_proc(library, "rad_api");
			if (load_func == nullptr)
			{
				overall_result &= false;
				mn::library_close(library);
				mn::file_remove(loaded_filepath);
				mn::log_error("module '{}' doesn't have rad_api function", loaded_filepath);
				continue;
			}

			// now call the new reload functions
			mn::allocator_pop();
			auto new_api = load_func(mod.api, true);
			mn::allocator_push(mn::memory::clib());
			if (new_api == nullptr)
			{
				overall_result &= false;
				mn::library_close(library);
				mn::file_remove(loaded_filepath);
				mn::log_error("module '{}' reload returned null", loaded_filepath);
				continue;
			}

			// now that we have reloaded the library we close the old one and remove its file
			mn::library_close(mod.library);
			mod.library = library;
			if (mn::file_remove(mod.loaded_file) == false)
				mn::log_error("failed to remove '{}'", mod.loaded_file);

			// replace the loaded filename
			mn::str_free(mod.loaded_file);
			mod.loaded_file = loaded_filepath;

			// replace the api
			mod.api = new_api;

			// replace the last_write time
			mod.last_write = last_write;
			mn::log_info("rad updated '{}' into '{}", mod.original_file, mod.loaded_file);
		}
	}
	return overall_result;
}
