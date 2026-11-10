set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
if(NOT PTOS_LIBCMINI_ROOT)
	message(FATAL_ERROR "PTOS_LIBCMINI_ROOT must name a pTOS-capable libcmini checkout")
endif()
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=include
	OUTPUT_VARIABLE GCC_BUILTIN_INCLUDE OUTPUT_STRIP_TRAILING_WHITESPACE)
# ARMv6/VFP hard-float matches pTOS/libcmini and runs on Pi 1 and Cortex-A7.
# Word relocations avoid MOVW/MOVT relocation pairs in the native ELF loader.
set(PTOS_ARM_ABI "-marm -march=armv6 -mfpu=vfp -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT "${PTOS_ARM_ABI} -mword-relocations -ffreestanding -fno-builtin -fno-pie -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -nostdinc -isystem ${PTOS_LIBCMINI_ROOT}/include -isystem ${GCC_BUILTIN_INCLUDE}")
set(CMAKE_ASM_FLAGS_INIT "${PTOS_ARM_ABI}")
set(PARATOS_PTOS ON CACHE BOOL "Build pTOS backend")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES PTOS_LIBCMINI_ROOT)
