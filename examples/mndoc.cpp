#include <mn/IO.h>
#include <mn/Path.h>
#include <mn/Defer.h>
#include <mn/Str.h>
#include <mn/Buf.h>

struct Doc_Element
{
	mn::Str doc_str;
	mn::Str doc_subject;
};

inline static Doc_Element
doc_element_new()
{
	return Doc_Element{
		mn::str_new(),
		mn::str_new()
	};
}

inline static void
doc_element_free(Doc_Element& self)
{
	mn::str_free(self.doc_str);
	mn::str_free(self.doc_subject);
}

inline static void
destruct(Doc_Element& self)
{
	doc_element_free(self);
}


inline static bool
is_header(const mn::Str& path)
{
	return mn::str_suffix(path, ".h") || mn::str_suffix(path, ".hpp");
}

inline static void
folder_list_headers(const mn::Str& path, mn::Buf<mn::Str>& out)
{
	auto entries = mn::path_entries(path);
	mn_defer(destruct(entries));

	for(auto entry: entries)
	{
		if (entry.name == "." || entry.name == "..")
			continue;

		auto folder_path = mn::str_tmpf("{}/{}", path, entry.name);
		if (entry.kind == mn::Path_Entry::KIND_FILE)
		{
			if(is_header(entry.name))
				mn::buf_push(out, clone(folder_path));
		}
		else if(entry.kind == mn::Path_Entry::KIND_FOLDER)
		{
			folder_list_headers(folder_path, out);
		}
	}
}

inline static void
header_doc(const mn::Str& path, mn::Buf<Doc_Element>& out)
{
	auto f = mn::file_open(path, mn::IO_MODE::READ, mn::OPEN_MODE::OPEN_ONLY);
	if (f == nullptr)
	{
		mn::printerr("could not open header file '{}'", path);
		return;
	}
	mn_defer(mn::file_close(f));

	auto r = mn::reader_new(f);
	mn_defer(mn::reader_free(r));

	auto element = doc_element_new();
	mn_defer(doc_element_free(element));

	auto line = mn::str_tmp();
	size_t paren_count = 0;
	while(mn::readln(r, line))
	{
		mn::str_trim(line);

		if (mn::str_prefix(line, "///"))
		{
			mn::str_trim_left(line, "/ ");
			element.doc_str = mn::strf(element.doc_str, "{}\n", line);
		}
		else if (element.doc_str.count > 0 && line.count > 0 && line != "{" && line != "}" && line != "};")
		{
			element.doc_subject = mn::strf(element.doc_subject, "{}\n", line);
		}
		else
		{
			// we found one
			if (element.doc_str.count > 0 && element.doc_subject.count > 0)
			{
				mn::buf_push(out, element);
				element = doc_element_new();
			}
			else
			{
				mn::str_clear(element.doc_str);
				mn::str_clear(element.doc_subject);
			}
		}

		mn::str_clear(line);
	}
}

int
main(int argc, char **argv)
{
	auto headers = mn::buf_new<mn::Str>();
	mn_defer(destruct(headers));

	for(int i = 1; i < argc; ++i)
	{
		auto path = mn::str_lit(argv[i]);
		if (mn::path_is_folder(path))
			folder_list_headers(path, headers);
		else if (mn::path_is_file(path) && is_header(path))
			mn::buf_push(headers, mn::str_from_c(path.ptr));
	}
	mn::memory::tmp()->free_all();

	auto docs = mn::buf_new<Doc_Element>();
	mn_defer(destruct(docs));

	for (auto header : headers)
		header_doc(header, docs);

	for(auto element: docs)
		mn::print("```\n{}```\n{}\n\n", element.doc_subject, element.doc_str);
	return 0;
}