cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

file(GLOB_RECURSE HAL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
get_subdirs_and_self(${CMAKE_CURRENT_SOURCE_DIR} HAL_INC)

add_library(hal STATIC "${HAL_SRC}")

target_include_directories(hal PUBLIC "${HAL_INC}" "${CFG_DIR}")

target_link_libraries(hal PRIVATE bsp lib)

print_item("HAL" "library ${ColorBold}hal${ColorReset} built")
