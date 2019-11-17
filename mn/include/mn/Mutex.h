#pragma once

#include "mn/Exports.h"
#include "mn/Stream.h"
#include "mn/Str.h"

namespace mn
{
	struct MutexCreationResult
	{
		enum Result_KIND
		{
			SUCCESS,
			ALREADY_EXISTS,
			FAILURE
		};
		Result_KIND kind;
	};
	MN_EXPORT MutexCreationResult
	create_mutex(mn::Str mutex_name, bool isOwner = true);

	inline static MutexCreationResult
	create_mutex(const char* mutex_name, bool isOwner = true)
	{
		return create_mutex(mn::str_from_c(mutex_name), isOwner);
	}
}