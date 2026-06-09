cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

file(GLOB_RECURSE APP_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

# # When a backing library is OFF, drop the apps that would otherwise pull
# # in undefined symbols at link time. main.c still has its own per-app
# # *_ENABLE switches for when the library is ON — those are demo-level
# # toggles, independent from this build-time gate.
# if(NOT FRAMEWORK_USE_LVGL)
#     list(FILTER APP_SRC EXCLUDE REGEX ".*/lvgl_(hello|ball)/.*\\.c$")
# endif()
# if(NOT FRAMEWORK_USE_LFS)
#     list(FILTER APP_SRC EXCLUDE REGEX ".*/(lfs_port|lfs_test)/.*\\.c$")
# endif()
# if(NOT FRAMEWORK_USE_RTT)
#     list(FILTER APP_SRC EXCLUDE REGEX ".*/rtt_test/.*\\.c$")
# endif()

get_subdirs_and_self(${CMAKE_CURRENT_SOURCE_DIR} APP_INC)

add_library(app STATIC "${APP_SRC}")

target_include_directories(app PUBLIC
    "${APP_INC}"
    "${CFG_DIR}"
)

target_link_libraries(app PRIVATE hal bsp lib syscall)

print_item("App" "library ${ColorBold}app${ColorReset} built")
