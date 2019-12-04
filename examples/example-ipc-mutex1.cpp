#include <mn/IO.h>
#include <mn/IPC.h>
#include <mn/Defer.h>
#include <mn/Thread.h>

int main()
{
	mn::print("Hello, World!\n");
	auto [mtx, locked] = mn::ipc::mutex_new("moshimoshi", true);
	mn_defer(mn::ipc::mutex_free(mtx));

	auto koko = mn::ipc::mutex_try_lock(mtx);

	mn::print("first {}\n", locked);
	mn::thread_sleep(10000);
	mn::print("Good Morning\n");

	mn::ipc::mutex_unlock(mtx);
	mn::print("Mutex Unlocked\n");

	mn::thread_sleep(10000);

	mn::print("Bye Bye!");
	return 0;
}