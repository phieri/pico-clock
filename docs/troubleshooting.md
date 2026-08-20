# Troubleshooting

## Build problems

### Missing Pico SDK or LittleFS sources

If configuration fails because the Pico SDK or `lfs.c` is absent, prepare the project dependencies first:

```bash
./scripts/bootstrap-pico.sh
```

If the build still complains about missing `lfs.c`, clone the littlefs dependency manually:

```bash
mkdir -p .deps
git -C .deps clone --depth 1 https://github.com/littlefs-project/littlefs.git littlefs
```

## Device does not connect to Wi‑Fi

- Confirm the `wifi` command was stored correctly.
- Check the serial console for the active SSID and the network state.
- Ensure the network is open or uses a supported password flow.
- Reboot the Pico after changing the configuration.

## Time is wrong

- Verify that NTP is reachable over the network.
- Check that the device has a valid gateway and DNS path.
- Look at the serial logs for drift and latency values; they are reported as part of the runtime loop.

## Display shows the wrong behaviour

- Use `date on|auto|off` to toggle the date visibility.
- Check that the hostname is set correctly.
- Confirm the framebuffer is updating after the board boots.

## Useful commands

```text
wifi <ssid> [<password>]
hostname <name>
date on|auto|off
```

If the issue persists, use the serial output during startup to track the networking and sync stages.
