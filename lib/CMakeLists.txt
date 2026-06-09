add_library(lib INTERFACE)

add_subdirectory(local_lib)
add_subdirectory(freertos)
add_subdirectory(RTT)
add_subdirectory(lfs)

include("${CMAKE_SOURCE_DIR}/cmake/lvgl_config.cmake")
add_subdirectory(lvgl)

target_link_libraries(lib INTERFACE local_lib freertos lvgl rtt littlefs)
