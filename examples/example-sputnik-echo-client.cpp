#include <mn/IO.h>
#include <mn/IPC.h>
#include <mn/Defer.h>

#include <assert.h>

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

		write_bytes = mn::ipc::sputnik_write(client, mn::block_from(line));
		assert(write_bytes == line.count && "sputnik_write failed");

		mn::str_resize(line, 1024);
		read_bytes = mn::ipc::sputnik_read(client, mn::block_from(line));
		assert(read_bytes == write_bytes);

		mn::str_resize(line, read_bytes);
		mn::print("server: '{}'\n", line);
	} while(read_bytes > 0);

	return 0;
}