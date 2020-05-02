cmake_minimum_required(VERSION 3.9)

project(mn VERSION 3.3.0 LANGUAGES CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MN_BUILD_EXAMPLES "Build example applications that showcase the mn library." ON)
option(MN_BUILD_TESTS    "Build mn unit tests." ON)

add_subdirectory(fmt-6.2.0 EXCLUDE_FROM_ALL)
add_subdirectory(mn)
add_subdirectory(rad)

if (MN_BUILD_TESTS)
    add_subdirectory(unittest)
endif()

if (MN_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
