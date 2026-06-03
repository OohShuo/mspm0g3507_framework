cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

file(GLOB_RECURSE LIB_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
get_subdirs_and_self(${CMAKE_CURRENT_SOURCE_DIR} LIB_INC)

add_library(lib STATIC "${LIB_SRC}")

target_include_directories(lib PUBLIC "${LIB_INC}" "${CFG_DIR}")
