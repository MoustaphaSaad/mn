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