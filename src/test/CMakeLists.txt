cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

file(GLOB_RECURSE TEST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

get_subdirs_and_self(${CMAKE_CURRENT_SOURCE_DIR} TEST_INC)

add_library(test STATIC "${TEST_SRC}")

target_include_directories(test PUBLIC "${TEST_INC}" "${CFG_DIR}")

target_link_libraries(test PRIVATE app hal bsp lib syscall)

print_item("test" "library ${ColorBold}test${ColorReset} built")
