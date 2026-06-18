cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(FREERTOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB_RECURSE FREERTOS_SOURCES
    "${FREERTOS_DIR}/*.c"
    "${FREERTOS_DIR}/portable/GCC/ARM_CM0/port.c"
    "${FREERTOS_DIR}/portable/GCC/ARM_CM0/portasm.c"
    "${FREERTOS_DIR}/portable/MemMang/heap_4.c"
)
list(REMOVE_ITEM FREERTOS_SOURCES
    "${FREERTOS_DIR}/croutine.c"
    "${FREERTOS_DIR}/event_groups.c"
    "${FREERTOS_DIR}/stream_buffer.c"
    "${FREERTOS_DIR}/timers.c"
)

set(FREERTOS_INCLUDES
    "${CFG_DIR}"
    "${FREERTOS_DIR}"
    "${FREERTOS_DIR}/include"
    "${FREERTOS_DIR}/portable/GCC/ARM_CM0"
)

add_library(freertos STATIC)
target_include_directories(freertos PUBLIC "${FREERTOS_INCLUDES}")
target_sources(freertos PRIVATE "${FREERTOS_SOURCES}")

print_item("FreeRTOS" "library ${ColorBold}freertos${ColorReset} built")
