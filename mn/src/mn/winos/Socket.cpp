#include "mn/Socket.h"
#include "mn/Fabric.h"

#include <WinSock2.h>
#include <WS2tcpip.h>

namespace mn
{
	struct _WIN_NET_INIT
	{
		_WIN_NET_INIT()
		{
			WORD wVersionRequested;
			WSADATA wsaData;
			int err;

			wVersionRequested = MAKEWORD(2, 2);
			err = WSAStartup(wVersionRequested, &wsaData);

			assert(err == 0 && "WSAStartup failed");

			assert(LOBYTE(wsaData.wVersion) == 2 && HIBYTE(wsaData.wVersion) == 2 &&
				"Could not find a usable version of Winsock.dll");
		}

		~_WIN_NET_INIT()
		{
			WSACleanup();
		}
	};
	static _WIN_NET_INIT _WIN_NET_INIT_INSTANCE;

	inline static int
	_socket_family_to_os(SOCKET_FAMILY f)
	{
		switch (f)
		{
		case SOCKET_FAMILY_IPV4:
			return AF_INET;
			break;
		case SOCKET_FAMILY_IPV6:
			return AF_INET6;
			break;
		default:
			assert(false && "unreachable");
			return 0;
		}
	}

	inline static void
	_socket_type_to_os(SOCKET_TYPE t, int& type, int& protocol)
	{
		switch (t)
		{
		case SOCKET_TYPE_TCP:
			type = SOCK_STREAM;
			protocol = IPPROTO_TCP;
			break;
		case SOCKET_TYPE_UDP:
			type = SOCK_DGRAM;
			protocol = IPPROTO_UDP;
			break;
		default:
			assert(false && "unreachable");
			break;
		}
	}


	// API
	void
	ISocket::dispose()
	{
		socket_close(this);
	}

	size_t
	ISocket::read(Block data)
	{
		return socket_read(this, data, INFINITE_TIMEOUT);
	}

	size_t
	ISocket::write(Block data)
	{
		return socket_write(this, data);
	}

	int64_t
	ISocket::size()
	{
		return 0;
	}

	Socket
	socket_open(SOCKET_FAMILY socket_family, SOCKET_TYPE socket_type)
	{
		int af = 0;
		int type = 0;
		int protocol = 0;

		af = _socket_family_to_os(socket_family);
		_socket_type_to_os(socket_type, type, protocol);

		auto handle = socket(af, type, protocol);
		if (handle == INVALID_SOCKET)
			return nullptr;

		auto self = mn::alloc_construct<ISocket>();
		self->handle = handle;
		self->family = socket_family;
		self->type = socket_type;
		return self;
	}

	void
	socket_close(Socket self)
	{
		::closesocket(self->handle);
		mn::free_destruct(self);
	}

	bool
	socket_connect(Socket self, const Str& address, const Str& port)
	{
		addrinfo hints{}, *info;

		hints.ai_family = _socket_family_to_os(self->family);
		_socket_type_to_os(self->type, hints.ai_socktype, hints.ai_protocol);

		int res = ::getaddrinfo(address.ptr, port.ptr, &hints, &info);
		if (res != 0)
			return false;

		worker_block_ahead();
		res = ::connect(self->handle, info->ai_addr, int(info->ai_addrlen));
		worker_block_clear();
		if (res == SOCKET_ERROR)
			return false;

		return true;
	}

	bool
	socket_bind(Socket self, const Str& port)
	{
		addrinfo hints{}, *info;

		hints.ai_family = _socket_family_to_os(self->family);
		_socket_type_to_os(self->type, hints.ai_socktype, hints.ai_protocol);
		hints.ai_flags = AI_PASSIVE;

		int res = ::getaddrinfo(nullptr, port.ptr, &hints, &info);
		if (res != 0)
			return false;

		res = ::bind(self->handle, info->ai_addr, int(info->ai_addrlen));
		if (res == SOCKET_ERROR)
			return false;

		return true;
	}

	bool
	socket_listen(Socket self, int max_connections)
	{
		if (max_connections == 0)
			max_connections = SOMAXCONN;

		worker_block_ahead();
		int res = ::listen(self->handle, max_connections);
		worker_block_clear();
		if (res == SOCKET_ERROR)
			return false;
		return true;
	}

	Socket
	socket_accept(Socket self, Timeout timeout)
	{
		pollfd pfd_read{};
		pfd_read.fd = self->handle;
		pfd_read.events = POLLIN;

		INT milliseconds = 0;
		if (timeout == INFINITE_TIMEOUT)
			milliseconds = INFINITE;
		else if (timeout == NO_TIMEOUT)
			milliseconds = 0;
		else
			milliseconds = INT(timeout.milliseconds);

		{
			worker_block_ahead();
			mn_defer(worker_block_clear());

			int ready = WSAPoll(&pfd_read, 1, milliseconds);
			if (ready == 0)
				return nullptr;
		}
		auto handle = ::accept(self->handle, nullptr, nullptr);
		if(handle == INVALID_SOCKET)
			return nullptr;

		auto other = mn::alloc_construct<ISocket>();
		other->handle = handle;
		other->family = self->family;
		other->type = self->type;
		return other;
	}

	void
	socket_disconnect(Socket self)
	{
		::shutdown(self->handle, SD_SEND);
	}

	void CALLBACK _completion_routine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)
	{
		return;
	}

	size_t
	socket_read(Socket self, Block data, Timeout timeout)
	{
		pollfd pfd_read{};
		pfd_read.fd = self->handle;
		pfd_read.events = POLLIN;

		WSABUF data_buf{};
		data_buf.len = ULONG(data.size);
		data_buf.buf = (char*)data.ptr;

		DWORD flags = 0;

		INT milliseconds = 0;
		if (timeout == INFINITE_TIMEOUT)
			milliseconds = INFINITE;
		else if (timeout == NO_TIMEOUT)
			milliseconds = 0;
		else
			milliseconds = INT(timeout.milliseconds);

		DWORD recieved_bytes = 0;
		worker_block_ahead();
		int ready = WSAPoll(&pfd_read, 1, milliseconds);
		if (ready > 0)
		{
			::WSARecv(
				self->handle,
				&data_buf,
				1,
				&recieved_bytes,
				&flags,
				NULL,
				NULL
			);
		}
		worker_block_clear();

		return recieved_bytes;
	}

	size_t
	socket_write(Socket self, Block data)
	{
		size_t sent_bytes = 0;

		WSABUF data_buf{};
		data_buf.len = ULONG(data.size);
		data_buf.buf = (char*)data.ptr;

		DWORD flags = 0;

		worker_block_ahead();
		int status = ::WSASend(
			self->handle,
			&data_buf,
			1,
			(LPDWORD)&sent_bytes,
			flags,
			NULL,
			NULL
		);
		worker_block_clear();

		if(status == 0)
			return sent_bytes;
		return 0;
	}

	int64_t
	socket_fd(Socket self)
	{
		return self->handle;
	}
}