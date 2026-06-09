cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

file(GLOB_RECURSE RTT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
get_subdirs_and_self(${CMAKE_CURRENT_SOURCE_DIR} RTT_INC)

add_library(rtt STATIC "${RTT_SRC}")

target_include_directories(rtt PUBLIC "${RTT_INC}" "${CFG_DIR}")

print_item("LocalLib" "library ${ColorBold}rtt${ColorReset} built")
