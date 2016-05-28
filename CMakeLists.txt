cmake_minimum_required(VERSION 3.2.2)
project(ParaTos)

add_subdirectory (mushashi)
add_subdirectory (gemdos)

set (SOURCE_FILES paratos.c memory.c tos_errors.c)

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/mushashi ${CMAKE_CURRENT_SOURCE_DIR}/gemdos)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")  # enable C11 standard

add_executable (paratos ${SOURCE_FILES})
target_link_libraries (paratos mushashi gemdos)
add_definitions( -D_GNU_SOURCE )
