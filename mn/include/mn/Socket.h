#pragma once

#include "mn/Stream.h"
#include "mn/Str.h"

namespace mn
{
	enum SOCKET_FAMILY
	{
		SOCKET_FAMILY_IPV4,
		SOCKET_FAMILY_IPV6
	};

	enum SOCKET_TYPE
	{
		SOCKET_TYPE_TCP,
		SOCKET_TYPE_UDP
	};

	typedef struct ISocket* Socket;
	struct ISocket final: IStream
	{
		int64_t handle;
		SOCKET_FAMILY family;
		SOCKET_TYPE type;

		MN_EXPORT virtual void
		dispose() override;

		MN_EXPORT virtual size_t
		read(Block data) override;

		MN_EXPORT virtual size_t
		write(Block data) override;

		MN_EXPORT virtual int64_t
		size() override;
	};

	MN_EXPORT Socket
	socket_open(SOCKET_FAMILY family, SOCKET_TYPE type);

	MN_EXPORT void
	socket_close(Socket self);

	inline static void
	destruct(Socket self)
	{
		socket_close(self);
	}

	MN_EXPORT bool
	socket_connect(Socket self, const Str& address, const Str& port);

	inline static bool
	socket_connect(Socket self, const char* address, const char* port)
	{
		return socket_connect(self, str_lit(address), str_lit(port));
	}

	MN_EXPORT bool
	socket_bind(Socket self, const Str& port);

	inline static bool
	socket_bind(Socket self, const char* port)
	{
		return socket_bind(self, str_lit(port));
	}

	MN_EXPORT bool
	socket_listen(Socket self, int max_connections = 0);

	MN_EXPORT Socket
	socket_accept(Socket self, Timeout timeout);

	MN_EXPORT void
	socket_disconnect(Socket self);

	MN_EXPORT size_t
	socket_read(Socket self, Block data, Timeout timeout);

	MN_EXPORT size_t
	socket_write(Socket self, Block data);

	MN_EXPORT int64_t
	socket_fd(Socket self);
}
