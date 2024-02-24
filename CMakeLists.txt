cmake_minimum_required(VERSION 3.16)

project(chiplet VERSION 1.1.0 LANGUAGES C CXX)
cmake_policy(VERSION 3.16)

include(cmake/BuildSettings.cmake)
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

include_directories(${PROJECT_SOURCE_DIR}/external)

enable_testing()

add_subdirectory(src)
add_subdirectory(test)

get_directory_property(hasParent PARENT_DIRECTORY)
if(NOT hasParent)
set(CPACK_PACKAGE_VENDOR "gulrak.net")
set(CPACK_PACKAGE_CONTACT "s.schuemann@pobox.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/gulrak/chiplet")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Chiplet - A CHIP-8 commandline preprocessor and assembler")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_GENERATOR "TGZ;ZIP" CACHE STRING "Generators to support. semi-colon delimited list")
include(CPack)
cpack_add_component(chiplet_pack)
endif()