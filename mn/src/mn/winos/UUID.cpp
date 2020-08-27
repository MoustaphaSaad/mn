#include "mn/UUID.h"

#include <objbase.h>

namespace mn
{
	UUID
	uuid_generate()
	{
		UUID self{};

		GUID newID;
		CoCreateGuid(&newID);

		self.bytes[0] = uint8_t((newID.Data1 >> 24) & 0xFF);
		self.bytes[1] = uint8_t((newID.Data1 >> 16) & 0xFF);
		self.bytes[2] = uint8_t((newID.Data1 >> 8) & 0xFF);
		self.bytes[3] = uint8_t((newID.Data1) & 0xFF);

		self.bytes[4] = uint8_t((newID.Data2 >> 8) & 0xFF);
		self.bytes[5] = uint8_t((newID.Data2) & 0xFF);

		self.bytes[6] = uint8_t((newID.Data3 >> 8) & 0xFF);
		self.bytes[7] = uint8_t((newID.Data3) & 0xFF);

		self.bytes[8]  = uint8_t(newID.Data4[0]);
		self.bytes[9]  = uint8_t(newID.Data4[1]);
		self.bytes[10] = uint8_t(newID.Data4[2]);
		self.bytes[11] = uint8_t(newID.Data4[3]);
		self.bytes[12] = uint8_t(newID.Data4[4]);
		self.bytes[13] = uint8_t(newID.Data4[5]);
		self.bytes[14] = uint8_t(newID.Data4[6]);
		self.bytes[15] = uint8_t(newID.Data4[7]);

		return self;
	}
} // namespace mn