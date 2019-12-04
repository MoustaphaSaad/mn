#include <mn/IO.h>
#include <mn/IPC.h>
#include <mn/Defer.h>
#include <mn/Thread.h>

int main()
{
	mn::print("Hello, World!\n");
	auto [mtx, first] = mn::ipc::mutex_new("/mostafa/moshi-moshi", true);
	mn_defer(mn::ipc::mutex_free(mtx));

	mn::thread_sleep(10000);
	mn::print("Good Morning\n");

	mn::ipc::mutex_unlock(mtx);
	mn::print("Mutex Unlocked\n");

	mn::thread_sleep(10000);

	mn::print("Bye Bye!");
	return 0;
}