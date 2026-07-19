cmake_minimum_required(VERSION 3.21)
project(pico_clock C ASM)

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

set(PICO_BOARD pico2_w CACHE STRING "Pico board variant")
set(PICO_PLATFORM rp2350 CACHE STRING "Pico platform")

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
pico_sdk_init()

add_executable(pico_clock src/main.c)
target_include_directories(pico_clock PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(pico_clock
  pico_stdlib
  pico_time
  pico_cyw43_arch_lwip_threadsafe_background
  pico_lwip
)

pico_enable_stdio_usb(pico_clock 1)
pico_enable_stdio_uart(pico_clock 0)
pico_add_extra_outputs(pico_clock)
