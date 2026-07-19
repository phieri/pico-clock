# pico-clock

A Raspberry Pi Pico 2 W firmware scaffold for a network clock that uses open Wi-Fi, probes the captive-portal check URL, prefers IPv6 networking with IPv4 fallback, synchronizes from multiple NTP servers at boot, tracks drift and latency, and formats the current time as HH:mm:ss.

![Pico Clock display preview](docs/pico-clock-screenshot.png)

## What is included
- CMake-based Pico SDK project targeting `pico2_w`
- Wi-Fi connection setup for the Pico W using the Raspberry Pi CYW43 driver
- A simple NTP client that requests the current time from an NTP server at boot
- An HTTP probe against http://networkcheck.kde.org/ that validates open Wi-Fi connectivity and rejects password-protected networks
- Drift tracking based on boot-time sync and subsequent corrections
- A framebuffer-based display loop that draws the current time and drift on a 1024x600 display surface

## Refactor highlights
- Refactored the runtime loop in `src/main.c` into focused helpers for serial input handling and display refreshes.
- Simplified configuration persistence in `src/config.c` with shared helpers for defaults, load/save paths, and command parsing.
- Split the display drawing routine in `src/display.c` into reusable framing and text helpers for easier maintenance.

## Build
1. Install the ARM toolchain and CMake.
2. Bootstrap the Pico SDK and extras:
   - `./scripts/bootstrap-pico.sh`
3. Configure and build:
   - `cmake -S . -B build -DPICO_SDK_PATH=$PWD/.deps/pico-sdk`
   - `cmake --build build -j2`

## Notes
The initial scaffold uses the Pico W's wireless stack and an NTP sync loop. To connect to a real network, update the Wi-Fi SSID default in `src/main.c` before flashing the firmware to hardware. Password-protected networks are intentionally unsupported; the firmware only autoconnects to open Wi-Fi and verifies connectivity by probing `http://networkcheck.kde.org/`.
