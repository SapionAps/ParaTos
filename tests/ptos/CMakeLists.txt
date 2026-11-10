set_source_files_properties(${MUSASHI_GENERATED_SOURCES} PROPERTIES GENERATED TRUE)
add_library(mushashi_ptos STATIC
	${PROJECT_SOURCE_DIR}/mushashi/m68kcpu.c
	${PROJECT_SOURCE_DIR}/mushashi/m68kdasm.c
	${MUSASHI_GENERATED_SOURCES})
target_include_directories(mushashi_ptos PRIVATE ${PROJECT_SOURCE_DIR}/mushashi ${PROJECT_BINARY_DIR}/mushashi)
target_compile_definitions(mushashi_ptos PRIVATE PARATOS_PTOS=1)
add_dependencies(mushashi_ptos mushashi)

add_executable(ptos_contract contract.c
	${PROJECT_SOURCE_DIR}/host/ptos/memory.c
	${PROJECT_SOURCE_DIR}/host/ptos/loader.c
	${PROJECT_SOURCE_DIR}/host/ptos/process.c
	${PROJECT_SOURCE_DIR}/host/ptos/dispatch.c
	${PROJECT_SOURCE_DIR}/host/ptos/runtime.c)
target_include_directories(ptos_contract PRIVATE ${PROJECT_SOURCE_DIR}/mushashi ${PROJECT_SOURCE_DIR}/host/ptos)
target_compile_definitions(ptos_contract PRIVATE PARATOS_PTOS=1)
target_compile_features(ptos_contract PRIVATE c_std_11)
target_compile_options(ptos_contract PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_link_libraries(ptos_contract PRIVATE mushashi_ptos)
add_test(NAME ptos_contract COMMAND ptos_contract)
set_tests_properties(ptos_contract PROPERTIES TIMEOUT 20)
