add_executable(saturndis saturndisassembler.cpp ../src/decoder/saturndecoder.cpp)
target_include_directories(saturndis PUBLIC ../include)
target_link_libraries(saturndis PUBLIC saturn ghc_filesystem fmt::fmt)
