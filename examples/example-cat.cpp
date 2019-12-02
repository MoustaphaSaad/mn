#include <mn/IO.h>
#include <mn/Str.h>
#include <mn/File.h>
#include <mn/Path.h>
#include <mn/Defer.h>

constexpr auto HELP_MSG = R"""(example-cat
a simple tool to concatenate files
'example-cat [FILE]...'
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