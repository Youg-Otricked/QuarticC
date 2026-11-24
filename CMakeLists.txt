cmake_minimum_required(VERSION 3.13)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(X86_ONLY "Build for X86 only" OFF)
if(NOT DEFINED ENV{CC})
    set(CMAKE_C_COMPILER clang)
endif()
if(NOT DEFINED ENV{CXX})
    set(CMAKE_CXX_COMPILER clang++)
endif()
project(qc LANGUAGES CXX C)
if(APPLE OR EMSCRIPTEN)
    set(CMAKE_CXX_STANDARD 20)
endif()
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG
    PATHS "${LLVM_CMAKE_DIR}/../clang" "${LLVM_INSTALL_PREFIX}/lib/cmake/clang"
    NO_DEFAULT_PATH
)
if(LLVM_VERSION_MAJOR LESS 21)
    message(FATAL_ERROR "LLVM 21+ required, found ${LLVM_VERSION_MAJOR}")
endif()
option(ENABLE_LLVM "Enable LLVM-based compiler" ON)
option(CXX_RUNTIME "Build runtime.ll from runtime.cpp instead of runtime.qc" ON)
set(RUNTIME_TARGET_FLAGS)
if(EMSCRIPTEN)
    set(COMPILER_FOR_RUNTIME em++)
else()
    set(COMPILER_FOR_RUNTIME "${CMAKE_CXX_COMPILER}")
endif()
if(NOT CXX_RUNTIME)
    if(NOT QC_EXECUTABLE)
        find_program(QC_EXECUTABLE NAMES qc REQUIRED)
    endif()
    if(NOT QC_EXECUTABLE)
        message(FATAL_ERROR "QC_EXECUTABLE was not provided and qc was not found in PATH")
    endif()
    message(STATUS "Using QC runtime compiler: ${QC_EXECUTABLE}")
    if(EMSCRIPTEN)
        set(RUNTIME_TARGET_FLAGS --target wasm32-unknown-unknown)
    else()
        set(RUNTIME_TARGET_FLAGS)
    endif()
endif()
set(RUNTIMENEEDS_LL_PATH "")
if(CXX_RUNTIME)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime.ll
        COMMAND ${COMPILER_FOR_RUNTIME} ${RUNTIME_TARGET_FLAGS} -S -emit-llvm -O3 ${CMAKE_CURRENT_SOURCE_DIR}/runtime.cpp -o ${CMAKE_CURRENT_BINARY_DIR}/runtime.ll
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runtime.cpp
        VERBATIM
    )
else()
    set(RUNTIMENEEDS_LL_PATH "${CMAKE_CURRENT_BINARY_DIR}/runtimeneeds.ll")
    add_custom_command(
        OUTPUT "${RUNTIMENEEDS_LL_PATH}"
        COMMAND ${COMPILER_FOR_RUNTIME} ${RUNTIME_TARGET_FLAGS} -S -emit-llvm -O3
            "${CMAKE_CURRENT_SOURCE_DIR}/runtimeneeds.cpp"
            -o "${RUNTIMENEEDS_LL_PATH}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/runtimeneeds.cpp"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/runtime.ll"
        COMMAND "${QC_EXECUTABLE}" -co -nrt ${RUNTIME_TARGET_FLAGS} -O3
            "${CMAKE_CURRENT_SOURCE_DIR}/runtime.qc"
            -o "${CMAKE_CURRENT_BINARY_DIR}/runtime.ll"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/runtime.qc"
        VERBATIM
    )
endif()
set(EMBED_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/runtime.ll")
if(RUNTIMENEEDS_LL_PATH)
    list(APPEND EMBED_DEPENDS "${RUNTIMENEEDS_LL_PATH}")
endif()
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime_embed.cpp
    COMMAND ${CMAKE_COMMAND}
        -DRUNTIME_LL=${CMAKE_CURRENT_BINARY_DIR}/runtime.ll
        -DRUNTIMENEEDS_LL=${RUNTIMENEEDS_LL_PATH}
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/runtime_embed.cpp
        -DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/runtime_embed.cpp.in
        -P ${CMAKE_CURRENT_SOURCE_DIR}/embed_runtime.cmake
    DEPENDS ${EMBED_DEPENDS}
    VERBATIM
)
set(SOURCES
    impl/fmt.cpp
    impl/translate.cpp
    impl/parser.cpp
    impl/main.cpp
    impl/lexer.cpp
    impl/nodes.cpp
    impl/token.cpp
    impl/errors.cpp
    impl/preproccesser.cpp
    impl/compiler.cpp
    impl/compiler_run.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/runtime_embed.cpp
)
add_executable(qc ${SOURCES})
target_include_directories(qc PRIVATE intf)
if(X86_ONLY)
    target_compile_definitions(qc PRIVATE X86_ONLY)
endif()

target_compile_options(qc PRIVATE
    $<$<CONFIG:Release>:-O3>
    --std=c++23
    $<$<CONFIG:RelWithDebInfo>:-O2>
    $<$<CONFIG:Debug>:-fstandalone-debug>
)
if(ENABLE_LLVM)
    separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
    add_definitions(${LLVM_DEFINITIONS_LIST})
    if(LLVM_ENABLE_ABI_BREAKING_CHECKS)
        target_compile_definitions(qc PRIVATE LLVM_ENABLE_ABI_BREAKING_CHECKS=1)
    else()
        target_compile_definitions(qc PRIVATE LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1)
    endif()
    add_definitions(-D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)
    target_compile_definitions(qc PRIVATE ENABLE_LLVM)
    set(CUSTOM_LLVM_INCLUDE "" CACHE PATH "Override LLVM/Clang include directory")
    set(CUSTOM_LLVM_LIB "" CACHE PATH "Override LLVM/Clang library directory")
    if(CUSTOM_LLVM_INCLUDE)
        set(QC_INC_ROOT "${CUSTOM_LLVM_INCLUDE}")
    else()
        list(GET LLVM_INCLUDE_DIRS 0 QC_INC_ROOT)
    endif()
    set(QC_INC_STAGE "${CMAKE_BINARY_DIR}/llvm-inc")
    file(REMOVE_RECURSE "${QC_INC_STAGE}")
    file(MAKE_DIRECTORY "${QC_INC_STAGE}")
    foreach(_sub llvm llvm-c clang clang-c)
        if(EXISTS "${QC_INC_ROOT}/${_sub}")
            file(CREATE_LINK "${QC_INC_ROOT}/${_sub}" "${QC_INC_STAGE}/${_sub}" SYMBOLIC)
        endif()
    endforeach()
    file(STRINGS "${QC_INC_STAGE}/llvm/Config/llvm-config.h" _ver REGEX "define LLVM_VERSION_STRING")
    string(REGEX REPLACE ".*\"(.*)\".*" "\\1" QC_HDR_VER "${_ver}")
    if(NOT QC_HDR_VER STREQUAL LLVM_PACKAGE_VERSION)
        message(FATAL_ERROR "LLVM header version ${QC_HDR_VER} (${QC_INC_ROOT}) != library version ${LLVM_PACKAGE_VERSION} (${LLVM_DIR})")
    endif()
    message(STATUS "LLVM headers: ${QC_INC_ROOT} (${QC_HDR_VER})")
    target_compile_options(qc BEFORE PRIVATE "SHELL:-isystem ${QC_INC_STAGE}")
    set(_llvm_lib_hint "${CUSTOM_LLVM_LIB}")
    if(NOT _llvm_lib_hint)
        set(_llvm_lib_hint "${LLVM_LIBRARY_DIRS}")
    endif()
    find_library(LLVM_LIB  NAMES LLVM LLVM-${LLVM_VERSION_MAJOR} PATHS "${_llvm_lib_hint}" NO_DEFAULT_PATH REQUIRED)
    find_library(CLANG_LIB NAMES clang clang-${LLVM_VERSION_MAJOR} PATHS "${_llvm_lib_hint}" NO_DEFAULT_PATH REQUIRED)
    message(STATUS "LLVM lib:  ${LLVM_LIB}")
    message(STATUS "Clang lib: ${CLANG_LIB}")
    target_link_libraries(qc PRIVATE ${LLVM_LIB} ${CLANG_LIB} ${LLVM_SYSTEM_LIBS})
    target_link_options(qc PRIVATE "LINKER:-rpath,${_llvm_lib_hint}")
    if(NOT APPLE)
        target_link_options(qc PRIVATE "LINKER:--disable-new-dtags")
    endif()
endif()
target_include_directories(qc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(EMSCRIPTEN)
    message(STATUS "Detected Emscripten environment - Applying WASM configuration")
    add_definitions(-D__EMSCRIPTEN__)
    add_definitions(-D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)
    if(NOT LLVM_WASM_PATH)
        set(LLVM_WASM_PATH "/usr/lib/llvm-21")
    endif()
    include_directories("${LLVM_WASM_PATH}/include")
    set_target_properties(qc PROPERTIES
        OUTPUT_NAME "qc"
        SUFFIX ".js"
        LINK_FLAGS "\
            -s WASM=1 \
            -s FORCE_FILESYSTEM=1 \
            -s EXPORTED_FUNCTIONS='[\"_run_quantumc_code\"]' \
            -s EXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\",\"FS\"]' \
            -s ALLOW_MEMORY_GROWTH=1 \
            --preload-file ${CMAKE_SOURCE_DIR}/stdlib.qc@/stdlib.qc \
            -s STACK_SIZE=134217728 \
            -s MODULARIZE=1 \
            -s EXPORT_NAME=\"QuarticC\" \
            -s TOTAL_MEMORY=805306368 \
            -s ASSERTIONS=1 \
            -s DEFAULT_LIBRARY_FUNCS_TO_INCLUDE='[\"$stackSave\",\"$stackRestore\",\"$stackAlloc\"]'\
            -Oz \
            -s NO_DISABLE_EXCEPTION_CATCHING"
    )
endif()
if(NOT EMSCRIPTEN)
    set(STD_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/std")
    set(STD_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/std_outs")
    set(STDLIB_SRC  "${STD_SRC_DIR}/stdlib.qc")
    set(STDLIB_OUT  "${STD_OUT_DIR}/std/libstdqc${CMAKE_SHARED_LIBRARY_SUFFIX}")
    file(GLOB_RECURSE STD_SOURCES CONFIGURE_DEPENDS "${STD_SRC_DIR}/*")
    add_custom_command(
        OUTPUT "${STDLIB_OUT}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${STD_OUT_DIR}/std"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${STD_OUT_DIR}/std"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${STD_SRC_DIR}" "${STD_OUT_DIR}/std"
        COMMAND $<TARGET_FILE:qc> -dynamic "${STDLIB_SRC}" -o "${STDLIB_OUT}"
        DEPENDS ${STD_SOURCES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Staging std/ into std_outs/ and compiling stdlib.qc..."
        VERBATIM
    )
    add_custom_target(build_stdlib ALL
        DEPENDS "${STDLIB_OUT}"
    )
endif()
