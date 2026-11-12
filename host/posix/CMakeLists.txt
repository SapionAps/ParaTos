# Linux and macOS share this backend; small platform differences stay here.
add_executable(paratos main.c executable.c memory.c bios.c
	cdev.c date.c dir.c file.c net.c path.c pexec.c proc.c signals.c sys.c tos_errors.c)
target_include_directories(paratos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(paratos PRIVATE gemdos mushashi)
target_compile_options(paratos PRIVATE -std=c11)
target_compile_definitions(paratos PRIVATE _GNU_SOURCE)
# Preserve the existing ./build/paratos CLI location.
set_target_properties(paratos PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

if(BUILD_TESTING)
	add_subdirectory(${PROJECT_SOURCE_DIR}/tests/posix ${PROJECT_BINARY_DIR}/tests/posix)
endif()
