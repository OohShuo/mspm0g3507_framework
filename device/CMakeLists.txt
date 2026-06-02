cmake_minimum_required(VERSION 3.15)
project(device_support LANGUAGES C ASM)

if(${BOARD_TYPE} STREQUAL "MSPM0G3507")
    set(STARTUP_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g350x_gcc.c" CACHE INTERNAL "Startup file for MSPM0G3507")
    set(DEVICE_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/ti/devices/msp/m0p/linker_files/gcc/mspm0g3507.lds" CACHE INTERNAL "Linker script for MSPM0G3507")
    set(TI_DRIVERLIB_A "${CMAKE_CURRENT_SOURCE_DIR}/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x/driverlib.a" CACHE INTERNAL "TI DriverLib static library for MSPM0G3507")
    set(DL_SYSCTL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/ti/driverlib/m0p/sysctl/dl_sysctl_mspm0g1x0x_g3x0x.c" CACHE INTERNAL "Source file for sysctl driver")
else()
    error("Unsupported BOARD_TYPE: ${BOARD_TYPE}.")
endif()

# add_library(device STATIC
#     ${STARTUP_FILE}
#     ${DL_SYSCTL_SOURCE}
# )

# target_include_directories(device PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/cmsis/Core/Include
#     ${CMAKE_CURRENT_SOURCE_DIR}/ti/driverlib
#     ${CMAKE_CURRENT_SOURCE_DIR}/ti/driverlib/m0p
# )

# target_link_libraries(device PUBLIC
#     ${TI_DRIVERLIB_A}
# )
