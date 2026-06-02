cmake_minimum_required(VERSION 3.15)

project(device LANGUAGES C ASM)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(TAG "Device")

if(${BOARD_TYPE} STREQUAL "MSPM0G3507")
    set(STARTUP_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g350x_gcc.c" CACHE INTERNAL "Startup file for MSPM0G3507")
    set(DEVICE_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/ti/devices/msp/m0p/linker_files/gcc/mspm0g3507.lds" CACHE INTERNAL "Linker script for MSPM0G3507")
    set(TI_DRIVERLIB_A "${CMAKE_CURRENT_SOURCE_DIR}/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x/driverlib.a" CACHE INTERNAL "TI DriverLib static library for MSPM0G3507")
    set(DL_SYSCTL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/ti/driverlib/m0p/sysctl/dl_sysctl_mspm0g1x0x_g3x0x.c" CACHE INTERNAL "Source file for sysctl driver")
else()
    error("Unsupported BOARD_TYPE: ${BOARD_TYPE}.")
endif()

set(PRODUCT_JSON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/product.json" CACHE INTERNAL "Path to the product.json file")

print_blankline()

print_item(${TAG} "Using board type: ${BOARD_TYPE}")

print_item(${TAG} "Product JSON file: ${PRODUCT_JSON_FILE}")
print_item(${TAG} "Startup file: ${STARTUP_FILE}")
print_item(${TAG} "Linker script: ${DEVICE_LINKER_SCRIPT}")
print_item(${TAG} "TI DriverLib: ${TI_DRIVERLIB_A}")
print_item(${TAG} "Sysctl Source: ${DL_SYSCTL_SOURCE}")
