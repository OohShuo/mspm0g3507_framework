# cmake/tools.cmake

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(TAG "Tools")

set(SYSCFG_INSTALL_DIR "${CMAKE_SOURCE_DIR}/tools/sysconfig" CACHE PATH "Directory for installing syscfg tool")
set(SYSCFG_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/syscfg" CACHE PATH "Directory for generated syscfg files")

set(SYSCFG_CLI "${SYSCFG_INSTALL_DIR}/sysconfig_cli.sh")
set(SYSCFG_GUI "${SYSCFG_INSTALL_DIR}/sysconfig_gui.sh")
set(PRODUCT_JSON_FILE "${CMAKE_SOURCE_DIR}/tools/product.json")

print_blankline()
print_item(${TAG} "SysConfig install directory: ${SYSCFG_INSTALL_DIR}")
print_item(${TAG} "SysConfig CLI: ${SYSCFG_CLI}")
print_item(${TAG} "SysConfig GUI: ${SYSCFG_GUI}")

function(syscfg_gen)
    set(SYSCFG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/config/${PROJECT_NAME}.syscfg")

    set(TEMP_GEN_DIR "${CMAKE_BINARY_DIR}/syscfg_temp")
    file(MAKE_DIRECTORY "${TEMP_GEN_DIR}")

    add_custom_command(
        OUTPUT
        "${SYSCFG_GEN_DIR}/ti_msp_dl_config.c"
        "${SYSCFG_GEN_DIR}/ti_msp_dl_config.h"
        "${SYSCFG_GEN_DIR}/device.opt"

        COMMAND ${SYSCFG_CLI}
        --compiler gcc
        --product "${PRODUCT_JSON_FILE}"
        --output "${TEMP_GEN_DIR}"
        --quiet
        "${SYSCFG_FILE}"

        COMMAND ${CMAKE_COMMAND} -E copy "${TEMP_GEN_DIR}/ti_msp_dl_config.c" "${SYSCFG_GEN_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy "${TEMP_GEN_DIR}/ti_msp_dl_config.h" "${SYSCFG_GEN_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy "${TEMP_GEN_DIR}/device.opt" "${SYSCFG_GEN_DIR}/"

        DEPENDS "${SYSCFG_FILE}"
        COMMENT "Generating and cleaning SysConfig files..."
        VERBATIM
    )

    print_blankline()
    message(STATUS "${ColorGreen}[SysConfig]${ColorReset} Configuration file setup for ${SYSCFG_GEN_DIR}")
endfunction()
