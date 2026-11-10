# Optional integration with a separately built pTOS kernel. No kernel or
# toolchain downloads, and no changes to the kernel checkout, during CTest.
set(PTOS_GUEST_LIBCMINI_DIR "" CACHE PATH "Built m68k libcmini (libcmini.a and objs/crt0.o)")
set(PTOS_OVMF_DIR "/usr/share/OVMF" CACHE PATH "OVMF firmware directory for x32 tests")
set(PTOS_QEMU_DATA_DIR "" CACHE PATH "Optional QEMU firmware/ROM search directory")
set(PTOS_M68K_TEST_DISK "" CACHE FILEPATH "Optional pTOS m68k test-hd.img to run through Paratos")
if(NOT EXISTS "${PTOS_GUEST_LIBCMINI_DIR}/libcmini.a" OR
	NOT EXISTS "${PTOS_GUEST_LIBCMINI_DIR}/objs/crt0.o")
	message(FATAL_ERROR "Set PTOS_GUEST_LIBCMINI_DIR to a built m68k libcmini; see host/ptos/README.txt")
endif()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(PTOS_M68K_CC NAMES m68k-atari-mintelf-gcc)
if(NOT PTOS_M68K_CC)
	message(FATAL_ERROR "The real-kernel fixtures need m68k-atari-mintelf-gcc")
endif()
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
	set(PTOS_TEST_ARCH arm)
	find_program(PTOS_TEST_QEMU NAMES qemu-system-arm)
else()
	set(PTOS_TEST_ARCH x32)
	find_program(PTOS_TEST_QEMU NAMES qemu-system-x86_64)
endif()
if(NOT PTOS_TEST_QEMU)
	message(FATAL_ERROR "The real-kernel test needs QEMU for ${PTOS_TEST_ARCH}")
endif()

add_executable(ptos_kernel_launcher ${PROJECT_SOURCE_DIR}/tests/ptos/kernel_launcher.c)
ptos_executable(ptos_kernel_launcher 0x00400000)
add_executable(ptos_native_child ${PROJECT_SOURCE_DIR}/tests/ptos/native_child.c)
ptos_executable(ptos_native_child 0x00400000)

set(PTOS_FIXTURES_DIR ${CMAKE_CURRENT_BINARY_DIR}/guest-fixtures)
set(PTOS_GUEST_OUTPUTS)
foreach(fixture cli semantics overlay fault)
	set(output ${PTOS_FIXTURES_DIR}/${fixture}.tos)
	add_custom_command(OUTPUT ${output}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${PTOS_FIXTURES_DIR}
		COMMAND ${PTOS_M68K_CC} -std=c11 -m68000 -Os -fno-builtin
			-Wall -Wextra -Werror -nostdlib -I${PTOS_LIBCMINI_ROOT}/include
			${PTOS_GUEST_LIBCMINI_DIR}/objs/crt0.o
			${PROJECT_SOURCE_DIR}/tests/ptos/${fixture}.c
			${PTOS_GUEST_LIBCMINI_DIR}/libcmini.a -lgcc -o ${output}
		DEPENDS ${PROJECT_SOURCE_DIR}/tests/ptos/${fixture}.c
			${PTOS_GUEST_LIBCMINI_DIR}/objs/crt0.o ${PTOS_GUEST_LIBCMINI_DIR}/libcmini.a
		VERBATIM)
	list(APPEND PTOS_GUEST_OUTPUTS ${output})
endforeach()
add_custom_target(ptos_guest_fixtures ALL DEPENDS ${PTOS_GUEST_OUTPUTS})
add_test(NAME ptos_kernel
	COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/ptos/run_kernel.py
		--arch ${PTOS_TEST_ARCH} --kernel-dir ${PTOS_KERNEL_DIR}
		--paratos $<TARGET_FILE:paratos> --launcher $<TARGET_FILE:ptos_kernel_launcher>
		--native-child $<TARGET_FILE:ptos_native_child> --fixtures ${PTOS_FIXTURES_DIR}
		--qemu ${PTOS_TEST_QEMU} --ovmf ${PTOS_OVMF_DIR}
		--qemu-data "${PTOS_QEMU_DATA_DIR}"
		--output ${CMAKE_CURRENT_BINARY_DIR}/kernel-test)
set_tests_properties(ptos_kernel PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)

if(PTOS_M68K_TEST_DISK)
	add_executable(ptos_disk_runner ${PROJECT_SOURCE_DIR}/tests/ptos/disk_runner.c)
	ptos_executable(ptos_disk_runner 0x00400000)
	add_test(NAME ptos_m68k_disk
		COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/ptos/run_kernel.py
			--arch ${PTOS_TEST_ARCH} --kernel-dir ${PTOS_KERNEL_DIR}
			--paratos $<TARGET_FILE:paratos> --launcher $<TARGET_FILE:ptos_disk_runner>
			--native-child $<TARGET_FILE:ptos_native_child> --fixtures ${PTOS_FIXTURES_DIR}
			--qemu ${PTOS_TEST_QEMU} --ovmf ${PTOS_OVMF_DIR}
			--qemu-data "${PTOS_QEMU_DATA_DIR}" --test-disk ${PTOS_M68K_TEST_DISK}
			--output ${CMAKE_CURRENT_BINARY_DIR}/m68k-disk-test)
	set_tests_properties(ptos_m68k_disk PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
endif()
