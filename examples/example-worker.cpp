#include <mn/IO.h>
#include <mn/Worker.h>
#include <mn/Thread.h>

#include <atomic>
#include <condition_variable>

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
		auto fabric = mn::fabric_new();
		auto single = mn::fabric_worker_next(fabric);

		std::atomic<size_t> wg = 1000;

		for(size_t i = 0; i < 1000; ++i)
			mn::worker_do(single, [i, &wg] { mn::thread_sleep(rand() % 500); mn::print("Hello, from task #{}!\n", i); wg.fetch_sub(1); });
		
		while(wg.load() > 0)
		{
			mn::thread_sleep(1);
		}
		mn::print("Done\n");
		mn::fabric_free(fabric);
	}
	return 0;
}
