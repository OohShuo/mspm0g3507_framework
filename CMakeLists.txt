cmake_minimum_required(VERSION 3.15)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain.cmake" CACHE FILEPATH "ARM Toolchain")
endif()

project(framework LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BOARD_TYPE "MSPM0G3507" CACHE STRING "Target board type")

set(CPU_FLAGS 
    -mcpu=cortex-m0plus 
    -march=armv6-m 
    -mthumb 
    -mfloat-abi=soft
)

add_compile_options(
    ${CPU_FLAGS}
    -fdiagnostics-color=always
    -D__MSPM0G3507__
    -O2 -g -std=c99 -Wall
    -ffunction-sections 
    -fdata-sections
)

set(SYSCFG_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/syscfg")

add_subdirectory(device)

add_executable(${PROJECT_NAME}.elf
    "src/app/gpio_toggle_output.c"
    "${SYSCFG_GEN_DIR}/ti_msp_dl_config.c"
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app"
    "${SYSCFG_GEN_DIR}"
)

target_compile_options(${PROJECT_NAME}.elf PUBLIC "@${SYSCFG_GEN_DIR}/device.opt")

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${CPU_FLAGS}
    "-T${DEVICE_LINKER_SCRIPT}"
    "-Wl,-Map,${PROJECT_NAME}.map"
    "--specs=nano.specs"
    "--specs=nosys.specs"
    "-Wl,--gc-sections"
)

target_link_libraries(${PROJECT_NAME}.elf PRIVATE device m c gcc nosys)