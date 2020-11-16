#include <mn/Thread.h>
#include <mn/Defer.h>

int main()
{
	auto mtx = mn::mutex_new();
	mn_defer(mn::mutex_free(mtx));

	mn::mutex_lock(mtx);
	mn::mutex_lock(mtx);
	return 0;
}