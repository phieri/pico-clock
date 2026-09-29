# Quick start

## 1. Prepare the build environment

Install the toolchain and dependencies used by the repository:

```bash
sudo apt-get update
sudo apt-get install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
```

Then bootstrap the Pico SDK and local dependencies:

```bash
./scripts/bootstrap-pico.sh
```

!!! note
    The project expects the Pico SDK under `.deps/pico-sdk`, littlefs under `.deps/littlefs`, and PicoDVI under `.deps/PicoDVI`.

## 2. Configure the firmware build

Use the default board target for a Pico 2 W build:

```bash
cmake -S . -B build -DPICO_SDK_PATH=$PWD/.deps/pico-sdk -DPICO_BOARD=pico2_w
```

Only Pico 2 W is supported for the Waveshare PICO-DVI-7inch.

## 3. Build the firmware

```bash
cmake --build build -j2
```

The generated build outputs are written to `build/` as `.uf2`, `.elf`, `.bin`, and `.hex` artifacts.

## 4. Flash and test the device

1. Put the Pico into USB bootloader mode.
2. Copy the generated `.uf2` image to the mounted drive.
3. Open the serial console at the configured baud rate.
4. Seat the Pico 2 W in the Waveshare PICO-DVI-7inch carrier and power the display according to its instructions. Connect a USB serial terminal within 30 seconds of boot to configure Wi‑Fi and the hostname. The display shows the clock after synchronization; USB serial also reports the time.

## 5. Configure the device over serial

After the first boot, configure the device with serial commands:

```text
wifi <ssid> [<password>]
hostname <name>
date on|auto|off
```

Example:

```text
wifi GuestNetwork secretpass
hostname desk-clock
date auto
```

The `wifi` command stores the credentials persistently. The `hostname` and `date` commands control the device identity and date display behaviour.
