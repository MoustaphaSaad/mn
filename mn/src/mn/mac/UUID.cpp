#include "mn/UUID.h"

#include <CoreFoundation/CFUUID.h>

namespace mn
{
	UUID
	uuid_generate()
	{
		UUID self{};
		auto CFUUID = CFUUIDCreate(NULL);
		auto bytes = CFUUIDGetUUIDBytes(CFUUID);
		::memcpy(self.bytes, bytes, 16);
		return self;
	}
} // namespace mn