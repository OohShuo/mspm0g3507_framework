cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

file(GLOB_RECURSE LFS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
get_subdirs_and_self(${CMAKE_CURRENT_SOURCE_DIR} LFS_INC)

add_library(littlefs STATIC "${LFS_SRC}")

target_include_directories(littlefs PUBLIC "${LFS_INC}" "${CFG_DIR}" )

target_compile_definitions(littlefs PRIVATE LFS_DEFINES=lfs_config.h)

if(FRAMEWORK_USE_RTT)
    target_link_libraries(littlefs PRIVATE rtt freertos)
else()
    target_link_libraries(littlefs PRIVATE freertos)
endif()

print_item("LittleFS" "library ${ColorBold}littlefs${ColorReset} built")
