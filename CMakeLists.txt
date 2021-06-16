cmake_minimum_required(VERSION 3.16)

project(mn VERSION 4.0.0 LANGUAGES CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_BUILD_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(FETCHCONTENT_BASE_DIR "${PROJECT_SOURCE_DIR}/_deps" CACHE STRING "put fetch content base dir outside of build folder")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(MN_BUILD_EXAMPLES    "Build example applications that showcase the mn library." ON)
option(MN_BUILD_TESTS       "Build mn unit tests."                                     ON)
option(MN_UNITY_BUILD       "Combine all mn source files into one jumbo build."        ON)
option(MN_LEAK              "Enables mn memory leak detection"                         OFF)
option(MN_DEADLOCK          "Enables mn deadlock detection"                            OFF)
option(MN_POOL_DOUBLE_FREE  "Enables mn pool double free check"                        OFF)
option(MN_SHARED            "Forces mn to build as a shared library"                   ON)
option(MN_ADDRESS_SANITIZER "Enables address sanitizer"                                OFF)
option(MN_THREAD_SANITIZER  "Enables thread sanitizer"                                 OFF)

if (MN_ADDRESS_SANITIZER AND MN_THREAD_SANITIZER)
	message(FATAL_ERROR "address sanitizer and thread sanitizer cannot run at the same time")
endif ()

if (MN_ADDRESS_SANITIZER)
	if (WIN32)
		message(FATAL_ERROR "windows doesn't support address sanitizer")
	else ()
		message(STATUS "feature: address sanitizer is enabled")
		add_compile_options(-fno-omit-frame-pointer -fsanitize=address)
		add_link_options(-fno-omit-frame-pointer -fsanitize=address)
	endif()
endif ()

if (MN_THREAD_SANITIZER)
	if (WIN32)
		message(FATAL_ERROR "windows doesn't support thread sanitizer")
	else ()
		message(STATUS "feature: thread sanitizer is enabled")
		add_compile_options(-fno-omit-frame-pointer -fsanitize=thread)
		add_link_options(-fno-omit-frame-pointer -fsanitize=thread)
	endif()
endif ()

# external dependencies
include(FetchContent)
FetchContent_Declare(
	fmt
	GIT_REPOSITORY https://github.com/fmtlib/fmt.git
	GIT_TAG        7.1.3
	GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(fmt)

add_subdirectory(mn)

if (MN_BUILD_TESTS)
	# unittests dependencies
	FetchContent_Declare(
		doctest
		GIT_REPOSITORY https://github.com/onqtam/doctest.git
		GIT_TAG        2.4.6
		GIT_PROGRESS   TRUE
	)
	FetchContent_Declare(
		nanobench
		GIT_REPOSITORY https://github.com/martinus/nanobench.git
		GIT_TAG        v4.3.5
		GIT_PROGRESS   TRUE
	)
	FetchContent_MakeAvailable(doctest nanobench)

	add_subdirectory(unittest)
endif()

if (MN_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()
