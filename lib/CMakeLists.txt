add_library(lib INTERFACE)

add_subdirectory(local_lib)
add_subdirectory(freertos)

target_link_libraries(lib INTERFACE local_lib freertos)
