cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

file(GLOB_RECURSE BSP_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
get_subdirs_and_self(${CMAKE_CURRENT_SOURCE_DIR} BSP_INC)

add_library(bsp STATIC "${BSP_SRC}" "${SYSCFG_GEN_C}")

target_include_directories(bsp PUBLIC "${BSP_INC}" "${CFG_DIR}" "${SYSCFG_GEN_DIR}")

target_link_libraries(bsp PRIVATE lib)
target_link_libraries(bsp PUBLIC ti freertos)
