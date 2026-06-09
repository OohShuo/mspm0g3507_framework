cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

file(GLOB_RECURSE APP_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
get_subdirs_and_self(${CMAKE_CURRENT_SOURCE_DIR} APP_INC)

add_library(app STATIC "${APP_SRC}")

target_include_directories(app PUBLIC
    "${APP_INC}"
    "${CFG_DIR}"
)

target_link_libraries(app PRIVATE hal bsp lib syscall)

print_item("App" "library ${ColorBold}app${ColorReset} built")
