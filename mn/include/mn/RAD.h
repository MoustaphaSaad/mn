#pragma once

#include "mn/Exports.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// RAD is a simple dynamic library loader which works by opening the registered libraries
// and watching them for changes, when you call update it will attempt to reload the changed libraries
// RAD interface requires the DLL to have a rad_api function
// void* rad_api(void* api, bool reload);
// on dll first load `rad_api` function will be called with (nullptr, false)
// on dll reload `rad_api` function will be called with (pointer, true)
// on dll unload `rad_api` function will be called with (pointer, false)
struct RAD;
MN_EXPORT extern struct RAD* rad_global;

typedef struct RAD_Settings
{
	bool disable_hot_reload;
} RAD_Settings;

// creates a new RAD instance
MN_EXPORT struct RAD*
rad_new(RAD_Settings settings);

// frees the given RAD instance
MN_EXPORT void
rad_free(struct RAD* self);

// registers a shared library as modules in this rad instance
MN_EXPORT bool
rad_register(struct RAD* self, const char* name, const char* filepath);

// searches for the given entity by name and returns a pointer to it
// if it doesn't exist a NULL is returned
MN_EXPORT void*
rad_ptr(struct RAD* self, const char* name);

// attempts to update the loaded modules if they have changed
MN_EXPORT bool
rad_update(struct RAD* self);

#ifdef __cplusplus
}
#endif

// C++ sugar functions
#ifdef __cplusplus
// just sugar over rad_ptr which casts the return
template<typename T>
inline T*
rad_api(RAD* self, const char* name)
{
	return (T*)rad_ptr(self, name);
}
#endif
