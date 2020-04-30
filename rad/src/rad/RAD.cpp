#include "rad/RAD.h"
#include <mn/Buf.h>
#include <mn/Str.h>
#include <mn/Library.h>
#include <mn/Defer.h>
#include <mn/Thread.h>
#include <mn/Path.h>
#include <mn/IO.h>

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
	mn::str_free(self.original_file);
	mn::str_free(self.loaded_file);
	mn::str_free(self.name);
	mn::library_close(self.library);
}

struct RAD
{
	mn::Mutex mtx;
	mn::Map<mn::Str, RAD_Module> modules;
};

// API
RAD*
rad_new()
{
	mn::allocator_push(mn::memory::clib());
	mn_defer(mn::allocator_pop());

	auto self = mn::alloc<RAD>();
	self->mtx = mn::mutex_new("Root Mutex");
	self->modules = mn::map_new<mn::Str, RAD_Module>();
	return self;
}

void
rad_free(RAD* self)
{
	mn::allocator_push(mn::memory::clib());
	mn_defer(mn::allocator_pop());

	destruct(self->modules);
	mn::mutex_free(self->mtx);
	mn::free(self);
}

bool
rad_register(RAD* self, const char* name, const char* filepath)
{
	mn::allocator_push(mn::memory::clib());
	mn_defer(mn::allocator_pop());

	if (mn::path_is_file(filepath) == false)
		return false;

	// file name
	auto loaded_filepath = mn::strf("{}.loaded-0", filepath);
	if (mn::path_is_file(loaded_filepath))
	{
		if (mn::file_remove(loaded_filepath) == false)
			return false;
	}
	if (mn::file_copy(filepath, loaded_filepath) == false)
		return false;

	auto library = mn::library_open(loaded_filepath);
	if (library == nullptr)
		return false;

	auto load_func = (Load_Func*)mn::library_proc(library, "rad_api");
	if (load_func == nullptr)
	{
		mn::library_close(library);
		return false;
	}

	RAD_Module mod{};
	mod.original_file = mn::str_from_c(filepath);
	mod.loaded_file = loaded_filepath;
	mod.name = mn::str_from_c(name);
	mod.library = library;
	mod.last_write = mn::file_last_write_time(filepath);
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

	return true;
}

void*
rad_ptr(RAD* self, const char* name)
{
	mn::allocator_push(mn::memory::clib());
	mn_defer(mn::allocator_pop());

	mn::mutex_lock(self->mtx);
	mn_defer(mn::mutex_unlock(self->mtx));

	if (auto it = mn::map_lookup(self->modules, mn::str_lit(name)))
		return it->value.api;
	return nullptr;
}

bool
rad_update(RAD* self)
{
	mn_defer(mn::memory::tmp()->free_all());

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
			mod.load_counter++;
			
			auto loaded_filepath = mn::strf("{}.loaded-{}", mod.original_file, mod.load_counter);
			if (mn::path_is_file(loaded_filepath))
			{
				if (mn::file_remove(loaded_filepath) == false)
				{
					overall_result |= false;
					continue;
				}
			}

			if (mn::file_copy(mod.original_file, loaded_filepath) == false)
			{
				overall_result |= false;
				continue;
			}

			auto library = mn::library_open(loaded_filepath);
			if (library == nullptr)
			{
				overall_result |= false;
				continue;
			}

			auto load_func = (Load_Func*)mn::library_proc(library, "rad_api");
			if (load_func == nullptr)
			{
				mn::library_close(library);
				overall_result |= false;
				continue;
			}

			// now call the new reload functions
			auto new_api = load_func(mod.api, true);
			if (new_api == nullptr)
			{
				overall_result |= false;
				mn::library_close(library);
				continue;
			}

			// now that we have reloaded the library we close the old one and remove its file
			mn::library_close(mod.library);
			mn::file_remove(mod.loaded_file);

			// replace the loaded filename
			mn::str_free(mod.loaded_file);
			mod.loaded_file = loaded_filepath;

			// replace the api
			mod.api = new_api;

			// replace the last_write time
			mod.last_write = last_write;
		}
	}
	return true;
}
