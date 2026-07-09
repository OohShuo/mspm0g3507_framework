# cmake/tools.cmake

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

if(CMAKE_HOST_WIN32)
    set(SCRIPTS_SUFFIX ".bat" CACHE STRING "Script suffix for Windows")
    set(REDIRECT_OUTPUT "" CACHE STRING "Redirect output for Windows")
    set(REDIRECT_ERROR "" CACHE STRING "Redirect error for Windows")
else()
    set(SCRIPTS_SUFFIX ".sh" CACHE STRING "Script suffix for Unix-like systems")
    set(REDIRECT_OUTPUT " > /dev/null" CACHE STRING "Redirect output for Unix-like systems")
    set(REDIRECT_ERROR " 2> /dev/null" CACHE STRING "Redirect error for Unix-like systems")
endif()

set(TAG "Tools")

if(NOT DEFINED SYSCONFIG_ROOT OR "${SYSCONFIG_ROOT}" STREQUAL "")
    set(SYSCONFIG_ROOT "${CMAKE_SOURCE_DIR}/tools/sysconfig")
endif()
get_filename_component(SYSCONFIG_ROOT "${SYSCONFIG_ROOT}" ABSOLUTE)

set(SYSCFG_INSTALL_DIR "${SYSCONFIG_ROOT}" CACHE PATH "Directory for installing syscfg tool" FORCE)
set(SYSCFG_GEN_DIR "${CMAKE_SOURCE_DIR}/config/syscfg" CACHE PATH "Directory for generated syscfg files")

set(SYSCFG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/config/${PROJECT_NAME}.syscfg")
set(SYSCFG_CLI "${SYSCFG_INSTALL_DIR}/sysconfig_cli${SCRIPTS_SUFFIX}")
set(SYSCFG_GUI "${SYSCFG_INSTALL_DIR}/sysconfig_gui${SCRIPTS_SUFFIX}")
set(PRODUCT_JSON_FILE "${CMAKE_SOURCE_DIR}/tools/product.json")
set(SYSCFG_GEN_FILES
    "${SYSCFG_GEN_DIR}/ti_msp_dl_config.c"
    "${SYSCFG_GEN_DIR}/ti_msp_dl_config.h"
    "${SYSCFG_GEN_DIR}/device.opt"
)

print_blankline()
print_item(${TAG} "SysConfig install directory: ${SYSCFG_INSTALL_DIR}")
print_item(${TAG} "SysConfig CLI: ${SYSCFG_CLI}")
print_item(${TAG} "SysConfig GUI: ${SYSCFG_GUI}")

function(syscfg_gen)
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
        "${SYSCFG_FILE}" "${REDIRECT_OUTPUT}"

        COMMAND ${CMAKE_COMMAND} -E copy "${TEMP_GEN_DIR}/ti_msp_dl_config.c" "${SYSCFG_GEN_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy "${TEMP_GEN_DIR}/ti_msp_dl_config.h" "${SYSCFG_GEN_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy "${TEMP_GEN_DIR}/device.opt" "${SYSCFG_GEN_DIR}/"

        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "-- ${ColorGreen}[SysConfig]${ColorReset} Configuration file setup for ${SYSCFG_GEN_DIR}"
        COMMAND ${CMAKE_COMMAND} -E echo ""

        DEPENDS "${SYSCFG_FILE}"

        COMMENT "Generating and cleaning SysConfig files..."

        VERBATIM
    )

    add_custom_target(syscfg_gen_target
        DEPENDS ${SYSCFG_GEN_FILES}
    )
endfunction()
