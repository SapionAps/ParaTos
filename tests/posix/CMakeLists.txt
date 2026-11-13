add_executable(posix_memory memory.c
	${PROJECT_SOURCE_DIR}/memory.c
	${PROJECT_SOURCE_DIR}/gemdos/process.c
	${PROJECT_SOURCE_DIR}/host/posix/memory.c)
target_include_directories(posix_memory PRIVATE ${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/mushashi ${PROJECT_SOURCE_DIR}/host/posix)
target_compile_definitions(posix_memory PRIVATE _GNU_SOURCE)
target_compile_options(posix_memory PRIVATE -std=c11 -UNDEBUG)
add_test(NAME posix_memory COMMAND posix_memory)

add_executable(posix_terminal terminal.c ${PROJECT_SOURCE_DIR}/host/posix/terminal.c)
target_include_directories(posix_terminal PRIVATE ${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/mushashi ${PROJECT_SOURCE_DIR}/host/posix)
target_compile_definitions(posix_terminal PRIVATE _GNU_SOURCE)
target_compile_options(posix_terminal PRIVATE -std=c11 -UNDEBUG)
target_link_libraries(posix_terminal PRIVATE Threads::Threads)
add_test(NAME posix_terminal COMMAND posix_terminal)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
	add_test(NAME posix_launch COMMAND ${Python3_EXECUTABLE}
		${CMAKE_CURRENT_SOURCE_DIR}/launch.py $<TARGET_FILE:paratos>)
	set_tests_properties(posix_launch PROPERTIES TIMEOUT 30)
	add_test(NAME posix_pexec COMMAND ${Python3_EXECUTABLE}
		${CMAKE_CURRENT_SOURCE_DIR}/pexec.py $<TARGET_FILE:paratos>)
	set_tests_properties(posix_pexec PROPERTIES TIMEOUT 60)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	# Function-section GC permits focused host-runtime tests without a CPU loop.
	add_executable(posix_pid_registry pid_registry.c)
	target_include_directories(posix_pid_registry PRIVATE ${PROJECT_SOURCE_DIR}
		${PROJECT_SOURCE_DIR}/gemdos ${PROJECT_SOURCE_DIR}/mushashi ${PROJECT_SOURCE_DIR}/host/posix)
	target_compile_definitions(posix_pid_registry PRIVATE _GNU_SOURCE)
	target_compile_options(posix_pid_registry PRIVATE -std=c11 -UNDEBUG -ffunction-sections -fdata-sections)
	target_link_options(posix_pid_registry PRIVATE -Wl,--gc-sections)
	add_test(NAME posix_pid_registry COMMAND posix_pid_registry)
	add_executable(posix_io_staging io_staging.c ${PROJECT_SOURCE_DIR}/host/posix/tasks.c
		${PROJECT_SOURCE_DIR}/host/posix/file.c ${PROJECT_SOURCE_DIR}/host/posix/signals.c)
	target_include_directories(posix_io_staging PRIVATE ${PROJECT_SOURCE_DIR}
		${PROJECT_SOURCE_DIR}/gemdos ${PROJECT_SOURCE_DIR}/mushashi ${PROJECT_SOURCE_DIR}/host/posix)
	target_compile_definitions(posix_io_staging PRIVATE _GNU_SOURCE)
	target_compile_options(posix_io_staging PRIVATE -std=c11 -UNDEBUG -ffunction-sections -fdata-sections)
	target_link_options(posix_io_staging PRIVATE -Wl,--gc-sections,--wrap=read,--wrap=write,--wrap=pause,--wrap=sigsuspend)
	target_link_libraries(posix_io_staging PRIVATE Threads::Threads)
	add_test(NAME posix_io_staging COMMAND posix_io_staging)
	set_tests_properties(posix_pid_registry posix_io_staging PROPERTIES TIMEOUT 5)
endif()
