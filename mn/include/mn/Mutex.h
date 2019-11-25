#pragma once

#include "mn/Exports.h"
#include "mn/Stream.h"
#include "mn/Str.h"

namespace mn
{
	enum MutexCreationResult
	{
		FAILURE,
		SUCCESS,
		ALREADY_EXISTS,		
	};

	MN_EXPORT MutexCreationResult
	create_mutex(mn::Str mutex_name, bool isOwner = true);

	inline static MutexCreationResult
	create_mutex(const char* mutex_name, bool isOwner = true)
	{
		return create_mutex(mn::str_from_c(mutex_name), isOwner);
	}

	MN_EXPORT bool
	wait_mutex(mn::Str mutex_name);
	
	inline static bool 
	wait_mutex(const char* mutex_name)
	{
		return wait_mutex(mn::str_from_c(mutex_name));
	}
}