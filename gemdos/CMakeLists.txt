# Guest ABI, PRG/argument loading and CPU integration. Object files let the
# CPU callbacks and host adapters refer to each other without archive cycles.
add_library(gemdos OBJECT arguments.c mem.c path.c process.c loader.c
	gemdos_dispatch.c traps.c cookiejar.c
	${PROJECT_SOURCE_DIR}/memory.c ${PROJECT_SOURCE_DIR}/runtime.c)
target_include_directories(gemdos PUBLIC ${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/mushashi ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(gemdos PRIVATE -std=c11)
