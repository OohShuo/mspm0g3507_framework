add_library(lib INTERFACE)

add_subdirectory(local_lib)
if(BUILD_PLATFORM STREQUAL "VM")
    # VM provides its own freertos stubs in src/vm/freertos/
    target_include_directories(lib INTERFACE "${CMAKE_SOURCE_DIR}/src/vm/freertos")
    set(_framework_lib_deps local_lib)
else()
    add_subdirectory(freertos)
    set(_framework_lib_deps local_lib freertos)
endif()

if(FRAMEWORK_USE_LVGL)
    include("${CMAKE_SOURCE_DIR}/cmake/lvgl_config.cmake")
    add_subdirectory(lvgl)
    list(APPEND _framework_lib_deps lvgl)
else()
    target_sources(local_lib PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/local_lib/lvgl_stubs.c")
endif()

if(FRAMEWORK_USE_RTT)
    add_subdirectory(RTT)
    list(APPEND _framework_lib_deps rtt)
endif()

if(FRAMEWORK_USE_LFS)
    add_subdirectory(lfs)
    list(APPEND _framework_lib_deps littlefs)
endif()

target_link_libraries(lib INTERFACE ${_framework_lib_deps})
