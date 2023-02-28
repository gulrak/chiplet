set(CHIPLET_LIBRARY_SOURCE
    ../include/chiplet/chip8compiler.hpp
    ../include/chiplet/octocompiler.hpp

    chip8compiler.cpp
    octocompiler.cpp
)

add_library(chiplet-lib STATIC ${CHIPLET_LIBRARY_SOURCE})
target_include_directories(chiplet-lib PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_link_libraries(chiplet-lib PUBLIC ghc_filesystem fmt::fmt)

add_executable(chiplet chiplet.cpp)
target_compile_definitions(chiplet PUBLIC CHIPLET_VERSION="${PROJECT_VERSION}" CHIPLET_HASH="${GIT_COMMIT_HASH}")
target_link_libraries(chiplet PRIVATE chiplet-lib)
