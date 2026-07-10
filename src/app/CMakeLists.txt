cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

file(GLOB_RECURSE APP_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

if(BUILD_PLATFORM STREQUAL "VM")
    list(FILTER APP_SRC EXCLUDE REGEX ".*/storage/storage\\.c$")
    list(FILTER APP_SRC EXCLUDE REGEX ".*/lfs_port/lfs_port\\.c$")
    list(FILTER APP_SRC EXCLUDE REGEX ".*/flash_mgr/flash_mgr\\.c$")
endif()

get_subdirs_and_self(${CMAKE_CURRENT_SOURCE_DIR} APP_INC)

add_library(app STATIC "${APP_SRC}")

target_include_directories(app PUBLIC
    "${APP_INC}"
    "${CMAKE_CURRENT_SOURCE_DIR}/game_assets"
    "${CFG_DIR}"
)

if(BUILD_PLATFORM STREQUAL "VM")
    target_include_directories(app BEFORE PUBLIC "${CMAKE_SOURCE_DIR}/src/vm")
endif()

target_link_libraries(app PRIVATE platform lib)

print_item("App" "library ${ColorBold}app${ColorReset} built")
