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