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
find_program(PTOS_M68K_CC NAMES m68k-atari-mintelf-gcc m68k-unknown-ptos-prgelf-gcc)
if(NOT PTOS_M68K_CC)
	message(FATAL_ERROR "The real-kernel fixtures need a MiNT ELF or pTOS m68k compiler")
endif()
get_filename_component(PTOS_M68K_CC_NAME "${PTOS_M68K_CC}" NAME)
if(PTOS_M68K_CC_NAME STREQUAL "m68k-unknown-ptos-prgelf-gcc")
	set(PTOS_M68K_LINK_FORMAT -mmintelf)
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
if(PTOS_VDI_LIBRARY)
	target_compile_definitions(ptos_kernel_launcher PRIVATE PTOS_TEST_VDI_AVAILABLE=1)
endif()
ptos_executable(ptos_kernel_launcher 0x00400000)
add_executable(ptos_fs_native ${PROJECT_SOURCE_DIR}/tests/ptos/fs_native.c)
ptos_executable(ptos_fs_native 0x00400000)
add_executable(ptos_native_child ${PROJECT_SOURCE_DIR}/tests/ptos/native_child.c)
ptos_executable(ptos_native_child 0x00400000)

# Kernel options independently control the native launcher, the Pexec(200)
# fixture, and the guest's staged PROCESSes.
file(STRINGS "${PTOS_KERNEL_DIR}/obj/autoconf.h" PTOS_KERNEL_OPTIONS
	REGEX "^#define CONF_WITH_(MULTITHREADING|LEGACY_PEXEC|EXEC_REPLACE|PEXEC200|PCLONE|PROCESS_FILES|NATIVE_SCHED) [01]$")
if("#define CONF_WITH_LEGACY_PEXEC 0" IN_LIST PTOS_KERNEL_OPTIONS)
	set(PTOS_NATIVE_MODE_ZERO_ONLY ON)
	target_compile_definitions(ptos_kernel_launcher PRIVATE PTOS_TEST_NATIVE_MODE_ZERO_ONLY=1)
endif()
if("#define CONF_WITH_EXEC_REPLACE 1" IN_LIST PTOS_KERNEL_OPTIONS AND
	NOT "#define CONF_WITH_PEXEC200 0" IN_LIST PTOS_KERNEL_OPTIONS)
	set(PTOS_EXEC_REPLACEMENT ON)
	target_compile_definitions(ptos_kernel_launcher PRIVATE PTOS_TEST_EXEC_REPLACEMENT=1)
endif()
if("#define CONF_WITH_PROCESS_FILES 1" IN_LIST PTOS_KERNEL_OPTIONS AND
	("#define CONF_WITH_MULTITHREADING 1" IN_LIST PTOS_KERNEL_OPTIONS OR
	 ("#define CONF_WITH_PCLONE 1" IN_LIST PTOS_KERNEL_OPTIONS AND
	  "#define CONF_WITH_NATIVE_SCHED 1" IN_LIST PTOS_KERNEL_OPTIONS)))
	set(PTOS_CLONED_GUESTS ON)
endif()
set(PTOS_FIXTURES cli semantics overlay fault interpreter filesystem fs_overlay vdi vdi_overlay)
if(PTOS_CLONED_GUESTS)
	list(APPEND PTOS_FIXTURES pclone_guest)
	add_executable(ptos_pclone_launcher ${PROJECT_SOURCE_DIR}/tests/ptos/pclone_launcher.c)
	ptos_executable(ptos_pclone_launcher 0x00400000)
endif()

set(PTOS_FIXTURES_DIR ${CMAKE_CURRENT_BINARY_DIR}/guest-fixtures)
set(PTOS_GUEST_OUTPUTS)
foreach(fixture IN LISTS PTOS_FIXTURES)
	set(fixture_definitions)
	if(PTOS_EXEC_REPLACEMENT AND fixture STREQUAL "semantics")
		set(fixture_definitions -DPTOS_TEST_EXEC_REPLACEMENT=1)
	elseif(PTOS_CLONED_GUESTS AND fixture STREQUAL "fs_overlay")
		set(fixture_definitions -DPTOS_TEST_CLONED_GUESTS=1)
	endif()
	set(output ${PTOS_FIXTURES_DIR}/${fixture}.tos)
	add_custom_command(OUTPUT ${output}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${PTOS_FIXTURES_DIR}
		COMMAND ${PTOS_M68K_CC} -std=c11 -m68000 -Os -fno-builtin ${fixture_definitions}
			-Wall -Wextra -Werror -nostdlib ${PTOS_M68K_LINK_FORMAT} -I${PTOS_LIBCMINI_ROOT}/include
			${PTOS_GUEST_LIBCMINI_DIR}/objs/crt0.o
			${PROJECT_SOURCE_DIR}/tests/ptos/${fixture}.c
			${PTOS_GUEST_LIBCMINI_DIR}/libcmini.a -lgcc -o ${output}
		DEPENDS ${PROJECT_SOURCE_DIR}/tests/ptos/${fixture}.c
			${PROJECT_SOURCE_DIR}/tests/ptos/vdi_fixture.h
			${PTOS_GUEST_LIBCMINI_DIR}/objs/crt0.o ${PTOS_GUEST_LIBCMINI_DIR}/libcmini.a
		VERBATIM)
	list(APPEND PTOS_GUEST_OUTPUTS ${output})
endforeach()
add_custom_target(ptos_guest_fixtures ALL DEPENDS ${PTOS_GUEST_OUTPUTS})
add_test(NAME ptos_kernel
	COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/ptos/run_kernel.py
		--arch ${PTOS_TEST_ARCH} --kernel-dir ${PTOS_KERNEL_DIR}
		--paratos $<TARGET_FILE:paratos> --launcher $<TARGET_FILE:ptos_kernel_launcher>
		--native-child $<TARGET_FILE:ptos_native_child> --filesystem-native $<TARGET_FILE:ptos_fs_native> --fixtures ${PTOS_FIXTURES_DIR}
		--qemu ${PTOS_TEST_QEMU} --ovmf ${PTOS_OVMF_DIR}
		--qemu-data "${PTOS_QEMU_DATA_DIR}"
		--output ${CMAKE_CURRENT_BINARY_DIR}/kernel-test)
set_tests_properties(ptos_kernel PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)

if(TARGET ptos_pclone_launcher)
	add_test(NAME ptos_kernel_pclone
		COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/ptos/run_kernel.py
			--arch ${PTOS_TEST_ARCH} --kernel-dir ${PTOS_KERNEL_DIR} --pclone
			--paratos $<TARGET_FILE:paratos> --launcher $<TARGET_FILE:ptos_pclone_launcher>
			--native-child $<TARGET_FILE:ptos_native_child> --filesystem-native $<TARGET_FILE:ptos_fs_native>
			--fixtures ${PTOS_FIXTURES_DIR} --qemu ${PTOS_TEST_QEMU} --ovmf ${PTOS_OVMF_DIR}
			--qemu-data "${PTOS_QEMU_DATA_DIR}" --output ${CMAKE_CURRENT_BINARY_DIR}/kernel-pclone-test)
	set_tests_properties(ptos_kernel_pclone PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
endif()

if(PTOS_VDI_LIBRARY)
	# Exercise the optional no-client build too, using the same VDI-enabled
	# kernel. Only six VDI launches are needed to check the fallback path.
	get_target_property(PTOS_NO_VDI_SOURCES paratos SOURCES)
	add_executable(ptos_no_vdi ${PTOS_NO_VDI_SOURCES})
	target_include_directories(ptos_no_vdi PRIVATE ${PROJECT_SOURCE_DIR}/mushashi)
	target_link_libraries(ptos_no_vdi PRIVATE mushashi)
	ptos_executable(ptos_no_vdi 0x41000000)
	add_executable(ptos_no_vdi_launcher ${PROJECT_SOURCE_DIR}/tests/ptos/kernel_launcher.c)
	target_compile_definitions(ptos_no_vdi_launcher PRIVATE PTOS_TEST_VDI_ONLY=1)
	if(PTOS_NATIVE_MODE_ZERO_ONLY)
		target_compile_definitions(ptos_no_vdi_launcher PRIVATE PTOS_TEST_NATIVE_MODE_ZERO_ONLY=1)
	endif()
	ptos_executable(ptos_no_vdi_launcher 0x00400000)
	add_test(NAME ptos_kernel_no_vdi
		COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/ptos/run_kernel.py
			--arch ${PTOS_TEST_ARCH} --kernel-dir ${PTOS_KERNEL_DIR}
			--paratos $<TARGET_FILE:ptos_no_vdi> --launcher $<TARGET_FILE:ptos_no_vdi_launcher>
			--native-child $<TARGET_FILE:ptos_native_child> --filesystem-native $<TARGET_FILE:ptos_fs_native>
			--fixtures ${PTOS_FIXTURES_DIR} --qemu ${PTOS_TEST_QEMU} --ovmf ${PTOS_OVMF_DIR}
			--qemu-data "${PTOS_QEMU_DATA_DIR}"
			--output ${CMAKE_CURRENT_BINARY_DIR}/kernel-no-vdi-test)
	set_tests_properties(ptos_kernel_no_vdi PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
endif()

if(PTOS_M68K_TEST_DISK)
	add_executable(ptos_disk_runner ${PROJECT_SOURCE_DIR}/tests/ptos/disk_runner.c)
	ptos_executable(ptos_disk_runner 0x00400000)
	add_test(NAME ptos_m68k_disk
		COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/ptos/run_kernel.py
			--arch ${PTOS_TEST_ARCH} --kernel-dir ${PTOS_KERNEL_DIR}
			--paratos $<TARGET_FILE:paratos> --launcher $<TARGET_FILE:ptos_disk_runner>
			--native-child $<TARGET_FILE:ptos_native_child> --filesystem-native $<TARGET_FILE:ptos_fs_native> --fixtures ${PTOS_FIXTURES_DIR}
			--qemu ${PTOS_TEST_QEMU} --ovmf ${PTOS_OVMF_DIR}
			--qemu-data "${PTOS_QEMU_DATA_DIR}" --test-disk ${PTOS_M68K_TEST_DISK}
			--output ${CMAKE_CURRENT_BINARY_DIR}/m68k-disk-test)
	set_tests_properties(ptos_m68k_disk PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
endif()
