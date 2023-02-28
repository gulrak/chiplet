if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS ON)
endif()

#add_compile_options(-fsanitize=address)
#add_link_options(-fsanitize=address)

set(DEPENDENCY_FOLDER "external")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER ${DEPENDENCY_FOLDER})
option(CHIPLET_FLAT_OUTPUT "Combine outputs (binaries, libs and archives) in top level bin and lib directories." ON)
if(CHIPLET_FLAT_OUTPUT)
    link_directories(${CMAKE_BINARY_DIR}/lib)
    set(BINARY_OUT_DIR ${CMAKE_BINARY_DIR}/bin)
    set(LIB_OUT_DIR ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_OUT_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LIB_OUT_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${LIB_OUT_DIR})
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${BINARY_OUT_DIR})
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${LIB_OUT_DIR})
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${LIB_OUT_DIR})
    endforeach()
    message("using combined output directories ${BINARY_OUT_DIR} and ${LIB_OUT_DIR}")
else()
    message("not combining output")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type, available options: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif()

if(APPLE AND CMAKE_BUILD_TYPE EQUAL "Release")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Force universal binary 2 builds" FORCE)
endif()
if(APPLE AND CMAKE_BUILD_TYPE EQUAL "Debug")
    set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Force universal binary 2 builds" FORCE)
endif()

if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static"  CACHE STRING "Linker Flags for MinGW Builds" FORCE)
endif()

include(FetchContent)

FetchContent_Declare(
        GhcFilesystem
        GIT_REPOSITORY "https://github.com/gulrak/filesystem.git"
        GIT_TAG "v1.5.12"
        GIT_SHALLOW TRUE
)
FetchContent_GetProperties(GhcFilesystem)
if(NOT ghcfilesystem_POPULATED)
    FetchContent_Populate(GhcFilesystem)
    add_subdirectory(${ghcfilesystem_SOURCE_DIR} ${ghcfilesystem_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_Declare(
    DocTest
    GIT_REPOSITORY "https://github.com/doctest/doctest.git"
    GIT_TAG "v2.4.9"
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(DocTest)
if(NOT doctest_POPULATED)
    FetchContent_Populate(doctest)
    add_subdirectory(${doctest_SOURCE_DIR} ${doctest_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
include_directories(${DOCTEST_INCLUDE_DIR})

FetchContent_Declare(
    FmtLib
    GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
    GIT_TAG "9.1.0"
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(FmtLib)
if(NOT fmtlib_POPULATED)
    FetchContent_Populate(FmtLib)
    add_subdirectory(${fmtlib_SOURCE_DIR} ${fmtlib_BINARY_DIR} EXCLUDE_FROM_ALL)
    include_directories("${fmtlib_SOURCE_DIR}/include")
    message(STATUS "fmtlib: ${fmtlib_SOURCE_DIR}/include")
endif()

find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT GIT_COMMIT_HASH)
        set(GIT_COMMIT_HASH "deadbeef")
    endif()
endif()

