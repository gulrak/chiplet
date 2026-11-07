add_library(saturn saturndecoder.cpp saturndecoder.hpp)
target_link_libraries(saturn PUBLIC fmt::fmt)

add_executable(saturndis saturndisassembler.cpp)
target_include_directories(saturndis PUBLIC ../include)
target_link_libraries(saturndis PUBLIC saturn ghc_filesystem)
