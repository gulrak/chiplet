
include(FindPython3)
add_test(NAME assembler-test-py COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/assembler-test.py $<TARGET_FILE:chiplet> ${CMAKE_CURRENT_SOURCE_DIR}/c-octo-tests WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH} )

add_executable(chiplet-tests main.cpp assembler_tests.cpp)
target_link_libraries(chiplet-tests PUBLIC chiplet-lib doctest fast_float)
doctest_discover_tests(chiplet-tests)
