cmake_minimum_required(VERSION 3.21)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED PICO_SDK_PATH)
  set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}/.deps/pico-sdk")
endif()

if(NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
  message(FATAL_ERROR
    "PICO_SDK_PATH does not point at a pico-sdk checkout. Run scripts/bootstrap-pico.sh first.")
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(pico_clock C CXX ASM)

set(PICO_BOARD pico2_w CACHE STRING "Pico board variant" FORCE)
set(PICO_PLATFORM rp2350 CACHE STRING "Pico platform" FORCE)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

pico_sdk_init()

add_executable(pico_clock
  src/main.c
  src/clock.c
  src/network.c
)

if(DEFINED WIFI_SSID)
  target_compile_definitions(pico_clock PRIVATE WIFI_SSID_DEFAULT=\"${WIFI_SSID}\")
endif()

if(DEFINED WIFI_PASSWORD)
  target_compile_definitions(pico_clock PRIVATE WIFI_PASSWORD_DEFAULT=\"${WIFI_PASSWORD}\")
endif()

target_link_libraries(pico_clock
  pico_stdlib
  pico_time
  pico_cyw43_arch_lwip_poll
  pico_lwip_nosys
)

pico_enable_stdio_usb(pico_clock 1)
pico_enable_stdio_uart(pico_clock 0)
pico_add_extra_outputs(pico_clock)
