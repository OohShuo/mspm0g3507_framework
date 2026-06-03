cmake_minimum_required(VERSION 3.15)

add_library(ti_device STATIC)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(TAG "TiDevice")

set(TI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ti")
set(CMSIS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmsis")
set(FREERTOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/freertos")

file(GLOB TI_STARTUP_C "${TI_DIR}/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g350x_gcc.c")
file(GLOB TI_LINKER_SCRIPTS "${TI_DIR}/devices/msp/m0p/linker_files/gcc/*.lds")
file(GLOB TI_DRIVERLIB_A "${TI_DIR}/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x/driverlib.a")

set(TI_LINKER_SCRIPTS "${TI_LINKER_SCRIPTS}" CACHE INTERNAL "TI linker scripts for MSPM0G3507")
set(TI_DRIVERLIB_A "${TI_DRIVERLIB_A}" CACHE INTERNAL "TI DriverLib static library for MSPM0G3507")

set(TI_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMSIS_DIR}/Core/Include"
    "${TI_DIR}"
    "${TI_DIR}/driverlib"
    "${TI_DIR}/driverlib/m0p"
)

file(GLOB_RECURSE FREERTOS_SOURCES
    "${FREERTOS_DIR}/*.c"
    "${FREERTOS_DIR}/portable/GCC/ARM_CM0/port.c"
    "${FREERTOS_DIR}/portable/GCC/ARM_CM0/portasm.c"
    "${FREERTOS_DIR}/portable/MemMang/heap_4.c"
)
list(REMOVE_ITEM FREERTOS_SOURCES
    "${FREERTOS_DIR}/croutine.c"
)

set(FREERTOS_INCLUDES
    "${CFG_DIR}"
    "${FREERTOS_DIR}"
    "${FREERTOS_DIR}/include"
    "${FREERTOS_DIR}/portable/GCC/ARM_CM0"
)

set(DEVICE_SOURCES
    "${TI_STARTUP_C}"
    "${FREERTOS_SOURCES}"
    CACHE INTERNAL "List of device source files"
)

set(DEVICE_INCLUDES
    "${TI_INCLUDES}"
    "${FREERTOS_INCLUDES}"
    CACHE INTERNAL "List of device include directories"
)

target_include_directories(ti_device PUBLIC ${DEVICE_INCLUDES})
target_sources(ti_device PUBLIC ${DEVICE_SOURCES})
target_link_libraries(ti_device PUBLIC m c gcc nosys "${TI_DRIVERLIB_A}")

target_compile_options(ti_device INTERFACE 
    "SHELL:@${SYSCFG_GEN_DIR}/device.opt"
)

print_blankline()

print_item(${TAG} "TI Linker scripts: ${TI_LINKER_SCRIPTS}")
print_item(${TAG} "TI DriverLib: ${TI_DRIVERLIB_A}")
