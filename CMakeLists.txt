cmake_minimum_required(VERSION 3.15)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain.cmake" CACHE FILEPATH "ARM Toolchain")
endif()

project(framework LANGUAGES C ASM)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/tools.cmake)

set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CPU_FLAGS
    -mcpu=cortex-m0plus
    -march=armv6-m
    -mthumb
    -mfloat-abi=soft
)

add_compile_options(
    ${CPU_FLAGS}
    -fdiagnostics-color=always
    -std=c99 -Wall
    -ffunction-sections
    -fdata-sections
)

add_subdirectory(ti_device) 

syscfg_gen()

file(GLOB_RECURSE APP_SRC "src/*.c")
file(GLOB_RECURSE BSP_SRC "src/*.c")

add_executable(${PROJECT_NAME}.elf
    ${APP_SRC}
    ${BSP_SRC}
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${SYSCFG_GEN_DIR}"
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE "@${SYSCFG_GEN_DIR}/device.opt")

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${CPU_FLAGS}
    "-T${TI_LINKER_SCRIPTS}"
    "-Wl,-Map,${PROJECT_NAME}.map"
    "--specs=nano.specs"
    "--specs=nosys.specs"
    "-Wl,--gc-sections"
    "-nostartfiles"
)

target_link_libraries(${PROJECT_NAME}.elf PRIVATE ti_device)

add_custom_command(
    TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating hex/bin files and checking size for ${PROJECT_NAME}.elf"
)

print_blankline()
