cmake_minimum_required(VERSION 3.23)

project(
    smallcanon
    VERSION 0.1.0
    DESCRIPTION "Graph canonization for small graphs"
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SMALLCANON_CLANG_TIDY "Run clang-tidy during builds" OFF)
if(SMALLCANON_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy-21 clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--extra-arg=-Wno-unknown-warning-option")
endif()

add_library(smallcanon INTERFACE
        include/smallcanon/bits.hpp)
add_library(smallcanon::smallcanon ALIAS smallcanon)

target_compile_features(smallcanon INTERFACE cxx_std_23)

target_include_directories(
    smallcanon
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_sources(
    smallcanon
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/smallcanon/adj_matrix.hpp
            include/smallcanon/g6_parser.hpp
)

target_compile_options(
    smallcanon
    INTERFACE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive- /Zc:preprocessor>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2 -Werror>
        $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast>
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
    add_subdirectory(src)
endif()

option(TESTS "Build smallcanon tests" OFF)

if(TESTS)
    include(CTest)

    include(FetchContent)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()
