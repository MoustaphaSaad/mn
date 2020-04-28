#include <mn/IO.h>
#include <mn/Root.h>
#include <mn/Defer.h>
#include <mn/Thread.h>
#include <mn/Path.h>

#include "hot_reload_lib.h"

int main(int argc, char** argv)
{
	auto folder = mn::file_directory(argv[0], mn::memory::tmp());
	mn::path_current_change(folder);

	mn::print("Hello, World!\n");

	auto root = mn::root_new();
	mn_defer(mn::root_free(root));

	// load
	if(mn::root_register(root, HOT_RELOAD_LIB_NAME, "hot_reload_lib.dll") == false)
	{
		mn::printerr("can't load library\n");
		return EXIT_FAILURE;
	}

	for(;;)
	{
		auto foo = mn::root_api<hot_reload_lib::Foo>(root, HOT_RELOAD_LIB_NAME);
		mn::print("foo.x: {}\n", foo->x++);
		mn::thread_sleep(1000);
		mn::root_update(root);
	}
	return 0;
}
