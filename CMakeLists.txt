cmake_minimum_required(VERSION 3.13)
project(ParaTos LANGUAGES C)

option(PARATOS_PTOS "Build the native pTOS backend (x32 or ARM)" OFF)
include(CTest)
if(PARATOS_PTOS)
	add_compile_definitions(PARATOS_PTOS=1)
endif()
add_subdirectory (mushashi)

if(PARATOS_PTOS)
	add_subdirectory(host/ptos)
	return()
endif()
add_subdirectory (gemdos)

if (CMAKE_C_COMPILER_ID MATCHES "Clang")
	add_compile_options(
		-Wno-sign-conversion
		-Wno-shorten-64-to-32
		-Wno-implicit-int-conversion
		-Wno-unused-parameter
		-Wno-pointer-sign
		-Wno-logical-op-parentheses
		-Wno-gnu-statement-expression-from-macro-expansion
		-Wno-unreachable-code-generic-assoc
		-Wno-parentheses
		-Wno-sign-compare
	)
endif ()

add_subdirectory(host/posix)

if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_subdirectory(tests/ptos)
endif()
