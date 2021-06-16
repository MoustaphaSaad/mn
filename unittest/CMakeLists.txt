cmake_minimum_required(VERSION 3.16)

# list source files
set(SOURCE_FILES
	src/unittest_mn.cpp
	src/unittest_main.cpp
)

# add executable target
add_executable(mn_unittest
	${SOURCE_FILES}
)

target_link_libraries(mn_unittest
	PRIVATE
		MoustaphaSaad::mn
		doctest::doctest
		nanobench::nanobench
)

target_compile_options(mn_unittest
	PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)