cmake_minimum_required(VERSION 3.9)

add_executable(example-replace example-replace.cpp)
target_link_libraries(example-replace
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(example-cut example-cut.cpp)
target_link_libraries(example-cut
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(example-cat example-cat.cpp)
target_link_libraries(example-cat
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(example-freq example-freq.cpp)
target_link_libraries(example-freq
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(mndoc mndoc.cpp)
target_link_libraries(mndoc
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(example-worker example-worker.cpp)
target_link_libraries(example-worker
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(example-echo-server example-echo-server.cpp)
target_link_libraries(example-echo-server
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(example-echo-client example-echo-client.cpp)
target_link_libraries(example-echo-client
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(example-file-lock-increment example-file-lock-increment.cpp)
target_link_libraries(example-file-lock-increment
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(example-file-lock-reset example-file-lock-reset.cpp)
target_link_libraries(example-file-lock-reset
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(example-sputnik-echo-server example-sputnik-echo-server.cpp)
target_link_libraries(example-sputnik-echo-server
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(example-sputnik-echo-client example-sputnik-echo-client.cpp)
target_link_libraries(example-sputnik-echo-client
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(mn_playground playground.cpp)
target_link_libraries(mn_playground
	PRIVATE
		MoustaphaSaad::mn
)

add_library(example-hot-reload-lib SHARED
	example-hot-reload-lib.h
	example-hot-reload-lib.cpp
)
target_link_libraries(example-hot-reload-lib
	PRIVATE
		MoustaphaSaad::mn
)
# generate exports header file
include(GenerateExportHeader)
generate_export_header(example-hot-reload-lib
	EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/example-hot-reload-lib-exports.h
)


add_executable(example-hot-reload-app example-hot-reload-app.cpp)
target_link_libraries(example-hot-reload-app
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(example-deadlock example-deadlock.cpp)
target_link_libraries(example-deadlock
	PRIVATE
		MoustaphaSaad::mn
)

add_executable(example-deadlock-recursive example-deadlock-recursive.cpp)
target_link_libraries(example-deadlock-recursive
	PRIVATE
		MoustaphaSaad::mn
)
