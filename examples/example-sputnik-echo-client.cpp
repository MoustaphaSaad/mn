#include <mn/IO.h>
#include <mn/IPC.h>
#include <mn/Defer.h>

#include <assert.h>

void
byte_client(mn::ipc::Sputnik client, mn::Str& line)
{
	auto write_bytes = mn::ipc::sputnik_write(client, mn::block_from(line));
	assert(write_bytes == line.count && "sputnik_write failed");

	mn::str_resize(line, 1024);
	auto read_bytes = mn::ipc::sputnik_read(client, mn::block_from(line));
	assert(read_bytes == write_bytes);

	mn::str_resize(line, read_bytes);
	mn::print("server: '{}'\n", line);
}

void
msg_client(mn::ipc::Sputnik client, mn::Str& line)
{
	mn::ipc::sputnik_msg_write(client, mn::block_from(line));
	auto msg = mn::ipc::sputnik_msg_read_alloc(client);
	mn::print("server: '{}'\n", line);
	mn::str_free(msg);
}

int
main()
{
	auto client = mn::ipc::sputnik_connect("sputnik");
	assert(client && "sputnik_connect failed");
	mn_defer(mn::ipc::sputnik_free(client));

	auto line = mn::str_new();
	mn_defer(mn::str_free(line));
	size_t read_bytes = 0, write_bytes = 0;
	do
	{
		mn::readln(line);
		if(line == "quit")
			break;
		else if(line.count == 0)
			continue;

		mn::print("you write: '{}'\n", line);

		// client byte stream
		//byte_client(client, line);
		// client msg units
		msg_client(client, line);
		read_bytes = 1;
	} while(read_bytes > 0);

	return 0;
}