cmake_minimum_required(VERSION 3.13)
project(qc LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(X86_ONLY "Build for X86 only" OFF)
set(CMAKE_CXX_COMPILER "clang++")
if(APPLE OR EMSCRIPTEN)
    set(CMAKE_CXX_STANDARD 20)
endif()
find_package(Clang REQUIRED CONFIG)
find_package(LLVM REQUIRED CONFIG)
if(LLVM_VERSION_MAJOR LESS 21)
    message(FATAL_ERROR "LLVM 21+ required, found ${LLVM_VERSION_MAJOR}")
endif()
option(ENABLE_LLVM "Enable LLVM-based compiler" ON)
option(CXX_RUNTIME "Build runtime.ll from runtime.cpp instead of runtime.qc" ON)
set(RUNTIME_TARGET_FLAGS)
set(COMPILER_FOR_RUNTIME "clang++")
if(EMSCRIPTEN)
    set(COMPILER_FOR_RUNTIME "em++")
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
    add_definitions(-D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)
    target_compile_definitions(qc PRIVATE ENABLE_LLVM)
    target_include_directories(qc PRIVATE ${LLVM_INCLUDE_DIRS})
    target_link_directories(qc PRIVATE ${LLVM_LIBRARY_DIRS})
    include_directories(${CLANG_INCLUDE_DIRS})
    target_link_libraries(qc PRIVATE
        LLVM
        ${LLVM_SYSTEM_LIBS}
        libclang
    )
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
