#include <mn/IO.h>
#include <mn/IPC.h>

int main(int argc, char** argv)
{
	mn::ipc::Pipe p{};
	if (argc == 1)
		p = mn::ipc::pipe_new("build/Mostafa");
	else
		p = mn::ipc::pipe_connect("build/Mostafa");

	mn::print("p: {}\n", (void*)p);
	mn::print("Hello, World!\n");
	::getchar();
	mn::ipc::pipe_free(p);
	return 0;
}
