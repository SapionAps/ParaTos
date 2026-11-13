project (mushashi)

set (M68KMAKE_SOURCES m68kmake.c)
set (M68KMAKE_INPUT m68k_in.c)
set (SOURCE_FILES m68kcpu.c m68kdasm.c)
set (GEN_SOURCE_FILES m68kops.c m68kopnz.c m68kopdm.c m68kopac.c)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} )

if(CMAKE_CROSSCOMPILING)
	find_program(PARATOS_HOST_CC NAMES cc gcc clang)
	if(NOT PARATOS_HOST_CC)
		message(FATAL_ERROR "A native C compiler is needed for m68kmake")
	endif()
	set(M68KMAKE_EXE ${CMAKE_CURRENT_BINARY_DIR}/m68kmake-host)
	add_custom_command(OUTPUT ${M68KMAKE_EXE}
		COMMAND ${PARATOS_HOST_CC} ${CMAKE_CURRENT_SOURCE_DIR}/m68kmake.c -o ${M68KMAKE_EXE}
		DEPENDS m68kmake.c VERBATIM)
else()
	add_executable(m68kmake ${M68KMAKE_SOURCES})
	set(M68KMAKE_EXE $<TARGET_FILE:m68kmake>)
endif()
add_custom_command (
  OUTPUT ${GEN_SOURCE_FILES}
  COMMAND ${M68KMAKE_EXE} . ${CMAKE_CURRENT_SOURCE_DIR}/${M68KMAKE_INPUT}
  DEPENDS ${M68KMAKE_EXE} ${M68KMAKE_INPUT}
  VERBATIM
  )

add_library (mushashi STATIC ${SOURCE_FILES} ${GEN_SOURCE_FILES})
target_compile_features(mushashi PUBLIC c_std_11)
foreach(src ${GEN_SOURCE_FILES})
	list(APPEND MUSASHI_GENERATED_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${src})
endforeach()
set(MUSASHI_GENERATED_SOURCES ${MUSASHI_GENERATED_SOURCES} PARENT_SCOPE)
