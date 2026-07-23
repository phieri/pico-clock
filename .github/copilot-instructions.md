# Copilot instructions for pico-clock

## Repository overview
- This repository builds Raspberry Pi Pico W / Pico 2 W firmware for a network clock.
- The project uses C/C++, the Raspberry Pi Pico SDK, and CMake.
- Primary source files live under `src/`:
  - `src/main.c`: firmware entry point.
  - `src/runtime.c`: boot-time setup and runtime loop orchestration.
  - `src/network.c`: Wi-Fi connection, captive-portal probing, and NTP sync.
  - `src/display.c`: framebuffer rendering.
  - `src/config.c`: persistent configuration.
  - `src/clock.c`: time and drift logic.
- Keep changes localized to the relevant module unless a cross-cutting refactor is explicitly required.

## Build and validation workflow
- The build flow is driven by `CMakeLists.txt`, `.github/workflows/build.yml`, and `scripts/bootstrap-pico.sh`.
- Use the repository's documented flow for local validation:
  1. Install the ARM cross-toolchain and build tools:
     - `cmake`, `gcc-arm-none-eabi`, `libnewlib-arm-none-eabi`, and `build-essential`
  2. Bootstrap the SDK and local dependencies:
     - `./scripts/bootstrap-pico.sh`
  3. Configure a local build directory:
     - `cmake -S . -B build -DPICO_SDK_PATH=$PWD/.deps/pico-sdk -DPICO_BOARD=pico2_w`
     - To match CI, you can also configure for `pico_w` by changing the board value.
  4. Build the firmware:
     - `cmake --build build -j2`
- CI builds both `pico_w` and `pico2_w`; the default local configuration target is `pico2_w`.
- Expected outputs are written under `build/` as `.uf2`, `.elf`, `.bin`, and `.hex` artifacts.
- If you change build-related files or source modules, rerun the configure/build steps before concluding work.

## Repository-specific conventions
- Preserve the existing CMake target names and board selection unless the task explicitly calls for a broader platform change.
- If you touch runtime or boot-time behavior, prefer updating `src/runtime.c` and keep `src/main.c` focused on entry-point wiring.
- Avoid introducing new build systems, package managers, or dependencies unless the task requires them.
- If you update documentation, keep it aligned with the actual build commands used by the repo and the behavior described in `README.md`.
- Do not commit real Wi-Fi credentials or other secrets; use dummy values for local validation.
- Wi-Fi credentials, the hostname, and date-display behavior are configured over the serial console after flashing; do not assume compile-time Wi-Fi defaults exist.

## Known issues / workarounds
- During first-time setup, CMake can fail because the repo expects the Pico SDK and littlefs sources to exist under `.deps/`.
  - Workaround: run `./scripts/bootstrap-pico.sh` first.
  - If configure later fails with `Cannot find source file .../.deps/littlefs/lfs.c`, clone littlefs into `.deps/littlefs` before retrying:
    - `mkdir -p .deps && git -C .deps clone --depth 1 https://github.com/littlefs-project/littlefs.git littlefs`
- The local environment may not have the ARM toolchain installed; install it before trying to build.
- Validation errors encountered in this sandbox:
  - The initial package-manager attempt failed with `Could not open lock file /var/lib/apt/lists/lock - open (13: Permission denied)` because the shell was not running with elevated permissions; rerunning with `sudo` resolved it.
  - `apt-get update` also emitted a transient DNS error for `dl.google.com` while refreshing package indexes; that was unrelated to the firmware build and did not block the main Ubuntu package sources once the normal mirrors were used.

## Notes for efficient agent work
- Start by inspecting the relevant source file and `CMakeLists.txt` before editing.
- Prefer small, surgical changes over broad rewrites.
- If a change affects networking, display rendering, or configuration persistence, verify the behavior through the build and note any limitations in the final summary.
