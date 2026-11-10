set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
if(NOT PTOS_LIBCMINI_ROOT)
	message(FATAL_ERROR "PTOS_LIBCMINI_ROOT must name a pTOS-capable libcmini checkout")
endif()
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include
	OUTPUT_VARIABLE GCC_BUILTIN_INCLUDE OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CMAKE_C_FLAGS_INIT "-mx32 -ffreestanding -fno-builtin -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -nostdinc -isystem ${PTOS_LIBCMINI_ROOT}/include -isystem ${GCC_BUILTIN_INCLUDE}")
set(PARATOS_PTOS ON CACHE BOOL "Build pTOS backend")
# Propagate the sysroot selector to CMake's compiler-check subproject.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES PTOS_LIBCMINI_ROOT)
