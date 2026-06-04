cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(TI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ti")
set(CMSIS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmsis")

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

add_library(ti STATIC)
target_include_directories(ti PUBLIC "${TI_INCLUDES}")
target_sources(ti PUBLIC "${TI_STARTUP_C}")
target_link_libraries(ti PUBLIC m c gcc nosys "${TI_DRIVERLIB_A}")

target_compile_options(ti INTERFACE 
    "SHELL:@${SYSCFG_GEN_DIR}/device.opt"
)

print_item("TI" "TI Linker scripts: ${TI_LINKER_SCRIPTS}")
print_item("TI" "TI DriverLib: ${TI_DRIVERLIB_A}")
print_item("TI" "library ${ColorBold}ti${ColorReset} built")
