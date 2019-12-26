#include <mn/IO.h>
#include <mn/Worker.h>
#include <mn/Thread.h>

int main()
{
	/*
	{
		auto worker = mn::worker_new("Test");
		mn::worker_do(worker, [] {mn::print("Hello, from worker!\n"); });
		mn::thread_sleep(1000);
		mn::worker_free(worker);
	}
	*/

	mn::print("Hello, World!\n");

	{
		auto fabric = mn::fabric_new(8);
		auto single = mn::fabric_worker_next(fabric);
		for(size_t i = 0; i < 1000; ++i)
			mn::worker_do(single, [i] { mn::print("Hello, from task #{}!\n", i); });
		mn::thread_sleep(1000);
		mn::fabric_free(fabric);
	}
	return 0;
}
