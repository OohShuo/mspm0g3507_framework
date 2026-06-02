cmake_minimum_required(VERSION 3.15)

project(device LANGUAGES C ASM)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(TAG "Device")

if(${BOARD_TYPE} STREQUAL "MSPM0G3507")
    set(STARTUP_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g350x_gcc.c" CACHE INTERNAL "Startup file for MSPM0G3507")
    set(DEVICE_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/ti/devices/msp/m0p/linker_files/gcc/mspm0g3507.lds" CACHE INTERNAL "Linker script for MSPM0G3507")
    set(TI_DRIVERLIB_A "${CMAKE_CURRENT_SOURCE_DIR}/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x/driverlib.a" CACHE INTERNAL "TI DriverLib static library for MSPM0G3507")
else()
    error("Unsupported BOARD_TYPE: ${BOARD_TYPE}.")
endif()

set(FREERTOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/freertos")

set(FREERTOS_SOURCES
    "${FREERTOS_DIR}/tasks.c"
    "${FREERTOS_DIR}/list.c"
    "${FREERTOS_DIR}/queue.c"
    "${FREERTOS_DIR}/timers.c"
    "${FREERTOS_DIR}/event_groups.c"
    "${FREERTOS_DIR}/stream_buffer.c"
    "${FREERTOS_DIR}/portable/GCC/ARM_CM0/port.c"
    "${FREERTOS_DIR}/portable/GCC/ARM_CM0/portasm.c"
    "${FREERTOS_DIR}/portable/MemMang/heap_4.c"
    CACHE INTERNAL "List of FreeRTOS source files"
)

set(FREERTOS_INCLUDE_DIRS
    "${FREERTOS_DIR}"
    "${FREERTOS_DIR}/include"
    "${FREERTOS_DIR}/portable/GCC/ARM_CM0"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/config"
    CACHE INTERNAL "List of FreeRTOS include directories"
)

print_blankline()

print_item(${TAG} "Using board type: ${BOARD_TYPE}")

print_item(${TAG} "Product JSON file: ${PRODUCT_JSON_FILE}")
print_item(${TAG} "Startup file: ${STARTUP_FILE}")
print_item(${TAG} "Linker script: ${DEVICE_LINKER_SCRIPT}")
print_item(${TAG} "TI DriverLib: ${TI_DRIVERLIB_A}")
print_item(${TAG} "Sysctl Source: ${DL_SYSCTL_SOURCE}")

print_blankline()

print_item(${TAG} "FreeRTOS directory: ${FREERTOS_DIR}")
# print_item(${TAG} "FreeRTOS source files:")
# foreach(src IN LISTS FREERTOS_SOURCES)
#     print_item(${TAG} "  - ${src}")
# endforeach()
# print_item(${TAG} "FreeRTOS include directories:")
# foreach(inc IN LISTS FREERTOS_INCLUDE_DIRS)
#     print_item(${TAG} "  - ${inc}")
# endforeach()
