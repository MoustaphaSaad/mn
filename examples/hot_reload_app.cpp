#include <mn/IO.h>
#include <mn/Defer.h>
#include <mn/Thread.h>
#include <mn/Path.h>

#include <rad/RAD.h>

#include "hot_reload_lib.h"

int main(int argc, char** argv)
{
	auto folder = mn::file_directory(argv[0], mn::memory::tmp());
	mn::path_current_change(folder);

	mn::print("Hello, World!\n");

	auto rad = rad_new();
	mn_defer(rad_free(rad));

	// load
	if(rad_register(rad, HOT_RELOAD_LIB_NAME, "hot_reload_lib.dll") == false)
	{
		mn::printerr("can't load library\n");
		return EXIT_FAILURE;
	}

	for(;;)
	{
		auto foo = rad_api<hot_reload_lib::Foo>(rad, HOT_RELOAD_LIB_NAME);
		mn::print("foo.x: {}\n", foo->x++);
		mn::thread_sleep(1000);
		rad_update(rad);
	}
	return 0;
}
