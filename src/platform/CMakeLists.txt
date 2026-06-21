cmake_minimum_required(VERSION 3.15)

add_library(platform_headers INTERFACE)
target_include_directories(platform_headers INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/src/test"
)

add_library(platform INTERFACE)
target_link_libraries(platform INTERFACE platform_headers)

if(BUILD_PLATFORM STREQUAL "VM")
    add_library(platform_vm STATIC platform_vm.c)
    target_link_libraries(platform_vm PUBLIC platform_headers PRIVATE vm)
    target_link_libraries(platform INTERFACE platform_vm)
else()
    add_library(platform_arm STATIC platform_arm.c)
    target_link_libraries(platform_arm PUBLIC platform_headers PRIVATE bsp)
    target_link_libraries(platform INTERFACE platform_arm)
endif()
