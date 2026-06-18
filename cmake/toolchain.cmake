set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(CMAKE_HOST_WIN32)
    set(EXEC_SUFFIX ".exe" CACHE STRING "Executable suffix for Windows")
else()
    set(EXEC_SUFFIX "" CACHE STRING "Executable suffix for Unix-like systems")
endif()

set(TOOLCHAIN_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/gcc-arm-none-eabi/bin")
set(LINKER_LIBS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/gcc-arm-none-eabi/arm-none-eabi/lib/thumb/v6-m/nofp")

set(CMAKE_C_COMPILER   "${TOOLCHAIN_BIN_DIR}/arm-none-eabi-gcc${EXEC_SUFFIX}")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_BIN_DIR}/arm-none-eabi-gcc${EXEC_SUFFIX}")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_BIN_DIR}/arm-none-eabi-g++${EXEC_SUFFIX}")
set(CMAKE_AR           "${TOOLCHAIN_BIN_DIR}/arm-none-eabi-gcc-ar${EXEC_SUFFIX}" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB       "${TOOLCHAIN_BIN_DIR}/arm-none-eabi-gcc-ranlib${EXEC_SUFFIX}" CACHE FILEPATH "" FORCE)
set(CMAKE_OBJCOPY      "${TOOLCHAIN_BIN_DIR}/arm-none-eabi-objcopy${EXEC_SUFFIX}")
set(CMAKE_SIZE         "${TOOLCHAIN_BIN_DIR}/arm-none-eabi-size${EXEC_SUFFIX}")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
