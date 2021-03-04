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
#include <mn/Result.h>
#include <mn/Fabric.h>
#include <mn/Block_Stream.h>
#include <mn/Handle_Table.h>
#include <mn/UUID.h>
#include <mn/SIMD.h>
#include <mn/Json.h>

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
		name = strf(name, "Name: {}", "Mostafa");
		CHECK(name == str_lit("Name: Mostafa"));
	}

	memory::tmp()->free_all();

	{
		Str name = str_with_allocator(memory::tmp());
		name = strf(name, "Name: {}", "Mostafa");
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

TEST_CASE("buf insert and remove ordered")
{
	auto v = mn::buf_lit({1, 2, 3, 5});
	mn::buf_insert(v, 3, 4);
	for(size_t i = 0; i < v.count; ++i)
		CHECK(v[i] == i + 1);
	mn::buf_remove_ordered(v, 3);
	CHECK(v.count == 4);
	CHECK(v[0] == 1);
	CHECK(v[1] == 2);
	CHECK(v[2] == 3);
	CHECK(v[3] == 5);
	mn::buf_free(v);
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

	str = strf(str, " age: {}", 25);
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

	s = str_from_c("r");
	str_trim(s);
	CHECK(s == "r");
	str_free(s);
}

TEST_CASE("String lower case and upper case")
{
	auto word = str_from_c("مصطفى");
	str_lower(word);
	CHECK(word == "مصطفى");
	str_free(word);

	auto word2 = str_from_c("PERCHÉa");
	str_lower(word2);
	CHECK(word2 == "perchéa");
	str_free(word2);

	auto word3 = str_from_c("Æble");
	str_lower(word3);
	CHECK(word3 == "æble");
	str_free(word3);
}

TEST_CASE("set general cases")
{
	auto num = mn::set_new<int>();

	for (int i = 0; i < 10; ++i)
		mn::set_insert(num, i);

	for (int i = 0; i < 10; ++i)
	{
		CHECK(*mn::set_lookup(num, i) == i);
	}

	for (int i = 10; i < 20; ++i)
		CHECK(mn::set_lookup(num, i) == nullptr);

	for (int i = 0; i < 10; ++i)
	{
		if (i % 2 == 0)
			mn::set_remove(num, i);
	}

	for (int i = 0; i < 10; ++i)
	{
		if (i % 2 == 0)
		{
			CHECK(mn::set_lookup(num, i) == nullptr);
		}
		else
		{
			CHECK(*mn::set_lookup(num, i) == i);
		}
	}

	int i = 0;
	for (auto n: num)
		++i;
	CHECK(i == 5);

	mn::set_free(num);
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
	for(const auto& [key, value]: num)
		++i;
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

	int* new_ptr2 = (int*)pool_get(pool);
	pool_put(pool, new_ptr2);

	pool_put(pool, new_ptr);
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

TEST_CASE("reader with empty newline")
{
	auto text = R"""(my name is mostafa

mostafa is 26 years old)""";
	Reader reader = reader_wrap_str(nullptr, text);
	Str str = str_new();

	size_t read_count = readln(reader, str);
	CHECK(read_count == 19);
	CHECK(str == "my name is mostafa");

	read_count = readln(reader, str);
	CHECK(read_count == 1);
	CHECK(str == "");

	read_count = readln(reader, str);
	CHECK(read_count == 23);
	CHECK(str == "mostafa is 26 years old");

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
	#if defined(OS_WINDOWS)
		CHECK(rune_lower('\u0645') == '\u0645');
	#elif defined(OS_LINUX)
		CHECK(rune_lower('U+0645') == 'U+0645');
	#elif defined(OS_MACOS)
		CHECK(rune_lower('U+0645') == 'U+0645');
	#endif
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
	SUBCASE("empty deque")
	{
		Deque n = deque_new<int>();
		deque_free(n);
	}

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

Result<int> my_div(int a, int b)
{
	if (b == 0)
		return Err{ "can't calc '{}/{}' because b is 0", a, b };
	return a / b;
}

enum class Err_Code { OK, ZERO_DIV };

Result<int, Err_Code> my_div2(int a, int b)
{
	if (b == 0)
		return Err_Code::ZERO_DIV;
	return a / b;
}

TEST_CASE("Result default error")
{
	SUBCASE("no err")
	{
		auto [r, err] = my_div(4, 2);
		CHECK(err == false);
		CHECK(r == 2);
	}

	SUBCASE("err")
	{
		auto [r, err] = my_div(4, 0);
		CHECK(err == true);
	}
}

TEST_CASE("Result error code")
{
	SUBCASE("no err")
	{
		auto [r, err] = my_div2(4, 2);
		CHECK(err == Err_Code::OK);
		CHECK(r == 2);
	}

	SUBCASE("err")
	{
		auto [r, err] = my_div2(4, 0);
		CHECK(err == Err_Code::ZERO_DIV);
	}
}

TEST_CASE("fabric simple creation")
{
	Fabric_Settings settings{};
	settings.workers_count = 3;
	auto f = mn::fabric_new(settings);
	mn::fabric_free(f);
}

TEST_CASE("fabric simple function")
{
	Fabric_Settings settings{};
	settings.workers_count = 3;
	Fabric f = fabric_new(settings);

	int n = 0;

	Auto_Waitgroup g;

	g.add(1);

	go(f, [&n, &g] { ++n; g.done(); });

	g.wait();
	CHECK(n == 1);

	fabric_free(f);
}

TEST_CASE("unbuffered channel with multiple workers")
{
	Fabric_Settings settings{};
	settings.workers_count = 3;
	Fabric f = fabric_new(settings);
	Chan<size_t> c = chan_new<size_t>();
	Auto_Waitgroup g;

	std::atomic<size_t> sum = 0;

	auto worker = [c, &sum, &g]{
		for (auto& num : c)
			sum += num;
		g.done();
	};

	for(size_t i = 0; i < 3; ++i)
	{
		g.add(1);
		go(f, worker);
	}

	for (size_t i = 0; i <= 100; ++i)
		chan_send(c, i);
	chan_close(c);

	g.wait();
	CHECK(sum == 5050);

	chan_free(c);
	fabric_free(f);
}

TEST_CASE("buffered channel")
{
	Fabric_Settings settings{};
	settings.workers_count = 3;
	Fabric f = fabric_new(settings);
	Chan<size_t> c = chan_new<size_t>(1000);
	Auto_Waitgroup g;

	std::atomic<size_t> sum = 0;

	auto worker = [c, &sum, &g] {
		for (const auto& num : c)
			sum += num;
		g.done();
	};

	for (size_t i = 0; i < 6; ++i)
	{
		g.add(1);
		go(f, worker);
	}

	for (size_t i = 0; i <= 10000; ++i)
		chan_send(c, i);
	chan_close(c);

	g.wait();
	CHECK(sum == 50005000);

	chan_free(c);
	fabric_free(f);
}

TEST_CASE("unbuffered channel from coroutine")
{
	Fabric_Settings settings{};
	settings.workers_count = 3;
	Fabric f = fabric_new(settings);
	Chan<size_t> c = chan_new<size_t>();
	Auto_Waitgroup g;

	size_t sum = 0;

	g.add(1);
	go(f, [c, &sum, &g] {
		for (auto num : c)
			sum += num;
		g.done();
	});

	go(f, [c] {
		for (size_t i = 0; i <= 100; ++i)
			chan_send(c, i);
		chan_close(c);
	});

	g.wait();
	CHECK(sum == 5050);

	fabric_free(f);
	chan_free(c);
}

TEST_CASE("buffered channel from coroutine")
{
	Fabric_Settings settings{};
	settings.workers_count = 3;
	Fabric f = fabric_new(settings);
	Chan<size_t> c = chan_new<size_t>(1000);
	Auto_Waitgroup g;

	size_t sum = 0;

	g.add(1);
	go(f, [c, &sum, &g] {
		for (auto num : c)
			sum += num;
		g.done();
	});

	go(f, [c]{
		for (size_t i = 0; i <= 10000; ++i)
			chan_send(c, i);
		chan_close(c);
	});

	g.wait();
	CHECK(sum == 50005000);

	fabric_free(f);
	chan_free(c);
}

TEST_CASE("coroutine launching coroutines")
{
	Fabric_Settings settings{};
	settings.workers_count = 3;
	Fabric f = fabric_new(settings);
	Chan<int> c = chan_new<int>(1000);
	Auto_Waitgroup g;

	size_t sum = 0;

	g.add(1);
	go(f, [&g, &sum, c]{

		go([&g, &sum, c]{
			for (auto num : c)
				sum += num;
			g.done();
		});

		for (int i = 0; i <= 10000; ++i)
			chan_send(c, i);
		chan_close(c);
	});

	g.wait();
	CHECK(sum == 50005000);

	fabric_free(f);
	chan_free(c);
}

TEST_CASE("stress")
{
	Fabric f = fabric_new({});
	Chan<size_t> c = chan_new<size_t>(100);
	Auto_Waitgroup g;

	std::atomic<size_t> sum = 0;

	for (size_t i = 0; i <= 1000; ++i)
	{
		g.add(1);
		go(f, [c, i] { chan_send(c, i); });
		go(f, [c, &sum, &g] { auto[n, _] = chan_recv(c); sum += n; g.done(); });
	}

	g.wait();
	CHECK(sum == 500500);

	fabric_free(f);
	chan_free(c);
}

TEST_CASE("buddy")
{
	auto buddy = mn::allocator_buddy_new();
	auto nums = mn::buf_with_allocator<int>(buddy);
	for(int i = 0; i < 1000; ++i)
		mn::buf_push(nums, i);
	auto test = mn::alloc_from(buddy, 1024*1024 - 16, alignof(int));
	CHECK(test.ptr == nullptr);
	for(int i = 0; i < 1000; ++i)
		CHECK(nums[i] == i);
	mn::buf_free(nums);
	mn::allocator_free(buddy);
}

TEST_CASE("handle table generation check")
{
	auto table = mn::handle_table_new<int>();
	auto handles = mn::buf_new<uint64_t>();
	for(int i = 0; i < 10; ++i)
		mn::buf_push(handles, mn::handle_table_insert(table, i));

	for(int i = 0; i < 10; ++i)
	{
		CHECK(mn::handle_table_get(table, handles[i]) == i);
		mn::handle_table_remove(table, handles[i]);
	}

	for(int i = 0; i < 10; ++i)
	{
		auto new_handle = mn::handle_table_insert(table, i);
		CHECK(new_handle != handles[i]);
	}

	mn::handle_table_free(table);
	mn::buf_free(handles);
}

TEST_CASE("handle table generation check")
{
	auto table = mn::handle_table_new<int>();
	auto handles = mn::buf_new<uint64_t>();
	for(int i = 0; i < 10; ++i)
		mn::buf_push(handles, mn::handle_table_insert(table, i));

	for(int i = 0; i < 10; ++i)
		if (i % 2 == 0)
			mn::handle_table_remove(table, handles[i]);

	for(size_t i = 0; i < handles.count; ++i)
	{
		if (handles[i] % 2 == 0)
		{
			mn::buf_remove(handles, i);
			--i;
		}
	}

	for(int i = 0; i < 10; ++i)
	{
		if (i % 2 == 0)
		{
			auto new_handle = mn::handle_table_insert(table, i);
			mn::buf_push(handles, new_handle);
		}
	}

	mn::handle_table_free(table);
	mn::buf_free(handles);
}

TEST_CASE("zero init buf")
{
	mn::Buf<int> nums{};
	for (int i = 0; i < 10; ++i)
		mn::buf_push(nums, i);
	for (int i = 0; i < 10; ++i)
		CHECK(nums[i] == i);
	mn::buf_free(nums);

	mn::Buf<int> nums2{};
	mn::buf_free(nums2);
}

TEST_CASE("zero init map")
{
	mn::Map<int, bool> table{};
	mn::map_insert(table, 1, true);
	CHECK(mn::map_lookup(table, 1)->value == true);
	mn::map_free(table);

}

TEST_CASE("uuid uniqueness")
{
	auto ids = mn::map_new<mn::UUID, size_t>();
	mn_defer(mn::map_free(ids));

	for (size_t i = 0; i < 1000000; ++i)
	{
		auto id = mn::uuid_generate();
		if(auto it = mn::map_lookup(ids, id))
			++it->value;
		else
			mn::map_insert(ids, id, size_t(1));
	}
	CHECK(ids.count == 1000000);
}

TEST_CASE("uuid parsing")
{
	SUBCASE("Case 01")
	{
		auto id = mn::uuid_generate();
		auto variant = uuid_variant(id);
		auto version = uuid_version(id);
		auto id_str = mn::str_tmpf("{}", id);
		auto [id2, err] = mn::uuid_parse(id_str);
		CHECK(err == false);
		CHECK(id == id2);
		auto id2_str = mn::str_tmpf("{}", id2);
		CHECK(id2_str == id_str);
	}

	SUBCASE("Case 02")
	{
		auto [id, err] = mn::uuid_parse("this is not a uuid");
		CHECK(err == true);
	}

	SUBCASE("Case 03")
	{
		auto [id, err] = mn::uuid_parse("62013B88-FA54-4008-8D42-F9CA4889e0B5");
		CHECK(err == false);
	}

	SUBCASE("Case 04")
	{
		auto [id, err] = mn::uuid_parse("62013BX88-FA54-4008-8D42-F9CA4889e0B5");
		CHECK(err == true);
	}

	SUBCASE("Case 05")
	{
		auto [id, err] = mn::uuid_parse("{62013B88-FA54-4008-8D42-F9CA4889e0B5}");
		CHECK(err == false);
	}

	SUBCASE("Case 06")
	{
		auto [id, err] = mn::uuid_parse("62013B88,FA54-4008-8D42-F9CA4889e0B5");
		CHECK(err == true);
	}

	SUBCASE("Case 07")
	{
		auto [id, err] = mn::uuid_parse("62013B88-FA54-4008-8D42-F9CA4889e0B5AA");
		CHECK(err == true);
	}

	SUBCASE("Case 08")
	{
		auto nil_str = mn::str_tmpf("{}", mn::null_uuid);
		CHECK(nil_str == "00000000-0000-0000-0000-000000000000");
	}

	SUBCASE("Case 09")
	{
		auto [id, err] = mn::uuid_parse("00000000-0000-0000-0000-000000000000");
		CHECK(err == false);
		CHECK(id == mn::null_uuid);
	}
}

TEST_CASE("report simd")
{
	auto simd = mn_simd_support_check();
	mn::print("sse: {}\n", simd.sse_supportted);
	mn::print("sse2: {}\n", simd.sse2_supportted);
	mn::print("sse3: {}\n", simd.sse3_supportted);
	mn::print("sse4.1: {}\n", simd.sse4_1_supportted);
	mn::print("sse4.2: {}\n", simd.sse4_2_supportted);
	mn::print("sse4a: {}\n", simd.sse4a_supportted);
	mn::print("sse5: {}\n", simd.sse5_supportted);
	mn::print("avx: {}\n", simd.avx_supportted);
}

TEST_CASE("json support")
{
	auto json = R"""(
		{
			"name": "my name is \"mostafa\"",
			"x": null,
			"y": true,
			"z": false,
			"w": 213.123,
			"a": [
				1, false
			],
			"subobject": {
				"name": "subobject"
			}
		}
	)""";

	auto [v, err] = mn::json::parse(json);
	CHECK(err == false);
	auto v_str = str_tmpf("{}", v);
	auto expected = R"""({"name":"my name is \"mostafa\"", "x":null, "y":true, "z":false, "w":213.123, "a":[1, false], "subobject":{"name":"subobject"}})""";
	CHECK(v_str == expected);
	mn::json::value_free(v);
}