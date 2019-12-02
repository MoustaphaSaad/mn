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