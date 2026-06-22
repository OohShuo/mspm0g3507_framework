cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

file(GLOB_RECURSE VM_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
set(VM_FREERTOS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/freertos/freertos_stubs.c")
list(REMOVE_ITEM VM_SRC "${VM_FREERTOS_SRC}")

add_library(freertos STATIC "${VM_FREERTOS_SRC}")
target_include_directories(freertos PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/freertos"
    "${CFG_DIR}"
)
target_link_libraries(freertos PUBLIC framework_sdl2)

add_library(vm STATIC ${VM_SRC})
target_sources(vm PRIVATE "${CMAKE_SOURCE_DIR}/src/hal/buzzer/buzzer_def.c")
target_include_directories(vm PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/ti"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/syscall"
    "${CMAKE_CURRENT_SOURCE_DIR}/freertos"
)
target_link_libraries(vm PUBLIC platform_headers freertos framework_sdl2 PRIVATE lib)

print_item("VM" "library ${ColorBold}vm${ColorReset} built (SDL2 virtual devices)")
