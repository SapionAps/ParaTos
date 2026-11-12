add_executable(posix_memory memory.c
	${PROJECT_SOURCE_DIR}/memory.c
	${PROJECT_SOURCE_DIR}/gemdos/process.c
	${PROJECT_SOURCE_DIR}/host/posix/memory.c)
target_include_directories(posix_memory PRIVATE ${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/mushashi ${PROJECT_SOURCE_DIR}/host/posix)
target_compile_definitions(posix_memory PRIVATE _GNU_SOURCE)
target_compile_options(posix_memory PRIVATE -std=c11 -UNDEBUG)
add_test(NAME posix_memory COMMAND posix_memory)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
	add_test(NAME posix_launch COMMAND ${Python3_EXECUTABLE}
		${CMAKE_CURRENT_SOURCE_DIR}/launch.py $<TARGET_FILE:paratos>)
	set_tests_properties(posix_launch PROPERTIES TIMEOUT 30)
	add_test(NAME posix_pexec COMMAND ${Python3_EXECUTABLE}
		${CMAKE_CURRENT_SOURCE_DIR}/pexec.py $<TARGET_FILE:paratos>)
	set_tests_properties(posix_pexec PROPERTIES TIMEOUT 60)
endif()
