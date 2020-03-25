#include <mn/IO.h>
#include <mn/Fabric.h>
#include <mn/IPC.h>
#include <mn/Defer.h>

#include <assert.h>

void
serve_client(mn::ipc::Sputnik client)
{
	auto data = mn::str_new();
	mn_defer({
		mn::str_free(data);
		mn::ipc::sputnik_free(client);
	});
	size_t read_bytes = 0, write_bytes = 0;

	do
	{
		mn::str_resize(data, 1024);
		read_bytes = mn::ipc::sputnik_read(client, mn::block_from(data), mn::INFINITE_TIMEOUT);
		if(read_bytes > 0)
		{
			mn::str_resize(data, read_bytes);
			write_bytes = mn::ipc::sputnik_write(client, mn::block_from(data));
			assert(write_bytes == read_bytes && "sputnik_write failed");
		}
	} while(read_bytes > 0);
}

void
serve_client_msg(mn::ipc::Sputnik client)
{
	mn_defer({
		mn::ipc::sputnik_free(client);
	});

	do
	{
		auto msg = mn::ipc::sputnik_msg_read_alloc(client, mn::INFINITE_TIMEOUT);
		if(msg.count == 0)
			break;
		mn::ipc::sputnik_msg_write(client, block_from(msg));
		mn::str_free(msg);
	} while(true);
}

int
main()
{
	auto f = mn::fabric_new();
	mn_defer(mn::fabric_free(f));

	auto server = mn::ipc::sputnik_new("sputnik");
	assert(server && "sputnik_new failed");
	mn_defer({
		mn::ipc::sputnik_disconnect(server);
		mn::ipc::sputnik_free(server);
	});

	while(mn::ipc::sputnik_listen(server))
	{
		auto client = mn::ipc::sputnik_accept(server, { 10000 });
		if (client)
		{
			mn::go(f, [client] { serve_client_msg(client); });
		}
		else
		{
			mn::print("accept timed out, trying again\n");
		}
	}
	return 0;
}