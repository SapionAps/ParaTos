if(NOT PTOS_LIBCMINI_ROOT OR NOT PTOS_LIBCMINI_ARCHIVE OR NOT PTOS_CRT0)
	message(FATAL_ERROR "Set PTOS_LIBCMINI_ROOT, PTOS_LIBCMINI_ARCHIVE and PTOS_CRT0; see host/ptos/README.md")
endif()

add_executable(paratos main.c native.c memory.c loader.c process.c dispatch.c runtime.c)
target_compile_features(paratos PRIVATE c_std_11)
target_compile_options(paratos PRIVATE -Wall -Wextra -Werror)
target_include_directories(paratos PRIVATE ${PROJECT_SOURCE_DIR}/mushashi)
target_link_libraries(paratos PRIVATE mushashi ${PTOS_LIBCMINI_ARCHIVE})
target_link_options(paratos PRIVATE -nostdlib -static -no-pie
	-Wl,-m,elf32_x86_64,--build-id=none,--emit-relocs,-z,noexecstack
	-Wl,-z,max-page-size=0x1000,-z,noseparate-code,-Ttext-segment=0x41000000
	${PTOS_CRT0})
set_target_properties(paratos PROPERTIES SUFFIX ".prg")
