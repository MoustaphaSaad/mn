#include <doctest/doctest.h>

#include <mn/Memory.h>
#include <mn/Buf.h>
#include <mn/Str.h>
#include <mn/Map.h>
#include <mn/Pool.h>
#include <mn/Memory_Stream.h>
#include <mn/Virtual_Memory.h>
#include <mn/IO.h>
#include <mn/Str_Intern.h>
#include <mn/Ring.h>
#include <mn/OS.h>
#include <mn/memory/Leak.h>
#include <mn/Task.h>
#include <mn/Path.h>
#include <mn/Fmt.h>
#include <mn/Defer.h>
#include <mn/Deque.h>

#include <chrono>
#include <iostream>
#include <sstream>

using namespace mn;

TEST_CASE("allocation")
{
	Block b = alloc(sizeof(int), alignof(int));
	CHECK(b.ptr != nullptr);
	CHECK(b.size != 0);

	free(b);
}

TEST_CASE("stack allocator")
{
	Allocator stack = allocator_stack_new(1024);

	allocator_push(stack);
	CHECK(allocator_top() == stack);

	Block b = alloc(512, alignof(char));
	free(b);

	allocator_pop();

	allocator_free(stack);
}

TEST_CASE("arena allocator")
{
	Allocator arena = allocator_arena_new(512);

	allocator_push(arena);
	CHECK(allocator_top() == arena);

	for (int i = 0; i < 1000; ++i)
		alloc<int>();

	allocator_pop();

	allocator_free(arena);
}

TEST_CASE("tmp allocator")
{
	{
		Str name = str_with_allocator(memory::tmp());
		str_pushf(name, "Name: %s", "Mostafa");
		CHECK(name == str_lit("Name: Mostafa"));
	}

	memory::tmp()->free_all();

	{
		Str name = str_with_allocator(memory::tmp());
		str_pushf(name, "Name: %s", "Mostafa");
		CHECK(name == str_lit("Name: Mostafa"));
	}

	memory::tmp()->free_all();
}

TEST_CASE("buf push")
{
	auto arr = buf_new<int>();
	for (int i = 0; i < 10; ++i)
		buf_push(arr, i);
	for (size_t i = 0; i < arr.count; ++i)
		CHECK(i == arr[i]);
	buf_free(arr);
}

TEST_CASE("range for loop")
{
	auto arr = buf_new<int>();
	for (int i = 0; i < 10; ++i)
		buf_push(arr, i);
	size_t i = 0;
	for (auto num : arr)
		CHECK(num == i++);
	buf_free(arr);
}

TEST_CASE("buf pop")
{
	auto arr = buf_new<int>();
	for (int i = 0; i < 10; ++i)
		buf_push(arr, i);
	CHECK(buf_empty(arr) == false);
	for (size_t i = 0; i < 10; ++i)
		buf_pop(arr);
	CHECK(buf_empty(arr) == true);
	buf_free(arr);
}

TEST_CASE("str push")
{
	Str str = str_new();

	str_push(str, "Mostafa");
	CHECK("Mostafa" == str);

	str_push(str, " Saad");
	CHECK(str == str_lit("Mostafa Saad"));

	str_push(str, " Abdel-Hameed");
	CHECK(str == str_lit("Mostafa Saad Abdel-Hameed"));

	str_pushf(str, " age: %d", 25);
	CHECK(str == "Mostafa Saad Abdel-Hameed age: 25");

	Str new_str = str_new();
	for(const char* it = begin(str); it != end(str); it = rune_next(it))
	{
		Rune r = rune_read(it);
		str_push(new_str, r);
	}
	CHECK(new_str == str);

	str_free(new_str);
	str_free(str);
}

TEST_CASE("str null terminate")
{
	Str str = str_new();
	str_null_terminate(str);
	CHECK(str == "");
	CHECK(str.count == 0);

	buf_pushn(str, 5, 'a');
	str_null_terminate(str);
	CHECK(str == "aaaaa");
	str_free(str);
}

TEST_CASE("str find")
{
	CHECK(str_find("hello world", "hello world", 0) == 0);
	CHECK(str_find("hello world", "hello", 0) == 0);
	CHECK(str_find("hello world", "hello", 1) == -1);
	CHECK(str_find("hello world", "world", 0) == 6);
	CHECK(str_find("hello world", "ld", 0) == 9);
}

TEST_CASE("str split")
{
	auto res = str_split(",A,B,C,", ",", true);
	CHECK(res.count == 3);
	CHECK(res[0] == "A");
	CHECK(res[1] == "B");
	CHECK(res[2] == "C");
	destruct(res);

	res = str_split("A,B,C", ",", false);
	CHECK(res.count == 3);
	CHECK(res[0] == "A");
	CHECK(res[1] == "B");
	CHECK(res[2] == "C");
	destruct(res);

	res = str_split(",A,B,C,", ",", false);
	CHECK(res.count == 5);
	CHECK(res[0] == "");
	CHECK(res[1] == "A");
	CHECK(res[2] == "B");
	CHECK(res[3] == "C");
	CHECK(res[4] == "");
	destruct(res);

	res = str_split("A", ";;;", true);
	CHECK(res.count == 1);
	CHECK(res[0] == "A");
	destruct(res);

	res = str_split("", ",", false);
	CHECK(res.count == 1);
	CHECK(res[0] == "");
	destruct(res);

	res = str_split("", ",", true);
	CHECK(res.count == 0);
	destruct(res);

	res = str_split(",,,,,", ",", true);
	CHECK(res.count == 0);
	destruct(res);

	res = str_split(",,,", ",", false);
	CHECK(res.count == 4);
	CHECK(res[0] == "");
	CHECK(res[1] == "");
	CHECK(res[2] == "");
	CHECK(res[3] == "");
	destruct(res);

	res = str_split(",,,", ",,", false);
	CHECK(res.count == 2);
	CHECK(res[0] == "");
	CHECK(res[1] == ",");
	destruct(res);

	res = str_split("test", ",,,,,,,,", false);
	CHECK(res.count == 1);
	CHECK(res[0] == "test");
	destruct(res);

	res = str_split("test", ",,,,,,,,", true);
	CHECK(res.count == 1);
	CHECK(res[0] == "test");
	destruct(res);
}

TEST_CASE("str trim")
{
	Str s = str_from_c("     \r\ntrim  \v");
	str_trim(s);
	CHECK(s == "trim");
	str_free(s);

	s = str_from_c("     \r\ntrim \n koko \v");
	str_trim(s);
	CHECK(s == "trim \n koko");
	str_free(s);
}

TEST_CASE("map general cases")
{
	auto num = map_new<int, int>();

	for (int i = 0; i < 10; ++i)
		map_insert(num, i, i + 10);

	for (int i = 0; i < 10; ++i)
	{
		CHECK(map_lookup(num, i)->key == i);
		CHECK(map_lookup(num, i)->value == i + 10);
	}

	for (int i = 10; i < 20; ++i)
		CHECK(map_lookup(num, i) == nullptr);

	for (int i = 0; i < 10; ++i)
	{
		if (i % 2 == 0)
			map_remove(num, i);
	}

	for (int i = 0; i < 10; ++i)
	{
		if (i % 2 == 0)
			CHECK(map_lookup(num, i) == nullptr);
		else
		{
			CHECK(map_lookup(num, i)->key == i);
			CHECK(map_lookup(num, i)->value == i + 10);
		}
	}

	int i = 0;
	for (auto it = map_begin(num);
		it != map_end(num);
		it = map_next(num, it))
	{
		++i;
	}
	CHECK(i == 5);

	map_free(num);
}

TEST_CASE("Pool general case")
{
	Pool pool = pool_new(sizeof(int), 1024);
	int* ptr = (int*)pool_get(pool);
	CHECK(ptr != nullptr);
	*ptr = 234;
	pool_put(pool, ptr);
	int* new_ptr = (int*)pool_get(pool);
	CHECK(new_ptr == ptr);
	pool_free(pool);
}

TEST_CASE("Memory_Stream general case")
{
	Memory_Stream mem = memory_stream_new();
	CHECK(memory_stream_size(mem) == 0);
	CHECK(memory_stream_cursor_pos(mem) == 0);
	memory_stream_write(mem, block_lit("Mostafa"));
	CHECK(memory_stream_size(mem) == 7);
	CHECK(memory_stream_cursor_pos(mem) == 7);

	char name[8] = { 0 };
	CHECK(memory_stream_read(mem, block_from(name)) == 0);
	CHECK(memory_stream_cursor_pos(mem) == 7);

	memory_stream_cursor_to_start(mem);
	CHECK(memory_stream_cursor_pos(mem) == 0);

	CHECK(memory_stream_read(mem, block_from(name)) == 7);
	CHECK(memory_stream_cursor_pos(mem) == 7);

	CHECK(::strcmp(name, "Mostafa") == 0);
	memory_stream_free(mem);
}

TEST_CASE("virtual memory allocation")
{
	size_t size = 1ULL * 1024ULL * 1024ULL * 1024ULL;
	Block block = virtual_alloc(nullptr, size);
	CHECK(block.ptr != nullptr);
	CHECK(block.size == size);
	virtual_free(block);
}

TEST_CASE("reads")
{
	int a, b;
	float c, d;
	Str e = str_new();
	size_t read_count = reads("-123 20 1.23 0.123 Mostafa ", a, b, c, d, e);
	CHECK(read_count == 5);
	CHECK(a == -123);
	CHECK(b == 20);
	CHECK(c == 1.23f);
	CHECK(d == 0.123f);
	CHECK(e == "Mostafa");
	str_free(e);
}

TEST_CASE("reader")
{
	Reader reader = reader_wrap_str(nullptr, str_lit("Mostafa Saad"));
	Str str = str_new();
	size_t read_count = readln(reader, str);
	CHECK(read_count == 12);
	CHECK(str == "Mostafa Saad");

	str_free(str);
	reader_free(reader);
}

TEST_CASE("path windows os encoding")
{
	auto os_path = path_os_encoding("C:/bin/my_file.exe");

	#if OS_WINDOWS
		CHECK(os_path == "C:\\bin\\my_file.exe");
	#endif

	str_free(os_path);
}

TEST_CASE("Str_Intern general case")
{
	Str_Intern intern = str_intern_new();

	const char* is = str_intern(intern, "Mostafa");
	CHECK(is != nullptr);
	CHECK(is == str_intern(intern, "Mostafa"));

	const char* big_str = "my name is Mostafa";
	const char* begin = big_str + 11;
	const char* end = begin + 7;
	CHECK(is == str_intern(intern, begin, end));

	str_intern_free(intern);
}

TEST_CASE("simple data ring case")
{
	allocator_push(memory::leak());

	Ring<int> r = ring_new<int>();

	for (int i = 0; i < 10; ++i)
		ring_push_back(r, i);

	for (size_t i = 0; i < r.count; ++i)
		CHECK(r[i] == i);

	for (int i = 0; i < 10; ++i)
		ring_push_front(r, i);

	for (int i = 9; i >= 0; --i)
	{
		CHECK(ring_back(r) == i);
		ring_pop_back(r);
	}

	for (int i = 9; i >= 0; --i)
	{
		CHECK(ring_front(r) == i);
		ring_pop_front(r);
	}

	ring_free(r);

	allocator_pop();
}

TEST_CASE("complex data ring case")
{
	allocator_push(memory::leak());
	Ring<Str> r = ring_new<Str>();

	for (int i = 0; i < 10; ++i)
		ring_push_back(r, str_from_c("Mostafa"));

	for (int i = 0; i < 10; ++i)
		ring_push_front(r, str_from_c("Saad"));

	for (int i = 4; i >= 0; --i)
	{
		CHECK(ring_back(r) == "Mostafa");
		str_free(ring_back(r));
		ring_pop_back(r);
	}

	for (int i = 4; i >= 0; --i)
	{
		CHECK(ring_front(r) == "Saad");
		str_free(ring_front(r));
		ring_pop_front(r);
	}

	destruct(r);

	allocator_pop();
}

TEST_CASE("Rune")
{
	CHECK(rune_upper('a') == 'A');
	CHECK(rune_upper('A') == 'A');
	CHECK(rune_lower('A') == 'a');
	CHECK(rune_lower('a') == 'a');
	CHECK(rune_lower('م') == 'م');
}

TEST_CASE("Task")
{
	CHECK(std::is_pod_v<Task<void()>> == true);

	auto add = Task<int(int, int)>::make([](int a, int b) { return a + b; });
	CHECK(std::is_pod_v<decltype(add)> == true);

	auto inc = Task<int(int)>::make([=](int a) mutable { return add(a, 1); });
	CHECK(std::is_pod_v<decltype(inc)> == true);

	CHECK(add(1, 2) == 3);
	CHECK(inc(5) == 6);

	task_free(add);
	task_free(inc);
}

struct V2 
{
	int x, y;
};

inline static std::ostream&
operator<<(std::ostream& out, const V2& v)
{
	out << "V2{ " << v.x << ", " << v.y << " }";
	return out;
}

TEST_CASE("Fmt")
{
	SUBCASE("str formatting")
	{
		Str n = strf("{}", str_lit("mostafa"));
		CHECK(n == "mostafa");
		str_free(n);
	}

	SUBCASE("buf formatting")
	{
		Buf<int> b = buf_lit({1, 2, 3});
		Str n = strf("{}", b);
		CHECK(n == "[3]{0: 1, 1: 2, 2: 3 }");
		str_free(n);
		buf_free(b);
	}

	SUBCASE("map formatting")
	{
		Map<Str, V2> m = map_new<Str, V2>();
		map_insert(m, str_from_c("ABC"), V2{654, 765});
		map_insert(m, str_from_c("DEF"), V2{6541, 7651});
		Str n = strf("{}", m);
		CHECK(n == "[2]{ ABC: V2{ 654, 765 }, DEF: V2{ 6541, 7651 } }");
		str_free(n);
		destruct(m);
	}
}

TEST_CASE("Deque")
{
	SUBCASE("deque push")
	{
		Deque nums = deque_new<int>();
		for (int i = 0; i < 1000; ++i)
		{
			if (i % 2 == 0)
				deque_push_front(nums, i);
			else
				deque_push_back(nums, i);
		}

		for (int i = 0; i < 500; ++i)
			CHECK(nums[i] % 2 == 0);

		for (int i = 500; i < 1000; ++i)
			CHECK(nums[i] % 2 != 0);

		deque_free(nums);
	}

	SUBCASE("deque pop")
	{
		Deque nums = deque_new<int>();
		for (int i = 0; i < 10; ++i)
		{
			if (i % 2 == 0)
				deque_push_front(nums, i);
			else
				deque_push_back(nums, i);
		}

		CHECK(deque_front(nums) == 8);
		CHECK(deque_back(nums) == 9);

		deque_pop_front(nums);
		CHECK(deque_front(nums) == 6);

		deque_pop_back(nums);
		CHECK(deque_back(nums) == 7);

		deque_free(nums);
	}
}
