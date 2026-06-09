cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(SYSCALL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/retarget.c")
get_subdirs_and_self(${CMAKE_CURRENT_SOURCE_DIR} SYSCALL_INC)

add_library(syscall STATIC "${SYSCALL_SRC}")

target_include_directories(syscall PUBLIC "${SYSCALL_INC}" "${CFG_DIR}")
target_link_libraries(syscall PUBLIC lib)

print_item("SysCall" "library ${ColorBold}syscall${ColorReset} built (newlib retarget)")
