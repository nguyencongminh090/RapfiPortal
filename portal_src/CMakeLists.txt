cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)
project(portal-engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # Generates compile_commands.json for IDE/clangd

#==========================================================
# Auto-detect SIMD/ISA capability of the host
include(CheckCXXSourceCompiles)

function(bool_to_onoff in_var out_var)
    if(${in_var})
        set(${out_var} ON  PARENT_SCOPE)
    else()
        set(${out_var} OFF PARENT_SCOPE)
    endif()
endfunction()

set(DEFAULT_USE_SSE  OFF)
set(DEFAULT_USE_AVX2 OFF)
set(DEFAULT_USE_BMI2 OFF)

if(NOT CMAKE_CROSSCOMPILING)
    set(CMAKE_REQUIRED_FLAGS "-march=native")
    set(CMAKE_REQUIRED_QUIET TRUE)

    check_cxx_source_compiles("
        #include <immintrin.h>
        int main(){ __m128i a = _mm_setzero_si128(); (void)a; return 0; }"
        HAS_SSE)
    check_cxx_source_compiles("
        #include <immintrin.h>
        int main(){ __m256i a = _mm256_setzero_si256(); (void)a; return 0; }"
        HAS_AVX2)
    check_cxx_source_compiles("
        #include <immintrin.h>
        int main(){ unsigned long long r = _pext_u64(0xFFFFULL, 0xAAULL); (void)r; return 0; }"
        HAS_BMI2)

    bool_to_onoff(HAS_SSE  DEFAULT_USE_SSE)
    bool_to_onoff(HAS_AVX2 DEFAULT_USE_AVX2)
    bool_to_onoff(HAS_BMI2 DEFAULT_USE_BMI2)

    message(STATUS "Detected SIMD capability: SSE=${DEFAULT_USE_SSE} AVX2=${DEFAULT_USE_AVX2} BMI2=${DEFAULT_USE_BMI2}")
endif()

#==========================================================
# Options

option(NO_MULTI_THREADING "Disable multi-threading"    OFF)
option(NO_COMMAND_MODULES "Disable command modules"    OFF)
option(USE_SSE  "Enable SSE2/SSSE3/SSE4.1"            ${DEFAULT_USE_SSE})
option(USE_AVX2 "Enable AVX2/FMA"                     ${DEFAULT_USE_AVX2})
option(USE_BMI2 "Enable BMI2"                         ${DEFAULT_USE_BMI2})
option(ENABLE_LTO "Enable link time optimization"      OFF)
option(FORCE_ASSERTION "Force assertions in all builds" OFF)
option(USE_ASAN "Enable address sanitizer"             OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (default: Release)" FORCE)
endif()

#==========================================================
# Source files
# Structure mirrors Rapfi's src/ but with portal modifications:
#   game/board.h/.cpp  — MODIFIED: portal/wall support
#   config.h/.cpp      — MODIFIED: portal config storage
#   command/gomocup.cpp — MODIFIED: protocol + WALL/Portal input
#   command/opengen.cpp — MODIFIED: new rule support
#   main.cpp           — MODIFIED: portal engine entry
#   (all other files)  — copied unchanged from Rapfi

set(CORE_SOURCES
    # Command modules (unchanged copies)
    command/argutils.cpp
    command/benchmark.cpp
    command/command.cpp
    command/gomocup.cpp     # MODIFIED: portal protocol

    # Core (unchanged copies)
    core/hash.cpp
    core/iohelper.cpp
    core/utils.cpp
    core/platform.cpp
    core/version.cpp

    # Database (unchanged copies)
    database/dbclient.cpp
    database/dbutils.cpp
    database/dbtypes.cpp
    database/yxdbstorage.cpp

    # Eval (unchanged copies — reuses binary_model tables as-is)
    eval/eval.cpp
    eval/evaluator.cpp
    eval/influence.cpp
    eval/mix9svqnnue.cpp
    eval/mix10nnue.cpp

    # Game (MODIFIED board.cpp; pattern/movegen unchanged)
    game/board.cpp          # MODIFIED: newGame/move/undo portal logic
    game/movegen.cpp
    game/pattern.cpp

    # Search (unchanged copies)
    search/hashtable.cpp
    search/movepick.cpp
    search/opening.cpp
    search/searchcommon.cpp
    search/searchoutput.cpp
    search/searchthread.cpp
    search/timecontrol.cpp
    search/ab/history.cpp
    search/ab/search.cpp
    search/mcts/node.cpp
    search/mcts/search.cpp

    # Root (MODIFIED)
    config.cpp
    internalConfig.cpp
    main.cpp
)

set(MODULE_SOURCES
    command/database.cpp
    command/dataprep.cpp
    command/opengen.cpp     # MODIFIED: portal rule support
    command/selfplay.cpp
    command/tuning.cpp
    tuning/dataset.cpp
    tuning/datawriter.cpp
    tuning/optimizer.cpp
    tuning/tuner.cpp
)

add_executable(portal-engine
    ${CORE_SOURCES}
    $<$<NOT:$<BOOL:${NO_COMMAND_MODULES}>>:${MODULE_SOURCES}>
)
set_target_properties(portal-engine PROPERTIES OUTPUT_NAME "pbrain-MINT-P")

#==========================================================
# External libraries (symlinked/copied from Rapfi's external/)

add_subdirectory(external/cpptoml)
add_subdirectory(external/cxxopts)
add_subdirectory(external/lz4)
add_subdirectory(external/simde)
target_link_libraries(portal-engine PRIVATE cpptoml cxxopts lz4 simde)

if(NOT NO_COMMAND_MODULES)
    add_subdirectory(external/flat.hpp)
    add_subdirectory(external/zip)
    add_subdirectory(external/libnpy)
    add_subdirectory(external/thread-pool)
    target_link_libraries(portal-engine PRIVATE flat.hpp zip libnpy BS_thread_pool)
endif()

#==========================================================
# Compile definitions

if(NOT NO_MULTI_THREADING)
    target_compile_definitions(portal-engine PRIVATE MULTI_THREADING)
endif()
if(NOT NO_COMMAND_MODULES)
    target_compile_definitions(portal-engine PRIVATE COMMAND_MODULES)
endif()
if(USE_SSE)
    target_compile_definitions(portal-engine PRIVATE USE_SSE)
endif()
if(USE_AVX2)
    target_compile_definitions(portal-engine PRIVATE USE_AVX2)
endif()
if(USE_BMI2 OR USE_AVX2)
    target_compile_definitions(portal-engine PRIVATE USE_BMI2)
endif()

#==========================================================
# Compiler flags

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")

    message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")

    if(USE_AVX2)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma")
    elseif(USE_SSE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1")
    endif()
    if(USE_BMI2 OR USE_AVX2)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mbmi2")
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall \
        -Wno-unused-variable \
        -Wno-unused-function \
        -Wno-missing-braces \
        -Wno-logical-op-parentheses \
        -Wno-parentheses \
        -Wno-sign-compare")

    if(FORCE_ASSERTION)
        string(TOUPPER ${CMAKE_BUILD_TYPE} _BT)
        string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_${_BT} "${CMAKE_CXX_FLAGS_${_BT}}")
    endif()

    if(USE_ASAN)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    endif()

    if(ENABLE_LTO)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT _ipo OUTPUT _err)
        if(_ipo)
            set_property(TARGET portal-engine PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        else()
            message(STATUS "LTO not supported: <${_err}>")
        endif()
    endif()

    # Linking
    if(NOT WIN32 AND NOT EMSCRIPTEN)
        if(NO_MULTI_THREADING AND NO_COMMAND_MODULES)
            target_link_libraries(portal-engine PRIVATE -static)
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            target_link_libraries(portal-engine PRIVATE pthread)
        else()
            target_link_libraries(portal-engine PRIVATE pthread atomic)
        endif()
    endif()

elseif(MSVC)
    message(STATUS "Compiler: MSVC")
    if(USE_AVX2)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D__SSE3__ /D__SSSE3__ /D__SSE4_1__ /D__AVX__ /D__AVX2__ /D__FMA__")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
    elseif(USE_SSE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D__SSE3__ /D__SSSE3__ /D__SSE4_1__")
    endif()
endif()

#==========================================================
# Test target — portal board logic tests

# Core sources minus main.cpp (test has its own main)
set(TEST_SOURCES ${CORE_SOURCES})
list(REMOVE_ITEM TEST_SOURCES main.cpp)

add_executable(portal-test
    test_portal.cpp
    ${TEST_SOURCES}
)
target_link_libraries(portal-test PRIVATE cpptoml cxxopts lz4 simde)
if(NOT NO_MULTI_THREADING)
    target_compile_definitions(portal-test PRIVATE MULTI_THREADING)
endif()
if(USE_SSE)
    target_compile_definitions(portal-test PRIVATE USE_SSE)
endif()
if(USE_AVX2)
    target_compile_definitions(portal-test PRIVATE USE_AVX2)
endif()
if(USE_BMI2 OR USE_AVX2)
    target_compile_definitions(portal-test PRIVATE USE_BMI2)
endif()
if(NOT WIN32 AND NOT EMSCRIPTEN)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        target_link_libraries(portal-test PRIVATE pthread)
    else()
        target_link_libraries(portal-test PRIVATE pthread atomic)
    endif()
endif()

# Pattern detection test suite (Phase 1: normal, Phase 2: portal)
add_executable(portal-pattern-test
    test_pattern.cpp
    ${TEST_SOURCES}
)
target_link_libraries(portal-pattern-test PRIVATE cpptoml cxxopts lz4 simde)
if(NOT NO_MULTI_THREADING)
    target_compile_definitions(portal-pattern-test PRIVATE MULTI_THREADING)
endif()
if(USE_SSE)
    target_compile_definitions(portal-pattern-test PRIVATE USE_SSE)
endif()
if(USE_AVX2)
    target_compile_definitions(portal-pattern-test PRIVATE USE_AVX2)
endif()
if(USE_BMI2 OR USE_AVX2)
    target_compile_definitions(portal-pattern-test PRIVATE USE_BMI2)
endif()
if(NOT WIN32 AND NOT EMSCRIPTEN)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        target_link_libraries(portal-pattern-test PRIVATE pthread)
    else()
        target_link_libraries(portal-pattern-test PRIVATE pthread atomic)
    endif()
endif()

#==========================================================
# IDE helper

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Sources" FILES ${CORE_SOURCES} ${MODULE_SOURCES})
