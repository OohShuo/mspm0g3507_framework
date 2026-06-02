cmake_minimum_required(VERSION 3.15)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain.cmake" CACHE FILEPATH "ARM Toolchain")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(framework LANGUAGES C ASM)

set(BOARD_TYPE "MSPM0G3507" CACHE STRING "Target board type")
set(SYSCFG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/config/${PROJECT_NAME}.syscfg")
set(SYSCFG_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/syscfg")

add_subdirectory(device)
add_subdirectory(tools)

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
    -std=c99 -Wall
    -ffunction-sections
    -fdata-sections
)

syscfg_gen(${SYSCFG_FILE} ${SYSCFG_GEN_DIR})

file(GLOB_RECURSE APP_SRC "src/app/*.c")
file(GLOB_RECURSE BSP_SRC "src/bsp/*.c")

add_executable(${PROJECT_NAME}.elf
    ${APP_SRC}
    ${BSP_SRC}
    "${SYSCFG_GEN_DIR}/ti_msp_dl_config.c"
    "${STARTUP_FILE}"
    "${DL_SYSCTL_SOURCE}"
    "${FREERTOS_SOURCES}"
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${SYSCFG_GEN_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/device"
    "${CMAKE_CURRENT_SOURCE_DIR}/device/cmsis/Core/Include"
    "${CMAKE_CURRENT_SOURCE_DIR}/device/ti/driverlib"
    "${CMAKE_CURRENT_SOURCE_DIR}/device/ti/driverlib/m0p"
    "${FREERTOS_INCLUDE_DIRS}"
    "${CMAKE_CURRENT_SOURCE_DIR}/config"
)

target_compile_options(${PROJECT_NAME}.elf PUBLIC "@${SYSCFG_GEN_DIR}/device.opt")

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${CPU_FLAGS}
    "-T${DEVICE_LINKER_SCRIPT}"
    "-Wl,-Map,${PROJECT_NAME}.map"
    "--specs=nano.specs"
    "--specs=nosys.specs"
    "-Wl,--gc-sections"
    "-nostartfiles"
)

target_link_libraries(${PROJECT_NAME}.elf PRIVATE m c gcc nosys
    "${TI_DRIVERLIB_A}")

add_custom_command(
    TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating hex/bin files and checking size for ${PROJECT_NAME}.elf"
)

print_blankline()
