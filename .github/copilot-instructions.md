# Copilot instructions for pico-clock

## Repository overview
- This repository builds a Raspberry Pi Pico 2 W firmware image for a network clock.
- The project uses C/C++ and the Raspberry Pi Pico SDK via CMake.
- Primary source files live under `src/`:
  - `src/main.c`: boot-time setup and the runtime loop.
  - `src/network.c`: Wi-Fi connection, captive-portal probing, and NTP sync.
  - `src/display.c`: framebuffer rendering.
  - `src/config.c`: persistent configuration.
  - `src/clock.c`: time and drift logic.
- Keep changes localized to the relevant module unless a cross-cutting refactor is explicitly required.

## Build and validation workflow
- The build is driven by `CMakeLists.txt` and `.github/workflows/build.yml`.
- Use the repository's documented flow for local validation:
  1. Install `cmake`, `gcc-arm-none-eabi`, `libnewlib-arm-none-eabi`, and `build-essential`.
  2. Bootstrap the SDK and local dependencies:
     - `./scripts/bootstrap-pico.sh`
  3. Configure a local build directory:
     - `cmake -S . -B build -DPICO_SDK_PATH=$PWD/.deps/pico-sdk -DPICO_BOARD=pico2_w -DWIFI_SSID=ci_build -DWIFI_PASSWORD=ci_build`
  4. Build the firmware:
     - `cmake --build build -j2`
- Expected outputs are generated under `build/` as `.uf2`, `.elf`, `.bin`, and `.hex` artifacts.
- If you change build-related files or source modules, rerun the configure/build steps before concluding work.

## Repository-specific conventions
- Preserve the existing CMake target names and board selection unless the task explicitly calls for a broader platform change.
- The default board is `pico2_w`; CI also builds for `pico_w`.
- Avoid introducing new build systems, package managers, or dependencies unless the task requires them.
- If you update documentation, keep it aligned with the actual build commands used by the repo and the behavior described in `README.md`.
- Do not commit real Wi-Fi credentials or other secrets; use dummy values for local validation.

## Known issues / workarounds
- During first-time setup, CMake can fail because the repo expects the Pico SDK and littlefs sources to exist under `.deps/`.
  - Workaround: run `./scripts/bootstrap-pico.sh` first to populate `.deps/pico-sdk`.
  - If configure later fails with `Cannot find source file .../.deps/littlefs/lfs.c`, clone littlefs into `.deps/littlefs` before retrying:
    - `mkdir -p .deps && git -C .deps clone --depth 1 https://github.com/littlefs-project/littlefs.git littlefs`
- The local environment may not have the ARM toolchain installed; install it before trying to build.
- The default local configuration targets `pico2_w`, while CI also validates `pico_w`.

## Notes for efficient agent work
- Start by inspecting the relevant source file and `CMakeLists.txt` before editing.
- Prefer small, surgical changes over broad rewrites.
- If a change affects networking, display rendering, or configuration persistence, verify the behavior through the build and note any limitations in the final summary.
