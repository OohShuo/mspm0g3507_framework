cmake_minimum_required(VERSION 3.15)

project(tools LANGUAGES C ASM)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(TAG "Tools")

set(SYSCFG_INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sysconfig" CACHE PATH "Directory for installing syscfg tool")
set(SYSCFG_CLI "${SYSCFG_INSTALL_DIR}/sysconfig_cli.sh" CACHE FILEPATH "Path to sysconfig_cli.sh")
set(SYSCFG_GUI "${SYSCFG_INSTALL_DIR}/sysconfig_gui.sh" CACHE FILEPATH "Path to sysconfig_gui.sh")

print_blankline()

print_item(${TAG} "SysConfig install directory: ${SYSCFG_INSTALL_DIR}")
print_item(${TAG} "SysConfig CLI: ${SYSCFG_CLI}")
print_item(${TAG} "SysConfig GUI: ${SYSCFG_GUI}")

function(syscfg_gen syscfg_file output_dir)
    set(TEMP_GEN_DIR "${CMAKE_BINARY_DIR}/syscfg_temp")
    file(MAKE_DIRECTORY "${TEMP_GEN_DIR}")

    add_custom_command(
        OUTPUT 
            "${output_dir}/ti_msp_dl_config.c"
            "${output_dir}/ti_msp_dl_config.h"
            "${output_dir}/device.opt"
        
        COMMAND ${SYSCFG_CLI} 
            --compiler gcc
            --product "${PRODUCT_JSON_FILE}"
            --output "${TEMP_GEN_DIR}"
            --quiet
            "${syscfg_file}"
        
        COMMAND ${CMAKE_COMMAND} -E copy "${TEMP_GEN_DIR}/excluded/ti_msp_dl_config.c" "${output_dir}/"
        COMMAND ${CMAKE_COMMAND} -E copy "${TEMP_GEN_DIR}/excluded/ti_msp_dl_config.h" "${output_dir}/"
        COMMAND ${CMAKE_COMMAND} -E copy "${TEMP_GEN_DIR}/excluded/device.opt" "${output_dir}/"
        
        
        DEPENDS "${syscfg_file}"
        COMMENT "Generating and cleaning SysConfig files..."
        VERBATIM
    )

    include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)
    print_blankline()
    message(STATUS "${ColorGreen}[SysConfig]${ColorReset} Configuration file generated in ${output_dir}")
endfunction()