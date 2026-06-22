cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(PLATFORM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "Platform source directory")

get_subdirs_and_self("${CMAKE_SOURCE_DIR}/src/hal" PLATFORM_HAL_INC)
get_subdirs_and_self("${CMAKE_SOURCE_DIR}/src/bsp" PLATFORM_BSP_INC)

add_library(platform_headers INTERFACE)
target_include_directories(platform_headers INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/config"
    "${CMAKE_SOURCE_DIR}/src/test"
)

add_library(platform INTERFACE)
target_link_libraries(platform INTERFACE platform_headers)

if(BUILD_PLATFORM STREQUAL "VM")
    find_package(SDL2 REQUIRED)

    add_library(framework_sdl2 INTERFACE)
    target_compile_definitions(framework_sdl2 INTERFACE SDL_MAIN_HANDLED)

    if(TARGET SDL2::SDL2)
        target_link_libraries(framework_sdl2 INTERFACE SDL2::SDL2)
    else()
        target_include_directories(framework_sdl2 INTERFACE ${SDL2_INCLUDE_DIRS})

        set(_FRAMEWORK_SDL2_LIBRARIES ${SDL2_LIBRARIES})
        list(FILTER _FRAMEWORK_SDL2_LIBRARIES EXCLUDE REGEX ".*SDL2main.*")
        target_link_libraries(framework_sdl2 INTERFACE ${_FRAMEWORK_SDL2_LIBRARIES})
    endif()

    target_include_directories(platform_headers INTERFACE
        "${CMAKE_SOURCE_DIR}/src/vm/ti"
        "${CMAKE_SOURCE_DIR}/src/vm"
        "${CMAKE_SOURCE_DIR}/src/vm/syscall"
        "${CMAKE_SOURCE_DIR}/src/vm/freertos"
        "${PLATFORM_HAL_INC}"
        "${PLATFORM_BSP_INC}"
        "${CMAKE_SOURCE_DIR}/src/syscall"
    )

    add_subdirectory("${CMAKE_SOURCE_DIR}/src/vm" "${CMAKE_BINARY_DIR}/src/vm")
    add_library(platform_vm STATIC platform_vm.c)
    target_link_libraries(platform_vm PUBLIC platform_headers PRIVATE vm)
    target_link_libraries(platform INTERFACE platform_vm vm)
else()
    include(${CMAKE_SOURCE_DIR}/cmake/tools.cmake)
    set(SYSCFG_FILE "${CMAKE_SOURCE_DIR}/config/${PROJECT_NAME}.syscfg")
    file(GLOB SYSCFG_GEN_C "${SYSCFG_GEN_DIR}/*.c")
    syscfg_gen()

    add_subdirectory("${CMAKE_SOURCE_DIR}/ti_device" "${CMAKE_BINARY_DIR}/ti_device")
    add_subdirectory("${CMAKE_SOURCE_DIR}/lib/freertos" "${CMAKE_BINARY_DIR}/lib/freertos")
    add_subdirectory("${CMAKE_SOURCE_DIR}/src/syscall" "${CMAKE_BINARY_DIR}/src/syscall")
    add_subdirectory("${CMAKE_SOURCE_DIR}/src/bsp" "${CMAKE_BINARY_DIR}/src/bsp")
    add_subdirectory("${CMAKE_SOURCE_DIR}/src/hal" "${CMAKE_BINARY_DIR}/src/hal")

    add_library(platform_arm STATIC platform_arm.c)
    target_link_libraries(platform_arm PUBLIC platform_headers PRIVATE bsp)
    target_link_libraries(platform INTERFACE platform_arm hal bsp syscall)
endif()

function(platform_add_executable main_source)
    if(BUILD_PLATFORM STREQUAL "VM")
        add_executable(${PROJECT_NAME}_vm "${main_source}" ${ARGN})
        target_link_libraries(${PROJECT_NAME}_vm PRIVATE app platform lib)
    else()
        add_subdirectory("${CMAKE_SOURCE_DIR}/src/test" "${CMAKE_BINARY_DIR}/src/test")
        add_executable(${PROJECT_NAME}.elf
            "${main_source}"
            "${PLATFORM_SOURCE_DIR}/it_arm.c"
            ${ARGN}
        )
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
        target_link_libraries(${PROJECT_NAME}.elf PRIVATE app platform lib test)
        add_custom_command(
            TARGET ${PROJECT_NAME}.elf POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex
            COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
            COMMENT "Generating hex/bin files and checking size for ${PROJECT_NAME}.elf"
        )
    endif()
endfunction()
