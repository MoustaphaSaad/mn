#include "mn/UUID.h"

#include <uuid/uuid.h>

#include <type_traits>

namespace mn
{
	UUID
	uuid_generate()
	{
		UUID self{};
		static_assert(std::is_same<unsigned char[16], uuid_t>::value, "Incompatible uuid_t");
		::uuid_generate(self.bytes);
		return self;
	}
} // namespace mn