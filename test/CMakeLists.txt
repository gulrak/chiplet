include(${PROJECT_SOURCE_DIR}/cmake/FetchFromGH.cmake)

github_fetch_file(
    REPO https://github.com/Timendus/chipcode.git
    REF  main
    PATH octopus2/images/clock.png
    OUT  ${CMAKE_BINARY_DIR}/test/images/clock.png
)

github_fetch_file(
    REPO https://github.com/Timendus/chipcode.git
    REF  main
    PATH octopus2/images/clock-bw.png
    OUT  ${CMAKE_BINARY_DIR}/test/images/clock-bw.png
)

github_fetch_file(
    REPO https://github.com/Timendus/chipcode.git
    REF  main
    PATH octopus2/images/clock-bg.png
    OUT  ${CMAKE_BINARY_DIR}/test/images/clock-bg.png
)

github_fetch_file(
    REPO https://github.com/Timendus/chipcode.git
    REF  main
    PATH octopus2/images/clock-four-colors.png
    OUT  ${CMAKE_BINARY_DIR}/test/images/clock-four-colors.png
)

github_fetch_file(
    REPO https://github.com/Timendus/chipcode.git
    REF  main
    PATH octopus2/images/clock-dithered.png
    OUT  ${CMAKE_BINARY_DIR}/test/images/clock-dithered.png
)

include(FindPython3)
add_test(NAME assembler-test-py COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/assembler-test.py $<TARGET_FILE:chiplet> ${CMAKE_CURRENT_SOURCE_DIR}/c-octo-tests WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH} )

add_executable(chiplet-tests main.cpp assembler_tests.cpp chunkedmemory_test.cpp utility_tests.cpp image_tests.cpp)
target_compile_definitions(chiplet-tests PUBLIC IMAGES_PATH="${CMAKE_BINARY_DIR}/test/images")
target_link_libraries(chiplet-tests PUBLIC chiplet-lib doctest fast_float)
doctest_discover_tests(chiplet-tests)

add_executable(gifimage-tests gifimage_tests.cpp)
target_link_libraries(gifimage-tests fmt::fmt)

add_executable(dis1802 dis1802.cpp)
target_link_libraries(dis1802 PUBLIC chiplet-lib)
