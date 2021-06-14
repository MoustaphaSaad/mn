cmake_minimum_required(VERSION 3.9)

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
		nanobench::nanobench
)

# add doctest folder
target_include_directories(mn_unittest
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/doctest
)

target_compile_options(mn_unittest
	PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)