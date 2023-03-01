set(CHIPLET_LIBRARY_SOURCE
    ../include/chiplet/chip8compiler.hpp
    ../include/chiplet/chip8decompiler.hpp
    ../include/chiplet/chip8meta.hpp
    ../include/chiplet/chip8variants.hpp
    ../include/chiplet/octocompiler.hpp

    chip8compiler.cpp
    chip8decompiler.cpp
    octocompiler.cpp
)

add_library(chiplet-lib STATIC ${CHIPLET_LIBRARY_SOURCE})
target_include_directories(chiplet-lib PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_link_libraries(chiplet-lib PUBLIC ghc_filesystem fmt::fmt)

add_executable(chiplet chiplet.cpp)
target_compile_definitions(chiplet PUBLIC CHIPLET_VERSION="${PROJECT_VERSION}" CHIPLET_HASH="${GIT_COMMIT_HASH}")
target_link_libraries(chiplet PRIVATE chiplet-lib)

if(APPLE)
    install(TARGETS chiplet DESTINATION "." COMPONENT chiplet_pack)
    install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION "." COMPONENT chiplet_pack)
    install(FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION "." COMPONENT chiplet_pack)
else()
    install(TARGETS chiplet DESTINATION . COMPONENT chiplet_pack)
    install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION "." COMPONENT chiplet_pack)
    install(FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION "." COMPONENT chiplet_pack)
    if(UNIX)
        set(CPACK_COMPONENT_CADMIUM_APP_DESCRIPTION "A commandline multitool for CHIP-8 variant development")
    endif()
endif()