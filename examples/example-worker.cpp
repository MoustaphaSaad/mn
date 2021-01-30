#include <mn/IO.h>
#include <mn/Fabric.h>
#include <mn/Thread.h>

int main()
{
	auto fabric = mn::fabric_new({});

	mn::Waitgroup wg = 1000;

	for(size_t i = 0; i < 1000; ++i)
	{
		mn::fabric_do(fabric, [i, &wg] {
			mn::thread_sleep(rand() % 1000);
			mn::print("Hello, from task #{}!\n", i);
			mn::waitgroup_done(wg);
		});
	}

	mn::waitgroup_wait(wg);
	mn::print("Done\n");
	mn::fabric_free(fabric);
	return 0;
}
