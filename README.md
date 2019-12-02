# MN Tour

[![Build Status](https://travis-ci.org/MoustaphaSaad/mn.svg?branch=master)](https://travis-ci.org/MoustaphaSaad/mn)

28, Nov, 2019

MN is a minimal support library that I use and helps me in my "Pure Coding" style.

I think the way C was intended is to build your own tools and abstractions on top of the language. The C standard library provides both an example and a bare minimum library to use in your application. You need to build your own tools and abstractions that suites you and the programs you intend to build. Feel free to take it change it to suite your needs, also if you wanted to add new features to it then send them to me and I'll see what I can do.

You can communicate with me through e-mail moustapha.saad.abdelhamed@gmail.com

The library is licensed under BSD-3 and you can find it here [MN](https://github.com/MoustaphaSaad/mn/)

## Coding Style

The coding style is simple. At its core is these principle

- Data and code should be separate things.
- Data should be dumb/simple/not smart.
- Code edits data.

Given the above points you can conclude that I don't really use C++ OOP features -since the whole point of OOP is to make data "smart" by attaching behavior to it- I just use C structs and functions.

If the struct has reference types like `Buf` , `Str`, `Map`, etc... I tend to make a new function and a free function for it. 

```C++
struct Person
{
    mn::Str name;
};

Person person_new(mn::Str name) { return Person{name}; }
void person_free(Person& self) { mn::str_free(self.name); }

// i also define an overload for the generic destruct function
// this function is used if you call destruct on Buf<Person> for example
void destruct(Person& self) { person_free(self); };

// when i need to clone the type i do the same thing
Person person_clone(const Person& self) { return person_new(clone(self.name)); }
// i also define an overload for the generic clone function
// this function is used if you call clone on Buf<Person> for example
Person clone(const Person& self) { return person_clone(self); }
```

As you can notice the style is pretty simple. Now let's explore the library a little bit.

Browse the header files for full documentation and to view the full interface nearly each function is documented in-line in the code.

## Memory

Let's talk about memory since it's the most used resource in any program

### Representation

Memory is represented in its simplest form is just a `void*` to the memory and a `size_t` of that region of memory

```C++
struct Block
{
	void*  ptr;
	size_t size;

	operator bool() const
	{
		return ptr != nullptr && size != 0;
	}
};
```

Allocation and freeing memory in its simplest form is done using

```C++
inline static Block
alloc(size_t size, uint8_t alignment);

inline static void
free(Block block);
```

All interaction in memory is done using `Block`s but what if you need to allocate a single instance of any type do you have to go through the above interface? and write

```C++
int* my_int = (int*)mn::alloc(sizeof(int), alignof(int)).ptr;
mn::free(Block{my_int, sizeof(*my_int)});
```

The answer is no, the following functions are provided to help with this use case

```C++
template<typename T>
inline static T*
alloc();

template<typename T>
inline static T*
alloc_zerod();

template<typename T>
inline static void
free(const T* ptr);
```

Given the above interface then you can write

```C++
int* my_int = mn::alloc_zerod<int>();
mn::free(my_int);
```

### Allocators

One of the problematic things about OOP is that everything is an object and this doesn't really work with small objects, let me explain.

Consider a graph system which has millions of nodes and each one of them is an independent object. This will introduce the problem of how to manage these objects?. The C++ answer will be through RAII. RAII is correct but it's tedious, complex, and non performant. The Java answer will be Garbage Collection. Again it's a correct answer but it's complex and non performant.

You see the real problem isn't how to manage the graph nodes, it's this part "millions of nodes and each one of them is an independent object". You as a human won't think about the nodes of the graph individually anyway.

The correct answer is based on the same principle of GC but you do it locally and by hand which eliminates the cost of a GC.

GC works by attaching small objects lifetime to a root object and when the root goes out of reach then all the small objects will be freed. We do that here using allocators. and all the types/functions that use memory are designed to play nicely with allocators

In the graph system we'll do a small allocator/pool in the graph struct and allocate nodes from it. At the end we can throw the allocator/pool away.

Let's talk about the memory system we have in mn now. All memory traffic goes through an allocator whether it's a custom allocator or a global one

```C++
//for example this is the clib allocator (malloc/free) this is the default allocator
inline Allocator clib_allocator = Allocator(-1);
```

Remember the above `alloc` and `free` functions those are actually based on the functions below

```C++
Block
alloc_from(Allocator allocator, size_t size, uint8_t alignment);

void
free_from(Allocator allocator, Block block);
```

They just use the `clib_allocator`

#### Allocator Types

There are currently three kinds of allocator

- **Stack Allocators**: a stack allocator that pumps a pointer into an internal stack and returns a nullptr when it runs out of memory (you can only free the last-element/top-of-stack) or reset the whole stack at once
- **Arena Allocators**: an arena is a list of stacks/buckets and it allocates memory in each stack/bucket until it's full then it adds another stack/bucket to the list (doesn't free anything inside on its own, you can only free the entire arena at once)
- **Tmp Allocators**: sometimes you don't know a clear owner of the memory or a clear owner doesn't exist at all and for this reason you can use the tmp allocator which is just a `thread_local` arena that you can only free on a regular basis in case you are the application, it's not recommended to call free if you are implementing a library because the application is the only place where you are sure that when you call free there will be no dangling reference pointing to the tmp memory.

### Example

```C++
// Allocators work in a stack manner where you can push and pop
// allocators to the stack and everything by default uses the top of the allocator stack
// here we add the leak detector allocator to be used by the code below
mn::allocator_push(mn::leak());

// you can allocate any type using this way but it allocate a single instance
auto p = mn::alloc<Point>();
p->x = 12;
p->y = 13;

//don't forget to free it
mn::free(p);


// also you can allocate a block of memory
// this allocates a 1 KB of memory
auto m = mn::alloc(1024, alignof(char));

// you can check the block using a simple if statement
if(m)
{
	// block is not empty
}

// don't forget to free it
mn::free(m);

// here we pop the leak detector from the allocator so we switch back to the default
// allocator which is the clib allocator
// go ahead and remove one of the free calls above to see the leak detection report:
// Leak size: 1024, call stack:
// [12]: mn::callstack_dump
// [11]: <lambda_df79aca8c65b7a47c69b16965c6f8a4c>::operator()
// [10]: <lambda_df79aca8c65b7a47c69b16965c6f8a4c>::<lambda_invoker_cdecl>
// [9]: mn::alloc_from
// [8]: mn::alloc
// [7]: mn_tour_stuff
// [6]: main
// [5]: invoke_main
// [4]: __scrt_common_main_seh
// [3]: __scrt_common_main
// [2]: mainCRTStartup
// [1]: BaseThreadInitThunk
// [0]: RtlUserThreadStart
// 
// Leaks count: 1, Leaks size(bytes): 1024
mn::allocator_pop();

// we can create an arena so that we can allocate nodes freely and don't
// worry about managing the lifetime of each node individually
// at the end all we have to do is just call `allocator_free` to free
// the arena itself and all the nodes that's allocated from it
auto arena = allocator_arena_new();

auto node = mn::alloc_from<Node>(arena);
node->data = 234;
node->next = nullptr;

// don't forget to free the arena
mn::allocator_free(arena);
```

### Pool

Let's create a memory pool of points

```C++
struct Point
{
	float x, y;
};


//we create a pool with the element size of sizeof(Point) and a bucket size of 1024 Points
auto pool = mn::pool_new(sizeof(Point), 1024);

//we get a point from the pool
auto p1 = (Point*)mn::pool_get(pool);
p1->x = 123;
p1->y = 34;

//put back the point into the pool
mn::pool_put(pool, p1);

//we get another point from the pool
auto p2 = (Point*)mn::pool_get(pool);
p2->x = 123;
p2->y = 34;

//don't forget to free the pool
mn::pool_free(pool);
```

## Buf

Buf is a dynamic c array implementation

```C++
auto numbers = mn::buf_new<int>();
for(int i = 0; i < 10; ++i)
	mn::buf_push(numbers, i);

// you can iterate over it using the simple form
for(int i = 0; i < numbers.count; ++i)
	mn::print("{}\n", numbers[i]);

// or you can use a range for loop
for(auto n: numbers)
	mn::print("{}\n", numbers[i]);

// remove single element from buf
mn::buf_remove(numbers, 0);

// remove all even integers
mn::buf_remove_of(numbers, [](auto n){ return n % 2 == 0; });

//don't forget to free it
mn::buf_free(numbers);

//you can also create a buf with a custom allocator
auto numbers = mn::buf_with_allocator<int>(custom_allocator);
```

## Str

Str is a `Buf<char>` specialization which maintains a null termination byte at the end also it's utf-8 string

```C++
auto name = mn::str_from_c("Mostafa");
mn::print("{}\n", name);

mn::str_push(name, u8" مصطفى");
mn::print("{}\n", name);

// you can iterator over the individual chars like the buf
for(size_t i = 0; i < name.count; ++i)
	mn::print("{}\n", name[i]);

// or you can iterate over the runes
for(const char* it = name.ptr;
	*it;
	it = mn::rune_next(it))
{
	auto c = mn::rune_read(it);
	mn::print("0x{:X}\n", c);
}

// don't forget to free the name
mn::str_free(name);
```

## Ring

Ring is a circular buffer used mainly as queue

```C++
auto q = mn::ring_new<int>();

// insert the elements into the q
for(int i = 0; i < 10; ++i)
{
	if(i % 2 == 0)
		mn::ring_push_back(q, i);
	else
		mn::ring_push_front(q, i);
}

// loop over the elements in the ring
for(size_t i = 0; i < q.count; ++i)
	mn::print("{}\n", q[i]);

// pop until the ring is empty
while(q.count)
{
	mn::print("front: {}, back: {}\n", mn::ring_front(q), mn::ring_back(q));
	mn::ring_pop_front(q);
}

mn::ring_free(q);
```

## Map

Map is an open-addressing hash map implementation

```C++
auto map = mn::map_new<Str, int>();

mn::map_reserve(map, 10);

for(int i = 0; i < 10; ++i)
{
    auto name = mn::strf("Mostafa {}", i);
	mn::map_insert(map, name, i);
}

// no we can iterate over the map
for(auto it = mn::map_begin(map);
	it != mn::map_end(map);
	it = mn::map_next(map, it))
{
	mn::print("{}: {}\n", it->key, it->value);
}

// we can remove elements from the map
mn::map_remove(map, mn::str_lit("Mostafa 0"));

// also we can do lookups
// str_lit is used to wrap the c string only (no allocation happens)
if(auto it = mn::map_lookup(map, mn::str_lit("Mostafa 0")))
	mn::print("{}: {}\n", it->key, it->value);

// no since we have a composite structure Map of Str if we call `map_free` we would be only 
// freeing the map and leaking all the strings so instead we will call destruct which will
// free the strings also
destruct(map);
```

## File

Let's load the binary content of a file

```C++
// here we open the file with read permission and we choose to open only mode
auto f = mn::file_open("D:/content.bin", mn::IO_MODE::READ, mn::OPEN_MODE::OPEN_ONLY);

// we check that the file is opened
if(mn::file_valid(f) == false)
	mn::panic("cannot read file");

// we allocate a block of exactly the same size as the file
auto content = mn::alloc(file_size(f), alignof(char));

// then we read the file into the block
size_t read_size = mn::file_read(f, content);
assert(read_size == content.size);

// don't forget to close the file
mn::file_close(f);
```

## File System Manipulation

Let's print all the file names inside a specific folder

```C++
auto content = mn::path_entries("D:/");

for(Path_Entry& entry: content)
	if(entry.kind == Path_Entry::KIND_FILE)
	{
		// DO AT YOUR OWN RISK
		// here we call str_from_c to create a tmp string with the base path
		// then we call path join to join the file name and the base path together
		// then we call the file remove function to delete the file from disk
		mn::file_remove(mn::str_tmpf("D:/{}", entry.name));
		mn::print("{}\n", entry.name);
	}

// don't forget to destruct the array
destruct(content);
```

## Memory_Stream

Let's create an in memory stream and print into it

```C++
// we create an in memory stream
auto stream = mn::memory_stream_new();

// print into the stream
mn::print_to(stream, "My name is {}, and my age is {}\n", "Mostafa", "25");

// extract the string from the string
auto str = mn::memory_stream_str(stream);

// we print the string content of the stream
mn::print("stream content: '{}'\n", str);

// don't forget to free the stream
mn::stream_free(stream);
mn::str_free(str);
```

## Reader

Let's read numbers from a file

```C++
// here we create a new reader that's based on a file stream we open
auto file = mn::file_open("D:/numbers.txt", mn::IO_MODE::READ, mn::OPEN_MODE::OPEN_ONLY);
auto reader = reader_new(file);

// while we can read numbers we print them
int num;
while(mn::vreads(reader, num))
	mn::print("{}\n", num);

mn::reader_free(reader);
mn::file_close(file);
```

## Str_Intern

A String interning is an operation in which all of the unique strings is stored once and every time a duplicate is encountered it returns a pointer to the same stored string it's used mainly to avoid string compare functions since all you have to do now is compare the string pointers if they are the same then they have the same content

```C++
auto intern = mn::str_intern_new();

const char* is = mn::str_intern(intern, "Mostafa");
if(is == mn::str_intern(intern, "Mostafa"))
	mn::print("They are the same pointer\n");

const char* big_str = "my name is Mostafa";
const char* begin = big_str + 11;
const char* end = begin + 7;
if(is == mn::str_intern(intern, begin, end));
	mn::print("we can intern sub strings too\n");

mn::str_intern_free(intern);
```

## Mutex

Let's use a mutex to protect some function from multiple thread access

```C++
auto mtx = mn::mutex_new();

//lock the mutex
mn::mutex_lock(mtx);
	//call the function
	foo();
//unlock the mutex
mn::mutex_unlock(mtx);

//don't forget to free the mutex
mn::mutex_free(mtx);
```

## Thread

Let's make a thread

```C++
void my_func(void* arg)
{
	printfmt("Hello, from another thread\n");
}

// we create a thread which will start in `my_func` with arg: `nullptr`
auto th = mn::thread_new(my_func, nullptr);
// we wait for the thread to finish
mn::thread_join(th);
// we destroy the thread
mn::thread_free(th);
```

## Defer

You'll notice from the above code that we put `buf_new` at the start and `buf_free` at the end of the code snippet, Enter Defer which is a way to tell the language to execute this code when we go out of scope

```C++
{
    auto numbers = mn::buf_with_count<int>(10);
    mn_defer(mn::buf_free(numbers));

    // do stuff with numbers
    
    // buf_free will be called when we exit from the scope
}
```

## Err

Sometimes your function may error and you'll need to communicate the error to the caller, Enter Err type

```C++
mn::Err
foo()
{
    auto r = rand();
    if (r % 2 == 0)
        return mn::Err {"the odds are against you, because r is '{}'", r};
    // return no error
    return mn::Err{};
}
```

## Result

Sometimes you'll need to return a value or an error instead of just the error, Enter Result type

```C++
// make a function that returns a result
Result<int>
my_div(int a, int b)
{
	if (b == 0)
    {
        // return an error 
		return Err{ "can't calc '{}/{}' because b is 0", a, b };
    }
	return a / b;
}

// then you can recieve it
auto [r, err] = my_div(4, 2);
if(err)
    mn::print("Error: {}", err);
mn::print("Ok: {}", r);
```

You can also use error codes type as error instead of the default Err type which is based around returning a string for errors and empty string for OK (no error)

```C++
enum class Err_Code {
    // the zero value has to be the OK value
    OK,
    ZERO_DIV
};

Result<int, Err_Code> my_div(int a, int b)
{
	if (b == 0)
		return Err_Code::ZERO_DIV;
	return a / b;
}

// then you can recieve it
auto [r, err] = my_div(4, 2);
if(err != Err_Code::OK)
  mn::print("Error");
mn::print("Ok: {}", r);
```

## Small Programs

Let's make small programs that illustrate the different ways you can use mn types

### Replace Program

let's make a small program that takes input from stdin replaces a certain word then prints it back to the stdout

```C++
#include <mn/IO.h>
#include <mn/Str.h>

constexpr auto HELP_MSG = R"""(example-replace
a simple tool to replace string with another string from stdin/stdout
'example-replace [search string] [replace string]'
)""";

int
main(int argc, char **argv)
{
	// ensure command line arguments are sent
	if (argc < 3)
	{
		mn::print(HELP_MSG);
		return -1;
	}

	// get search str from argv
	auto search_str	 = mn::str_from_c(argv[1], mn::memory::tmp());
	// get replace str from argv
	auto replace_str = mn::str_from_c(argv[2], mn::memory::tmp());

	// ensure search str is not empty
	if (search_str.count == 0)
	{
		mn::printerr("search string is empty!!!\n");
		return -1;
	}

	// create tmp string
	auto line	 = mn::str_tmp();

	// while we can read line
	while (mn::readln(line))
	{
		// replace the search str with replace str
		mn::str_replace(line, search_str, replace_str);
		// print the line back
		mn::print("{}\n", line);
	}

	return 0;
}
```

### Cut Program

Let's make a small program that will take input from stdin cut it into words and list them in stdout one word  at a time

```C++
#include <mn/IO.h>
#include <mn/Str.h>
#include <mn/Defer.h>

int
main()
{
	// create tmp string
	auto line	 = mn::str_new();
	mn_defer(mn::str_free(line));

	// while we can read line
	while (mn::readln(line))
	{
		// split words, which return a tmp Buf<Str>
		auto words = mn::str_split(line, " ", true);
		for (auto& word : words)
		{
			// trim the word
			mn::str_trim(word);
			// print it
			mn::print("{}\n", word);
		}
		// free all the tmp memory
		mn::memory::tmp()->free_all();
	}

	return 0;
}
```

### Cat Program

Let's make a small program like cat

```C++
#include <mn/IO.h>
#include <mn/Str.h>
#include <mn/File.h>
#include <mn/Path.h>
#include <mn/Defer.h>

constexpr auto HELP_MSG = R"""(example-cat
a simple tool to concatenate files
'example-cat [FILES]...'
)""";

int
main(int argc, char **argv)
{
	// loop over arguments
	for(int i = 1; i < argc; ++i)
	{
		// check if path is file
		if(mn::path_is_file(argv[i]) == false)
		{
			// print error message
			mn::printerr("{} is not a file\n", argv[i]);
			return -1;
		}

		// load file content
		auto content = mn::file_content_str(argv[i]);
		mn_defer(mn::str_free(content));

		// print it
		mn::print("{}", content);
	}

	return 0;
}
```

### Frequency Program

Let's make a program which counts the frequency of words in stdin and print the stats out at the end

```C++
#include <mn/IO.h>
#include <mn/Str.h>
#include <mn/Map.h>
#include <mn/Defer.h>

int
main()
{
	// create tmp string
	auto line	 = mn::str_new();
	mn_defer(mn::str_free(line));

	auto freq = mn::map_new<mn::Str, size_t>();
	mn_defer(destruct(freq));

	// while we can read line
	while (mn::readln(line))
	{
		// split words, which return a tmp Buf<Str>
		auto words = mn::str_split(line, " ", true);
		for (auto& word : words)
		{
			// trim the word
			mn::str_trim(word);
			if (auto it = mn::map_lookup(freq, word))
				it->value++;
			else
				mn::map_insert(freq, clone(word), size_t(1));
		}
		// free all the tmp memory
		mn::memory::tmp()->free_all();
	}

	for (auto it = mn::map_begin(freq); it != mn::map_end(freq); it = mn::map_next(freq, it))
		mn::print("{} -> {}\n", it->key, it->value);

	return 0;
}
```

