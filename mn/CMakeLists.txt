cmake_minimum_required(VERSION 3.10)

# list the header files
set(HEADER_FILES
	include/mn/memory/Arena.h
	include/mn/memory/CLib.h
	include/mn/memory/Interface.h
	include/mn/memory/Leak.h
	include/mn/memory/Stack.h
	include/mn/memory/Virtual.h
	include/mn/Base.h
	include/mn/Buf.h
	include/mn/Debug.h
	include/mn/Defer.h
	include/mn/Exports.h
	include/mn/File.h
	include/mn/IO.h
	include/mn/Map.h
	include/mn/Memory.h
	include/mn/Memory_Stream.h
	include/mn/OS.h
	include/mn/Pool.h
	include/mn/Reader.h
	include/mn/Ring.h
	include/mn/Str.h
	include/mn/Str_Intern.h
	include/mn/Stream.h
	include/mn/Thread.h
	include/mn/Virtual_Memory.h
)

# list the source files
set(SOURCE_FILES
	src/mn/memory/Arena.cpp
	src/mn/memory/CLib.cpp
	src/mn/memory/Leak.cpp
	src/mn/memory/Stack.cpp
	src/mn/memory/Virtual.cpp
	src/mn/Base.cpp
	src/mn/Memory.cpp
	src/mn/Memory_Stream.cpp
	src/mn/OS.cpp
	src/mn/Pool.cpp
	src/mn/Reader.cpp
	src/mn/Str.cpp
	src/mn/Str_Intern.cpp
	src/mn/Stream.cpp
)

# list os specfic files
if(WIN32)
	set(SOURCE_FILES ${SOURCE_FILES} 
		src/mn/winos/Debug.cpp
		src/mn/winos/File.cpp
		src/mn/winos/Thread.cpp
		src/mn/winos/Virtual_Memory.cpp
	)
elseif(LINUX)
	set(SOURCE_FILES ${SOURCE_FILES} 
		src/mn/linux/Debug.cpp
		src/mn/linux/File.cpp
		src/mn/linux/Thread.cpp
		src/mn/linux/Virtual_Memory.cpp
	)
endif()


# list tool files
set(TOOL_FILES)
if(MSVC)
	set(TOOL_FILES ${TOOL_FILES}
		tools/vs/mn.natvis
	)
endif(MSVC)


# add library target
add_library(mn
	${HEADER_FILES}
	${SOURCE_FILES}
)

add_library(MoustaphaSaad::mn ALIAS mn)


# Windows specfic flags to enable W variations of winapi
if(WIN32)
	target_compile_definitions(mn
		PRIVATE
			UNICODE=1
			_UNICODE=1
	)
endif(WIN32)


# enable C++17
# disable any compiler specifc extensions
# add d suffix in debug mode
target_compile_features(mn PUBLIC cxx_std_17)
set_target_properties(mn PROPERTIES
	CXX_EXTENSIONS OFF
	DEBUG_POSTFIX d
)

# generate exports header file
include(GenerateExportHeader)
generate_export_header(mn
	EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/include/mn/Exports.h
)

# list include directories
target_include_directories(mn
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)


# output directory
set_target_properties(mn
	PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)