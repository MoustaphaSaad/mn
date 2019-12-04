cmake_minimum_required(VERSION 3.9)

# list the header files
set(HEADER_FILES
	include/mn/memory/Arena.h
	include/mn/memory/CLib.h
	include/mn/memory/Interface.h
	include/mn/memory/Leak.h
	include/mn/memory/Stack.h
	include/mn/memory/Virtual.h
	include/mn/memory/Fast_Leak.h
	include/mn/Base.h
	include/mn/Buf.h
	include/mn/Debug.h
	include/mn/Defer.h
	include/mn/Exports.h
	include/mn/Path.h
	include/mn/Fixed_Buf.h
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
	include/mn/Rune.h
	include/mn/Context.h
	include/mn/Task.h
	include/mn/Fmt.h
	include/mn/Read.h
	include/mn/Deque.h
	include/mn/Result.h
	include/mn/IPC.h
	include/utf8proc/utf8proc.h
)

# list the source files
set(SOURCE_FILES
	src/mn/memory/Arena.cpp
	src/mn/memory/CLib.cpp
	src/mn/memory/Leak.cpp
	src/mn/memory/Stack.cpp
	src/mn/memory/Virtual.cpp
	src/mn/memory/Fast_Leak.cpp
	src/mn/Base.cpp
	src/mn/Memory_Stream.cpp
	src/mn/OS.cpp
	src/mn/Pool.cpp
	src/mn/Reader.cpp
	src/mn/Str.cpp
	src/mn/Str_Intern.cpp
	src/mn/Stream.cpp
	src/mn/Rune.cpp
	src/mn/Context.cpp
	src/utf8proc/utf8proc.cpp
)

# list os specfic files
if(WIN32)
	set(SOURCE_FILES ${SOURCE_FILES} 
		src/mn/winos/Debug.cpp
		src/mn/winos/Path.cpp
		src/mn/winos/File.cpp
		src/mn/winos/Thread.cpp
		src/mn/winos/Virtual_Memory.cpp
		src/mn/winos/IPC.cpp
	)
elseif(UNIX)
	set(SOURCE_FILES ${SOURCE_FILES} 
		src/mn/linux/Debug.cpp
		src/mn/linux/Path.cpp
		src/mn/linux/File.cpp
		src/mn/linux/Thread.cpp
		src/mn/linux/Virtual_Memory.cpp
		src/mn/linux/IPC.cpp
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
	${TOOL_FILES}
)

add_library(MoustaphaSaad::mn ALIAS mn)

target_link_libraries(mn
	PUBLIC
		fmt::fmt
)

# make it reflect the same structure as the one on disk
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HEADER_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${TOOL_FILES})


# Windows specfic flags to enable W variations of winapi
if(WIN32)
	target_compile_definitions(mn
		PRIVATE
			UNICODE=1
			_UNICODE=1
		PUBLIC
			OS_WINDOWS=1
	)
	target_link_libraries(mn
		PRIVATE
			dbghelp
	)
	target_compile_options(mn
		PRIVATE
			/WX /W4
	)
elseif(UNIX)
	target_link_libraries(mn
		PRIVATE
			pthread rt
	)
	target_compile_definitions(mn
		PUBLIC
			OS_LINUX=1
	)
	target_compile_options(mn
		PRIVATE
			-Wall -Werror
	)
endif()

if(MN_LEAK)
	message(STATUS "Leak Detector enabled")
	target_compile_definitions(mn
		PRIVATE
			MN_LEAK=1
	)
endif(MN_LEAK)

# enable C++17
# disable any compiler specifc extensions
# add d suffix in debug mode
target_compile_features(mn PUBLIC cxx_std_17)
set_target_properties(mn PROPERTIES
	CXX_EXTENSIONS OFF
	DEBUG_POSTFIX d
)

# define debug macro
target_compile_definitions(mn PRIVATE "$<$<CONFIG:DEBUG>:DEBUG>")

# generate exports header file
include(GenerateExportHeader)
generate_export_header(mn
	EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/include/mn/Exports.h
)

# list include directories
target_include_directories(mn
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src
)
