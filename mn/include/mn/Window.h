#pragma once

#include "mn/Exports.h"
#include "mn/Stream.h"
#include "mn/Str.h"

namespace mn
{
	MN_EXPORT mn::Buf<uint64_t>
	get_windows_from_pid(uint64_t pid);
}