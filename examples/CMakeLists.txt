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