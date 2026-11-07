add_executable(viptape viptape.cpp)
target_include_directories(viptape PUBLIC ${PROJECT_SOURCE_DIR}/external ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(viptape PUBLIC ghc_filesystem fmt::fmt libresample tl::expected)

add_executable(hex2png hex2png.cpp)
target_include_directories(hex2png PUBLIC ${PROJECT_SOURCE_DIR}/external)

add_executable(saturndis saturndisassembler.cpp ../src/decoder/saturndecoder.cpp)
target_include_directories(saturndis PUBLIC ../include)
target_link_libraries(saturndis PUBLIC ghc_filesystem fmt::fmt tl::expected)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/c2vip.c)
    add_executable(c2vip c2vip.c c2vip.h)
endif()