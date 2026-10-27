cmake_minimum_required(VERSION 3.21)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(DEFINED ENV{PICO_SDK_PATH} AND NOT "$ENV{PICO_SDK_PATH}" STREQUAL "")
  set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
elseif(NOT DEFINED PICO_SDK_PATH OR PICO_SDK_PATH STREQUAL "")
  set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}/.deps/pico-sdk")
endif()

set(PICO_BOARD pico2_w CACHE STRING "Pico board variant")
if(NOT PICO_BOARD STREQUAL "pico2_w")
  message(FATAL_ERROR "The pico-clock firmware currently requires PICO_BOARD=pico2_w")
endif()

set(PICO_CLOCK_DISPLAY "dvi" CACHE STRING "Display output variant (dvi or spi)")
set_property(CACHE PICO_CLOCK_DISPLAY PROPERTY STRINGS dvi spi)
string(TOLOWER "${PICO_CLOCK_DISPLAY}" PICO_CLOCK_DISPLAY)

if(PICO_CLOCK_DISPLAY STREQUAL "dvi")
  set(DISPLAY_OUTPUT_SOURCE src/display_output.c)
elseif(PICO_CLOCK_DISPLAY STREQUAL "spi")
  set(DISPLAY_OUTPUT_SOURCE src/display_output_spi.c)
else()
  message(FATAL_ERROR "PICO_CLOCK_DISPLAY must be either dvi or spi")
endif()

if(NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
  message(FATAL_ERROR
    "PICO_SDK_PATH does not point at a pico-sdk checkout. Run scripts/bootstrap-pico.sh first.")
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(pico_clock C CXX ASM)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

pico_sdk_init()

if(PICO_CLOCK_DISPLAY STREQUAL "dvi")
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.deps/PicoDVI/software/libdvi/CMakeLists.txt")
    message(FATAL_ERROR "PicoDVI is missing. Run scripts/bootstrap-pico.sh first.")
  endif()
  add_subdirectory(.deps/PicoDVI/software/libdvi)
endif()

add_library(littlefs STATIC
  ${CMAKE_SOURCE_DIR}/.deps/littlefs/lfs.c
  ${CMAKE_SOURCE_DIR}/.deps/littlefs/lfs_util.c
)
target_include_directories(littlefs PUBLIC ${CMAKE_SOURCE_DIR}/.deps/littlefs)

add_executable(pico_clock
  src/main.c
  src/clock.c
  src/display.c
  ${DISPLAY_OUTPUT_SOURCE}
  src/cpr_compat.c
  src/network.c
  src/config.c
  src/runtime.c
)

target_include_directories(pico_clock PRIVATE
  ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_arch/include
  ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_driver/include
  ${PICO_SDK_PATH}/lib/cyw43-driver/src
)

target_compile_definitions(pico_clock PRIVATE
  PICO_CYW43_ARCH_HEADER=pico/cyw43_arch/arch_threadsafe_background.h
  LWIP_NETIF_HOSTNAME=1
)

if(PICO_CLOCK_DISPLAY STREQUAL "dvi")
  target_compile_definitions(pico_clock PRIVATE
    DVI_VERTICAL_REPEAT=1
    DVI_N_TMDS_BUFFERS=3
    DVI_MONOCHROME_TMDS=1
  )
else()
  target_compile_definitions(pico_clock PRIVATE PICO_CLOCK_DISPLAY_SPI=1)
endif()

target_link_libraries(pico_clock
  pico_stdlib
  pico_time
  pico_multicore
  pico_cyw43_arch_lwip_threadsafe_background
  pico_lwip_nosys
  hardware_flash
  littlefs
)

if(PICO_CLOCK_DISPLAY STREQUAL "dvi")
  target_link_libraries(pico_clock libdvi)
else()
  target_link_libraries(pico_clock hardware_spi)
endif()

pico_enable_stdio_usb(pico_clock 1)
pico_enable_stdio_uart(pico_clock 0)
pico_add_extra_outputs(pico_clock)
