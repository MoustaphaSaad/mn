#include "mn/UUID.h"

#include <CoreFoundation/CFUUID.h>

namespace mn
{
	UUID
	uuid_generate()
	{
		UUID self{};
		auto id = CFUUIDCreate(NULL);
		auto bytes = CFUUIDGetUUIDBytes(id);
		CFRelease(id);
		::memcpy(self.bytes, &bytes, 16);
		return self;
	}
} // namespace mn