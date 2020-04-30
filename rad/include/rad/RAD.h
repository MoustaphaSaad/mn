#pragma once

#include "rad/Exports.h"

#ifdef __cplusplus
extern "C" {
#endif

struct RAD;

// rad_new creates a new RAD instance
RAD_EXPORT struct RAD*
rad_new();

// rad_free frees the given RAD instance
RAD_EXPORT void
rad_free(struct RAD* self);

// rad_register registers a shared library as modules in this rad instance
RAD_EXPORT bool
rad_register(struct RAD* self, const char* name, const char* filepath);

// rad_ptr searches for the given entity by name and returns a pointer to it
// if it doesn't exist a NULL is returned
RAD_EXPORT void*
rad_ptr(struct RAD* self, const char* name);

// rad_update attempts to update the loaded modules if they have changed
RAD_EXPORT bool
rad_update(struct RAD* self);

#ifdef __cplusplus
}
#endif

// C++ sugar functions
#ifdef __cplusplus
// rad_api just sugar over rad_ptr which casts the return
template<typename T>
inline T*
rad_api(RAD* self, const char* name)
{
	return (T*)rad_ptr(self, name);
}
#endif
