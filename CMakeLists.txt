cmake_minimum_required(VERSION 3.15)

set(_framework_use_keys
    FRAMEWORK_USE_FREERTOS
    FRAMEWORK_USE_LVGL
    FRAMEWORK_USE_LFS
    FRAMEWORK_USE_RTT
    FRAMEWORK_USE_UART
)

set(FRAMEWORK_RUNTIME_MODE "game" CACHE STRING "Firmware runtime mode: game, flash_mgr, or test")
set_property(CACHE FRAMEWORK_RUNTIME_MODE PROPERTY STRINGS game flash_mgr test)

set(FRAMEWORK_RUNTIME_MODE_GAME 1)
set(FRAMEWORK_RUNTIME_MODE_FLASH_MGR 2)
set(FRAMEWORK_RUNTIME_MODE_TEST 3)

if(FRAMEWORK_RUNTIME_MODE STREQUAL "game")
    set(FRAMEWORK_RUNTIME_MODE_CURRENT ${FRAMEWORK_RUNTIME_MODE_GAME})
elseif(FRAMEWORK_RUNTIME_MODE STREQUAL "flash_mgr")
    set(FRAMEWORK_RUNTIME_MODE_CURRENT ${FRAMEWORK_RUNTIME_MODE_FLASH_MGR})
elseif(FRAMEWORK_RUNTIME_MODE STREQUAL "test")
    set(FRAMEWORK_RUNTIME_MODE_CURRENT ${FRAMEWORK_RUNTIME_MODE_TEST})
else()
    message(FATAL_ERROR
        "Unsupported FRAMEWORK_RUNTIME_MODE='${FRAMEWORK_RUNTIME_MODE}'. "
        "Expected one of: game, flash_mgr, test")
endif()

if(NOT BUILD_PLATFORM STREQUAL "VM" AND NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain.cmake" CACHE FILEPATH "ARM Toolchain")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(BUILD_PLATFORM STREQUAL "VM")
    project(framework LANGUAGES C CXX)
else()
    project(framework LANGUAGES C CXX ASM)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(
    FRAMEWORK_RUNTIME_MODE_GAME=${FRAMEWORK_RUNTIME_MODE_GAME}
    FRAMEWORK_RUNTIME_MODE_FLASH_MGR=${FRAMEWORK_RUNTIME_MODE_FLASH_MGR}
    FRAMEWORK_RUNTIME_MODE_TEST=${FRAMEWORK_RUNTIME_MODE_TEST}
    FRAMEWORK_RUNTIME_MODE_CURRENT=${FRAMEWORK_RUNTIME_MODE_CURRENT}
)

foreach(_k IN LISTS _framework_use_keys)
    if(${_k})
        add_compile_definitions(${_k}=1)
    else()
        add_compile_definitions(${_k}=0)
    endif()
endforeach()

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(CFG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(BUILD_PLATFORM STREQUAL "VM")
    add_compile_options(-fdiagnostics-color=always -Wall)
else()
    set(CPU_FLAGS
        -mcpu=cortex-m0plus
        -march=armv6-m
        -mthumb
        -mfloat-abi=soft
    )
    add_compile_options(
        ${CPU_FLAGS}
        -fdiagnostics-color=always
        -Wall
        -ffunction-sections
        -fdata-sections
        -fno-unwind-tables
        -fno-asynchronous-unwind-tables
        -mno-unaligned-access
    )
endif()

add_subdirectory(src/platform)
add_subdirectory(lib)
add_subdirectory(src/app)

file(GLOB ASSETS_C "${ASSETS_DIR}/*.c")
platform_add_executable("${SRC_DIR}/main.c" ${ASSETS_C})

print_blankline()
