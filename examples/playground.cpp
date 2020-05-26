#include <mn/IO.h>
#include <mn/Memory.h>

int main(int argc, char** argv)
{
	auto buddy = mn::allocator_buddy_new();
	auto nums = mn::buf_with_allocator<int>(buddy);
	for(int i = 0; i < 1000; ++i)
		mn::buf_push(nums, i);
	auto test = mn::alloc_from(buddy, 1024*1024 - 16, alignof(int));
	mn::buf_free(nums);
	mn::allocator_free(buddy);
	mn::print("Hello, World!\n");
	return 0;
}
