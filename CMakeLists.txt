cmake_minimum_required(VERSION 3.15)

set(_framework_use_keys
    FRAMEWORK_USE_FREERTOS
    FRAMEWORK_USE_LVGL
    FRAMEWORK_USE_LFS
    FRAMEWORK_USE_RTT
    FRAMEWORK_USE_UART
)

# ===== Virtual Device (x86, SDL2) =====
if(BUILD_PLATFORM STREQUAL "VM")
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    project(framework LANGUAGES C CXX)

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

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

    file(GLOB ASSETS_C "${ASSETS_DIR}/*.c")
    set(ENTRANCE_MAIN_C "${SRC_DIR}/vm/main_vm.c")

    add_compile_options(-fdiagnostics-color=always -Wall)

    find_package(SDL2 REQUIRED)

    add_executable(${PROJECT_NAME}_vm "${ENTRANCE_MAIN_C}" "${ASSETS_C}")

    # Dummy targets for HAL/BSP/syscall/ti — the VM library replaces all of them
    add_library(hal INTERFACE)
    add_library(bsp INTERFACE)
    add_library(syscall INTERFACE)
    add_library(ti INTERFACE)
    # lib/lfs links against freertos; VM provides it via vm
    add_library(freertos INTERFACE)
    target_include_directories(freertos INTERFACE "${CMAKE_SOURCE_DIR}/src/vm/freertos")
    target_link_libraries(freertos INTERFACE vm)

    add_subdirectory(lib)
    add_subdirectory(src/app)
    add_subdirectory(src/vm)

    # app inherits VM's PUBLIC include paths (hal, bsp, freertos headers)
    target_link_libraries(app PRIVATE vm)

    target_link_libraries(${PROJECT_NAME}_vm PRIVATE vm app lib)
    target_link_libraries(${PROJECT_NAME}_vm PRIVATE ${SDL2_LIBRARIES})
    target_include_directories(${PROJECT_NAME}_vm PRIVATE ${SDL2_INCLUDE_DIRS})

    print_blankline()

# ===== ARM Embedded (cross-compile) =====
else()
    if(NOT CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain.cmake" CACHE FILEPATH "ARM Toolchain")
    endif()

    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    project(framework LANGUAGES C CXX ASM)

    foreach(_k IN LISTS _framework_use_keys)
        if(${_k})
            add_compile_definitions(${_k}=1)
        else()
            add_compile_definitions(${_k}=0)
        endif()
    endforeach()

    include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)
    include(${CMAKE_SOURCE_DIR}/cmake/tools.cmake)

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
    set(CFG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
    set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

    file(GLOB ENTRANCE_MAIN_C "${SRC_DIR}/main.c")
    file(GLOB IT_C "${SRC_DIR}/it.c")
    file(GLOB SYSCFG_GEN_C "${SYSCFG_GEN_DIR}/*.c")
    file(GLOB LVGL_CONF_H "${CFG_DIR}/lvgl_config.h")
    file(GLOB ASSETS_C "${ASSETS_DIR}/*.c")

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
        -mno-unaligned-access
    )

    add_executable(${PROJECT_NAME}.elf "${ENTRANCE_MAIN_C}" "${IT_C}" "${ASSETS_C}")

    add_subdirectory(ti_device)
    add_subdirectory(lib)
    add_subdirectory(src/app)
    add_subdirectory(src/hal)
    add_subdirectory(src/bsp)
    add_subdirectory(src/syscall)
    add_subdirectory(src/test)

    target_link_options(${PROJECT_NAME}.elf PRIVATE
        ${CPU_FLAGS}
        "-T${TI_LINKER_SCRIPTS}"
        "-Wl,--cref"
        "-Wl,-Map,${PROJECT_NAME}.map"
        "--specs=nano.specs"
        "--specs=nosys.specs"
        "-Wl,--gc-sections"
        "-nostartfiles"
        "-Wl,--undefined=Reset_Handler"
        "-Wl,--print-memory-usage"
    )

    target_link_libraries(${PROJECT_NAME}.elf PRIVATE ti app hal bsp lib syscall test)

    add_custom_command(
        TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex
        COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
        COMMENT "Generating hex/bin files and checking size for ${PROJECT_NAME}.elf"
    )

    syscfg_gen()

    print_blankline()
endif()