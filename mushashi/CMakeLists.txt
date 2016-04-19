project (mushashi)

set (M68KMAKE_SOURCES m68kmake.c)
set (M68KMAKE_INPUT m68k_in.c)
set (SOURCE_FILES m68kcpu.c m68kdasm.c)
set (GEN_SOURCE_FILES m68kops.c m68kopnz.c m68kopdm.c m68kopac.c)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} )

add_executable(m68kmake ${M68KMAKE_SOURCES})
add_custom_command (
  OUTPUT ${GEN_SOURCE_FILES}
  COMMAND m68kmake . ${CMAKE_CURRENT_SOURCE_DIR}/${M68KMAKE_INPUT}
  DEPENDS m68kmake
  )

add_library (mushashi STATIC ${SOURCE_FILES} ${GEN_SOURCE_FILES})
