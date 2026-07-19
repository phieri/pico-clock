# Copilot instructions for pico-clock

## Repository overview
- This repository builds a Raspberry Pi Pico 2 W firmware image for a network clock.
- The project is C/C++ based and uses the Raspberry Pi Pico SDK via CMake.
- Primary source files live under `src/`:
  - `src/main.c` orchestrates the runtime loop and boot-time behavior.
  - `src/network.c` handles Wi-Fi/NTP/network probing.
  - `src/display.c` renders the framebuffer output.
  - `src/config.c` manages persistent configuration.
  - `src/clock.c` contains time/drift logic.
- Keep changes localized to the relevant module unless a cross-cutting refactor is explicitly required.

## Build and validation workflow
- The build is driven by `CMakeLists.txt` and the GitHub workflow in `.github/workflows/build.yml`.
- Use the repository’s documented flow for local validation:
  1. Install the ARM toolchain and CMake (the workflow uses `gcc-arm-none-eabi`, `libnewlib-arm-none-eabi`, and `build-essential`).
  2. Bootstrap the Pico SDK:
     - `./scripts/bootstrap-pico.sh`
  3. Configure a local build directory:
     - `cmake -S . -B build -DPICO_SDK_PATH=$PWD/.deps/pico-sdk -DPICO_BOARD=pico2_w -DWIFI_SSID=ci_build -DWIFI_PASSWORD=ci_build`
  4. Build the firmware:
     - `cmake --build build -j2`
- Expected outputs are generated under `build/` as `.uf2`, `.elf`, `.bin`, and `.hex` artifacts.
- If you change build-related files or source modules, rerun the configure/build steps before concluding work.

## Repository-specific conventions
- Preserve the existing CMake target names and board selection unless the task explicitly calls for a broader platform change.
- The default board is `pico2_w` and the Pico SDK platform default is `rp2350`.
- Avoid introducing new build systems, package managers, or dependencies unless the task requires them.
- If you update documentation, keep it aligned with the actual build commands used by the repo.
- Do not commit real Wi-Fi credentials or other secrets; use dummy values for local validation.

## Known issues / workarounds
- During first-time setup, `cmake` can fail because the repo expects the Pico SDK and littlefs sources to exist under `.deps/`.
  - Workaround: run `./scripts/bootstrap-pico.sh` first to populate `.deps/pico-sdk`.
  - If configure then fails with `Cannot find source file .../.deps/littlefs/lfs.c`, clone littlefs into `.deps/littlefs` before retrying the configure step, for example:
    - `mkdir -p .deps && git -C .deps clone --depth 1 https://github.com/littlefs-project/littlefs.git littlefs`
- The local environment may not have the ARM toolchain installed; install it before trying to build.
- The CI workflow builds for both `pico_w` and `pico2_w`, but the repo’s default local configuration targets `pico2_w`.

## Notes for efficient agent work
- Start by inspecting the relevant source file and `CMakeLists.txt` before editing.
- Prefer small, surgical changes over broad rewrites.
- If a change affects networking, display rendering, or configuration persistence, verify the behavior through the build and note any limitations in the final summary.
