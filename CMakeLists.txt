cmake_minimum_required(VERSION 3.16)
project(CSlash VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CSLASH_BUILD_TESTS "Build the test suite" ON)

add_library(cslash_core STATIC
    src/lexer.cpp
    src/parser.cpp
    src/runtime.cpp
    src/interpreter.cpp
    src/png.cpp
)
target_include_directories(cslash_core PUBLIC include)
target_compile_definitions(cslash_core PUBLIC CSLASH_VERSION="${PROJECT_VERSION}")

# GCC < 9 needs an extra library for std::filesystem.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(cslash_core PUBLIC stdc++fs)
endif()

if(MSVC)
    target_compile_options(cslash_core PUBLIC /W4 /utf-8)
else()
    target_compile_options(cslash_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

add_executable(cslash src/main.cpp)
target_link_libraries(cslash PRIVATE cslash_core)

if(CSLASH_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    add_executable(cslash_tests tests/test_main.cpp)
    target_link_libraries(cslash_tests PRIVATE cslash_core)
    add_test(NAME cslash_tests COMMAND cslash_tests)
endif()
