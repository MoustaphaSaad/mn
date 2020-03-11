#include <mn/IO.h>
#include <mn/IPC.h>

int main(int argc, char** argv)
{
	mn::ipc::Pipe p{};
	if (argc == 1)
		p = mn::ipc::pipe_new("Mostafa");
	else
		p = mn::ipc::pipe_connect("Mostafa");

	mn::print("p: {}\n", (void*)p);
	mn::print("Hello, World!\n");
	return 0;
}
