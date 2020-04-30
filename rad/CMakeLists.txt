cmake_minimum_required(VERSION 3.9)

# list the header files
set(HEADER_FILES
	include/rad/RAD.h
)

# list the source files
set(SOURCE_FILES
	src/rad/RAD.cpp
)

# add library target
add_library(rad
	${HEADER_FILES}
	${SOURCE_FILES}
)

add_library(MoustaphaSaad::rad ALIAS rad)

target_link_libraries(rad
	PRIVATE
		MoustaphaSaad::mn
)

# make it reflect the same structure as the one on disk
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HEADER_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_FILES})

# generate exports header file
include(GenerateExportHeader)
generate_export_header(rad
	EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/include/rad/Exports.h
)

# list include directories
target_include_directories(rad
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)
