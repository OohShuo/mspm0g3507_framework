cmake_minimum_required(VERSION 3.15)
include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

# Compile all .c files under src/vm (hal stubs, bsp stubs, freertos stubs, main, display, input)
file(GLOB_RECURSE VM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
add_library(vm STATIC ${VM_SRC})
target_sources(vm PRIVATE "${CMAKE_SOURCE_DIR}/src/hal/buzzer/buzzer_def.c")

# --- Include paths (order matters for shadowing) ---

# 0. TI stub headers (replace TI DriverLib)
target_include_directories(vm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/ti")

# 1. VM shadow headers override originals
target_include_directories(vm PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"            # display_vm.h, input_vm.h
    "${CMAKE_CURRENT_SOURCE_DIR}/syscall"    # rtt_log.h → shadows src/syscall/
    "${CMAKE_CURRENT_SOURCE_DIR}/freertos"   # FreeRTOS stubs → shadows lib/freertos/
)

# 2. Original HAL headers (type definitions needed by app code)
target_include_directories(vm PUBLIC
    "${CMAKE_SOURCE_DIR}/src/hal"
    "${CMAKE_SOURCE_DIR}/src/hal/st7789"
    "${CMAKE_SOURCE_DIR}/src/hal/button"
    "${CMAKE_SOURCE_DIR}/src/hal/joystick"
    "${CMAKE_SOURCE_DIR}/src/hal/buzzer"
    "${CMAKE_SOURCE_DIR}/src/hal/led_simple"
    "${CMAKE_SOURCE_DIR}/src/hal/led_breath"
    "${CMAKE_SOURCE_DIR}/src/hal/com_uart"
    "${CMAKE_SOURCE_DIR}/src/hal/w25q32"
    "${CMAKE_SOURCE_DIR}/src/hal/ws2812"
)

# 3. Original BSP headers (type definitions)
target_include_directories(vm PUBLIC
    "${CMAKE_SOURCE_DIR}/src/bsp"
    "${CMAKE_SOURCE_DIR}/src/bsp/gpio"
    "${CMAKE_SOURCE_DIR}/src/bsp/time"
    "${CMAKE_SOURCE_DIR}/src/bsp/adc"
    "${CMAKE_SOURCE_DIR}/src/bsp/pwm"
    "${CMAKE_SOURCE_DIR}/src/bsp/spi"
    "${CMAKE_SOURCE_DIR}/src/bsp/spi/soft"
    "${CMAKE_SOURCE_DIR}/src/bsp/spi/hard"
    "${CMAKE_SOURCE_DIR}/src/bsp/uart"
)

# 4. Config
target_include_directories(vm PUBLIC "${CFG_DIR}")

# 5. Original syscall path (needed after VM shadow for headers like retarget.h)
target_include_directories(vm PUBLIC "${CMAKE_SOURCE_DIR}/src/syscall")

# 6. App include paths (needed by main_vm.c)
target_include_directories(vm PRIVATE
    "${CMAKE_SOURCE_DIR}/src/app"
    "${CMAKE_SOURCE_DIR}/src/app/game_console"
    "${CMAKE_SOURCE_DIR}/src/hal"
    "${CMAKE_SOURCE_DIR}/src/test"
    "${CMAKE_SOURCE_DIR}/lib/local_lib"
)

# 7. SDL2
target_include_directories(vm PUBLIC ${SDL2_INCLUDE_DIRS})
target_link_libraries(vm PUBLIC ${FRAMEWORK_SDL2_LIBRARIES} lib)

print_item("VM" "library ${ColorBold}vm${ColorReset} built (SDL2 virtual devices)")
