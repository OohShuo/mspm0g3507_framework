cmake_minimum_required(VERSION 3.15)

set(_framework_use_keys
    FRAMEWORK_USE_FREERTOS
    FRAMEWORK_USE_LVGL
    FRAMEWORK_USE_LFS
    FRAMEWORK_USE_RTT
    FRAMEWORK_USE_UART
)

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

# ── VM flash image ──
if(BUILD_PLATFORM STREQUAL "VM")
    set(VM_FLASH_IMG "${CMAKE_BINARY_DIR}/vm_flash.bin")
    set(MK_VM_FLASH_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/mk_vm_flash.py")
    set(VM_FLASH_ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets/vm_flash")

    file(GLOB VM_FLASH_ASSETS "${VM_FLASH_ASSETS_DIR}/*")
    add_custom_command(
        OUTPUT "${VM_FLASH_IMG}"
        COMMAND python3 "${MK_VM_FLASH_SCRIPT}" -o "${VM_FLASH_IMG}" --assets-dir "${VM_FLASH_ASSETS_DIR}"
        DEPENDS ${VM_FLASH_ASSETS} "${MK_VM_FLASH_SCRIPT}"
        COMMENT "Building VM LittleFS flash image..."
        VERBATIM
    )
    add_custom_target(vm_flash_image DEPENDS "${VM_FLASH_IMG}")
    add_dependencies(${PROJECT_NAME}_vm vm_flash_image)
endif()

print_blankline()
