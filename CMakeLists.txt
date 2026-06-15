cmake_minimum_required(VERSION 3.23)

project(
        smallcanon
        VERSION 0.1.0
        DESCRIPTION "Graph canonization for small graphs"
        LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SMALLCANON_CLANG_TIDY "Run clang-tidy during builds" OFF)
if (SMALLCANON_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy-21 clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--extra-arg=-Wno-unknown-warning-option")
endif ()

include(FetchContent)

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)


set(XSIMD_SKIP_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(
        xsimd
        GIT_REPOSITORY https://github.com/xtensor-stack/xsimd.git
        GIT_TAG 14.2.0
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(xsimd)

add_library(smallcanon INTERFACE
        include/smallcanon/adj_matrix.hpp
        include/smallcanon/bitspan.hpp
        include/smallcanon/graph.hpp
        include/smallcanon/parser.hpp
)
add_library(smallcanon::smallcanon ALIAS smallcanon)

option(PRINT_DEBUG "Enable debug logging" OFF)


target_compile_features(smallcanon INTERFACE cxx_std_23)

target_include_directories(
        smallcanon
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(smallcanon INTERFACE xsimd)

target_compile_options(
        smallcanon
        INTERFACE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /Zc:preprocessor>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall>
)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
    add_subdirectory(src)
endif ()

option(TESTS "Build smallcanon tests" OFF)
option(INDIV_TESTS "Build many small test executables" OFF)
option(BENCHMARKS "Build smallcanon benchmarks" OFF)
option(NAUTY "Build with nauty" OFF)
option(CLI "Build cli tools" OFF)

if (NAUTY)
    enable_language(C)
    include(FetchContent)

    FetchContent_Declare(
            nauty_src
            URL https://pallini.di.uniroma1.it/nauty2_9_3.tar.gz
    )

    FetchContent_MakeAvailable(nauty_src)

    add_library(nauty STATIC
            ${nauty_src_SOURCE_DIR}/nauty.c
            ${nauty_src_SOURCE_DIR}/nautil.c
            ${nauty_src_SOURCE_DIR}/naugraph.c
            ${nauty_src_SOURCE_DIR}/schreier.c
            ${nauty_src_SOURCE_DIR}/naurng.c
            ${nauty_src_SOURCE_DIR}/traces.c
            ${nauty_src_SOURCE_DIR}/gtools.c
            ${nauty_src_SOURCE_DIR}/nausparse.c
    )

    target_include_directories(nauty PUBLIC
            ${nauty_src_SOURCE_DIR}
    )

    target_compile_definitions(nauty PUBLIC
            MAXN=WORDSIZE
    )

    target_compile_definitions(smallcanon INTERFACE SMALLCANON_WITH_NAUTY)
endif ()


if (TESTS)
    include(CTest)

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif ()

if (BENCHMARKS)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
            googlebenchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.9.1
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googlebenchmark)

    add_subdirectory(benchmarks)
endif ()

if (CLI)
    add_subdirectory(cli)
    if (PRINT_DEBUG)
        target_compile_definitions(devtool PRIVATE PRINT_DEBUG)
    endif()
endif ()
