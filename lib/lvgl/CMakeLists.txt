cmake_minimum_required(VERSION 3.12.4)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(LVGL_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

# =================== #
# Build configuration #
# =================== #

file(GLOB_RECURSE SOURCES
    "${LVGL_ROOT_DIR}/src/*.c"
    "${LVGL_ROOT_DIR}/src/*.cpp"
    "${LVGL_ROOT_DIR}/src/*.S"
)

file(GLOB_RECURSE THORVG_SOURCES
    "${LVGL_ROOT_DIR}/src/libs/thorvg/*.cpp"
    "${LVGL_ROOT_DIR}/src/others/vg_lite_tvg/*.cpp"
)

add_library(lvgl ${SOURCES})

target_link_libraries(lvgl PRIVATE freertos)

# ======================= #
# lv_conf.h configuration #
# ======================= #

if(LV_BUILD_CONF_PATH AND LV_BUILD_CONF_DIR)
    print_error("can not use LV_BUILD_CONF_DIR and LV_BUILD_CONF_PATH at the same time")
endif()

target_compile_definitions(lvgl PUBLIC LV_KCONFIG_IGNORE)

if(LV_BUILD_CONF_PATH)

    set(CONF_PATH ${LV_BUILD_CONF_PATH})
    print_item("LVGL" "Using configuration path: ${CONF_PATH}")

    if(NOT IS_ABSOLUTE ${CONF_PATH})
        file(REAL_PATH ${CONF_PATH} CONF_PATH BASE_DIRECTORY ${CMAKE_SOURCE_DIR})
        print_item("LVGL" "Converted to absolute path: ${CONF_PATH}")
    endif()

    get_filename_component(CONF_INC_DIR ${CONF_PATH} DIRECTORY)

    target_compile_definitions(lvgl PUBLIC LV_CONF_PATH="${CONF_PATH}")

elseif(LV_BUILD_CONF_DIR)

    print_item("LVGL" "Using configuration directory: ${LV_BUILD_CONF_DIR}")

    set(CONF_INC_DIR ${LV_BUILD_CONF_DIR})

    list(APPEND LVGL_PUBLIC_HEADERS ${CONF_INC_DIR}/lv_conf.h)

    # Avoid compilation errors due to the relative path of the include directory
    if(NOT IS_ABSOLUTE ${CONF_INC_DIR})
        file(REAL_PATH ${CONF_INC_DIR} CONF_INC_DIR BASE_DIRECTORY ${CMAKE_SOURCE_DIR})
        print_item("LVGL" "Converted to absolute path: ${CONF_INC_DIR}")
    endif()

    set(CONF_PATH ${CONF_INC_DIR}/lv_conf.h)

    target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)

else()

    print_item("LVGL" "Using lv_conf.h from the top-level project directory")

    list(APPEND LVGL_PUBLIC_HEADERS ${CMAKE_SOURCE_DIR}/lv_conf.h)

    set(CONF_INC_DIR ${CMAKE_SOURCE_DIR})
    set(CONF_PATH ${CONF_INC_DIR}/lv_conf.h)

    target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)

endif()

if(NOT EXISTS ${CONF_PATH})
    print_error("Configuration file: ${CONF_PATH} - not found")
endif()

if(LV_BUILD_LVGL_H_SYSTEM_INCLUDE)
    target_compile_definitions(lvgl PUBLIC LV_LVGL_H_INCLUDE_SYSTEM)
elseif(LV_BUILD_LVGL_H_SIMPLE_INCLUDE)
    target_compile_definitions(lvgl PUBLIC LV_LVGL_H_INCLUDE_SIMPLE)
endif()

include_directories(${CONF_INC_DIR} ${LVGL_ROOT_DIR})

target_include_directories(lvgl SYSTEM PUBLIC ${LVGL_ROOT_DIR} ${CONF_INC_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# ==================== #
# ThorVG configuration #
# ==================== #

get_target_property(COMP_DEF lvgl COMPILE_DEFINITIONS)

if(CONFIG_LV_USE_THORVG_INTERNAL)
    print_item("LVGL" "Enabling the building of ThorVG internal")

    add_library(lvgl_thorvg ${THORVG_SOURCES})
    add_library(lvgl::thorvg ALIAS lvgl_thorvg)
    target_include_directories(lvgl_thorvg PRIVATE ${LVGL_ROOT_DIR}/src/libs/thorvg)
    set_target_properties(lvgl_thorvg PROPERTIES COMPILE_DEFINITIONS "${COMP_DEF}")

    target_link_libraries(lvgl PRIVATE lvgl_thorvg)

    target_link_libraries(lvgl_thorvg PRIVATE lvgl)

endif()

set_source_files_properties(${LVGL_ROOT_DIR}/src/others/vg_lite_tvg/vg_lite_tvg.cpp
    PROPERTIES COMPILE_FLAGS -Wunused-parameter)

# ================ #
# Export variables #
# ================ #

set(LVGL_CONF_INC_DIR ${CONF_INC_DIR} CACHE PATH "Directory containing lv_conf.h")
set(LVGL_CONF_PATH ${CONF_PATH} CACHE PATH "Path to lv_conf.h")
set(LVGL_COMPILER_DEFINES ${COMP_DEF} CACHE STRING "Compiler defines for LVGL")

set(LVGL_VERSION_MAJOR "9")
set(LVGL_VERSION_MINOR "5")
set(LVGL_VERSION_PATCH "0")
set(LVGL_VERSION_INFO "")

set(LVGL_VERSION ${LVGL_VERSION_MAJOR}.${LVGL_VERSION_MINOR}.${LVGL_VERSION_PATCH} CACHE STRING "LVGL version")

# ================ #
# Finalize library #
# ================ #

print_item("LVGL" "library ${ColorBold}lvgl${ColorReset} built with configuration: ${CONF_PATH}")