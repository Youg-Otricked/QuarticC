if(RUNTIMENEEDS_LL AND NOT "${RUNTIMENEEDS_LL}" STREQUAL "")
    find_program(LLVM_LINK llvm-link REQUIRED)
    get_filename_component(OUTPUT_DIR "${OUTPUT}" DIRECTORY)
    set(MERGED_LL "${OUTPUT_DIR}/merged_runtime.ll")
    execute_process(
        COMMAND ${LLVM_LINK}
            "${RUNTIME_LL}"
            "${RUNTIMENEEDS_LL}"
            -S
            -o "${MERGED_LL}"
        RESULT_VARIABLE LINK_RESULT
    )
    if(NOT LINK_RESULT EQUAL 0)
        message(FATAL_ERROR "llvm-link failed")
    endif()
    set(EMBED_LL "${MERGED_LL}")
else()
    set(EMBED_LL "${RUNTIME_LL}")
endif()
file(READ "${EMBED_LL}" RUNTIME_LL_CONTENT HEX)
string(REGEX REPLACE
    "([0-9a-f][0-9a-f])"
    "0x\\1,"
    RUNTIME_LL_HEX
    "${RUNTIME_LL_CONTENT}"
)
file(READ "${TEMPLATE}" TEMPLATE_CONTENT)
string(CONFIGURE "${TEMPLATE_CONTENT}" GENERATED_OUTPUT @ONLY)
set(SHOULD_WRITE TRUE)
if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" CURRENT_OUTPUT)
    if("${CURRENT_OUTPUT}" STREQUAL "${GENERATED_OUTPUT}")
        set(SHOULD_WRITE FALSE)
        file(TOUCH "${OUTPUT}")
    endif()
endif()

if(SHOULD_WRITE)
    file(WRITE "${OUTPUT}" "${GENERATED_OUTPUT}")
endif()
