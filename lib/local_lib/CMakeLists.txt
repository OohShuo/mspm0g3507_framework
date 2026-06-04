cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

file(GLOB_RECURSE LOCAL_LIB_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
get_subdirs_and_self(${CMAKE_CURRENT_SOURCE_DIR} LOCAL_LIB_INC)

add_library(local_lib STATIC "${LOCAL_LIB_SRC}")

target_include_directories(local_lib PUBLIC "${LOCAL_LIB_INC}" "${CFG_DIR}")

print_item("LocalLib" "library ${ColorBold}local_lib${ColorReset} built")
