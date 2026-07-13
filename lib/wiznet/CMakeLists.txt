cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(ETHERNET_ITEMS "")
list(APPEND ETHERNET_ITEMS "W5100" "W5100S" "W5200" "W5300" "W5500" "W6100" "W6300")

set(INTERNET_ITEMS "")
list(APPEND INTERNET_ITEMS "AAC" "DHCP" "DHCP6" "DNS" "httpServer" "MQTT" "SNMP" "SNTP" "TFTP")

file(GLOB_RECURSE APP_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Application/*.c")
file(GLOB ETHERNET_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Ethernet/*.c")
file(GLOB INTERNET_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Internet/*.c")

get_subdirs_and_self("${CMAKE_CURRENT_SOURCE_DIR}/Application" APP_INC)
set(ETHERNET_INC "${CMAKE_CURRENT_SOURCE_DIR}/Ethernet")
set(INTERNET_INC "${CMAKE_CURRENT_SOURCE_DIR}/Internet")

set(ETHERNET_ENABLED_ITEMS "")
set(INTERNET_ENABLED_ITEMS "")

foreach(item IN LISTS ETHERNET_ITEMS)
    if(DEFINED WIZNET_${item}_ENABLED AND WIZNET_${item}_ENABLED)
        file(GLOB_RECURSE ETHERNET_${item}_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Ethernet/${item}/*.c")
        list(APPEND ETHERNET_SRC "${ETHERNET_${item}_SRC}")

        get_subdirs_and_self("${CMAKE_CURRENT_SOURCE_DIR}/Ethernet/${item}" ETHERNET_${item}_INC)
        list(APPEND ETHERNET_INC ${ETHERNET_${item}_INC})

        list(APPEND ETHERNET_ENABLED_ITEMS ${item})
    endif()
endforeach()

foreach(item IN LISTS INTERNET_ITEMS)
    if(DEFINED WIZNET_${item}_ENABLED AND WIZNET_${item}_ENABLED)
        file(GLOB_RECURSE INTERNET_${item}_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Internet/${item}/*.c")
        list(APPEND INTERNET_SRC "${INTERNET_${item}_SRC}")

        get_subdirs_and_self("${CMAKE_CURRENT_SOURCE_DIR}/Internet/${item}" INTERNET_${item}_INC)
        list(APPEND INTERNET_INC ${INTERNET_${item}_INC})

        list(APPEND INTERNET_ENABLED_ITEMS ${item})
    endif()
endforeach()

add_library(wiznet STATIC ${APP_SRC} ${ETHERNET_SRC} ${INTERNET_SRC})

target_include_directories(wiznet PUBLIC ${APP_INC} ${ETHERNET_INC} ${INTERNET_INC} ${CFG_DIR})
target_compile_options(wiznet PUBLIC 
    "-Wno-unused-function"
    "-Wno-unused-variable"
    "-Wno-unused-but-set-variable"
)

if(INTERNET_ENABLED_ITEMS)
    print_item("WiZnet" "Internet modules enabled: ${ColorBold}${INTERNET_ENABLED_ITEMS}${ColorReset}")
endif()
if(ETHERNET_ENABLED_ITEMS)
    print_item("WiZnet" "Ethernet modules enabled: ${ColorBold}${ETHERNET_ENABLED_ITEMS}${ColorReset}")
endif()

print_item("WiZnet" "library ${ColorBold}wiznet${ColorReset} built")
