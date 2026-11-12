if(NOT PTOS_LIBCMINI_ROOT OR NOT PTOS_LIBCMINI_ARCHIVE OR NOT PTOS_CRT0)
	message(FATAL_ERROR "Set PTOS_LIBCMINI_ROOT, PTOS_LIBCMINI_ARCHIVE and PTOS_CRT0; see host/ptos/README.txt")
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	set(PTOS_ARCH_SOURCE x32/call.c)
	set(PTOS_VDI_SOURCE x32/vdi.c)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
	set(PTOS_ARCH_SOURCE arm/call.c)
	set(PTOS_VDI_SOURCE arm/vdi.c)
else()
	message(FATAL_ERROR "Unsupported pTOS architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()
function(ptos_executable target image_base)
	target_compile_features(${target} PRIVATE c_std_11)
	target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
	target_link_libraries(${target} PRIVATE ${PTOS_LIBCMINI_ARCHIVE})
	target_link_options(${target} PRIVATE -nostdlib -static -no-pie
		-Wl,--build-id=none,--emit-relocs,-z,noexecstack
		-Wl,-z,max-page-size=0x1000,-z,noseparate-code,-Ttext-segment=${image_base}
		${PTOS_CRT0})
	if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
		target_link_options(${target} PRIVATE -Wl,-m,elf32_x86_64)
	else()
		target_link_libraries(${target} PRIVATE gcc)
	endif()
	set_target_properties(${target} PROPERTIES SUFFIX ".prg")
endfunction()

set(PTOS_VDI_LIBRARY "" CACHE FILEPATH "Optional pTOS x32 libptos-vdi.a userspace client")
add_executable(paratos main.c arguments.c native.c ${PTOS_ARCH_SOURCE} ${PTOS_VDI_SOURCE}
	memory.c loader.c process.c files.c filesystem.c vdi.c
	${PROJECT_SOURCE_DIR}/vdi/vdi.c dispatch.c runtime.c)
target_include_directories(paratos PRIVATE ${PROJECT_SOURCE_DIR}/mushashi)
target_link_libraries(paratos PRIVATE mushashi)
if(PTOS_VDI_LIBRARY)
	if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR NOT EXISTS "${PTOS_VDI_LIBRARY}")
		message(FATAL_ERROR "PTOS_VDI_LIBRARY must name a built x32 libptos-vdi.a")
	endif()
	target_compile_definitions(paratos PRIVATE PARATOS_PTOS_VDI=1)
	target_link_libraries(paratos PRIVATE ${PTOS_VDI_LIBRARY})
endif()
ptos_executable(paratos 0x41000000)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
	if(BUILD_TESTING)
		enable_language(ASM)
		add_executable(ptos_arm_abi native.c ${PTOS_ARCH_SOURCE}
			${PROJECT_SOURCE_DIR}/tests/ptos/arm_abi.c
			${PROJECT_SOURCE_DIR}/tests/ptos/arm_abi.S)
		target_include_directories(ptos_arm_abi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
		target_compile_options(ptos_arm_abi PRIVATE -Wall -Wextra -Werror)
		target_link_options(ptos_arm_abi PRIVATE -nostdlib -static
			-Wl,--build-id=none,-z,noexecstack
			-T${PROJECT_SOURCE_DIR}/tests/ptos/arm_abi.ld)
		set_property(TARGET ptos_arm_abi APPEND PROPERTY LINK_DEPENDS
			${PROJECT_SOURCE_DIR}/tests/ptos/arm_abi.ld)
		find_program(PTOS_QEMU_ARM qemu-system-arm)
		if(PTOS_QEMU_ARM)
			add_test(NAME ptos_arm_abi COMMAND ${PTOS_QEMU_ARM}
				-M virt -cpu cortex-a7 -m 128 -nic none
				-display none -serial none -monitor none
				-semihosting-config enable=on,target=native
				-kernel $<TARGET_FILE:ptos_arm_abi>)
			set_tests_properties(ptos_arm_abi PROPERTIES TIMEOUT 15
				PASS_REGULAR_EXPRESSION "ARM pTOS SVC ABI: PASS"
				FAIL_REGULAR_EXPRESSION "FAIL")
		endif()
	endif()
endif()

set(PTOS_KERNEL_DIR "" CACHE PATH "Built pTOS checkout for optional real-kernel tests")
if(BUILD_TESTING AND PTOS_KERNEL_DIR)
	include(${PROJECT_SOURCE_DIR}/tests/ptos/kernel.cmake)
endif()
