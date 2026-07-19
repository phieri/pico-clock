cmake_minimum_required(VERSION 3.21)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(DEFINED ENV{PICO_SDK_PATH} AND NOT "$ENV{PICO_SDK_PATH}" STREQUAL "")
  set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
elseif(NOT DEFINED PICO_SDK_PATH OR PICO_SDK_PATH STREQUAL "")
  set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}/.deps/pico-sdk")
endif()

set(PICO_BOARD pico2_w CACHE STRING "Pico board variant" FORCE)
set(PICO_PLATFORM rp2350 CACHE STRING "Pico platform" FORCE)

if(NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
  message(FATAL_ERROR
    "PICO_SDK_PATH does not point at a pico-sdk checkout. Run scripts/bootstrap-pico.sh first.")
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(pico_clock C CXX ASM)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

pico_sdk_init()

add_library(littlefs STATIC
  ${CMAKE_SOURCE_DIR}/.deps/littlefs/lfs.c
  ${CMAKE_SOURCE_DIR}/.deps/littlefs/lfs_util.c
)
target_include_directories(littlefs PUBLIC ${CMAKE_SOURCE_DIR}/.deps/littlefs)

add_executable(pico_clock
  src/main.c
  src/clock.c
  src/display.c
  src/cpr_compat.c
  src/network.c
  src/config.c
)

target_include_directories(pico_clock PRIVATE
  ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_arch/include
  ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_driver/include
  ${PICO_SDK_PATH}/lib/cyw43-driver/src
)

target_compile_definitions(pico_clock PRIVATE
  PICO_CYW43_ARCH_HEADER=pico/cyw43_arch/arch_poll.h
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
  hardware_flash
  littlefs
)

pico_enable_stdio_usb(pico_clock 1)
pico_enable_stdio_uart(pico_clock 0)
pico_add_extra_outputs(pico_clock)
