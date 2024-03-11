cmake_minimum_required(VERSION 3.16)
project(ghcpp17-tests)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS ON)
endif()

include(FetchContent)
FetchContent_Declare(
    DocTest
    GIT_REPOSITORY "https://github.com/doctest/doctest.git"
    GIT_TAG "v2.4.9"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(DocTest)
#FetchContent_GetProperties(DocTest)
#if(NOT doctest_POPULATED)
#    FetchContent_Populate(doctest)
#    add_subdirectory(${doctest_SOURCE_DIR} ${doctest_BINARY_DIR} EXCLUDE_FROM_ALL)
#endif()
#include_directories(${DOCTEST_INCLUDE_DIR})
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

enable_testing()

add_executable(ghcpp17-test main.cpp bit_tests.cpp span_tests.cpp)
target_link_libraries(ghcpp17-test PUBLIC doctest)
target_compile_options(ghcpp17-test PUBLIC -DCOMPILE_TIME_TESTS)
doctest_discover_tests(ghcpp17-test)
