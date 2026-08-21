# Configuration

The clock exposes its configuration through the serial console. This keeps Wi‑Fi credentials and display preferences out of the firmware binary and lets you reconfigure the device in place.

## Wi‑Fi

```text
wifi <ssid> [<password>]
```

- Stores the SSID and optional password persistently.
- If a network is open, the firmware will probe common captive-portal URLs as part of the connection check.
- Password-protected networks are skipped for the portal probe.

## Hostname

```text
hostname <name>
```

Set the device hostname. If no name is provided, the firmware resets the value to the default `pico-clock` hostname.

## Date display mode

```text
date on|auto|off
```

- `on`: always show the date beneath the time.
- `auto`: only show the date around midnight.
- `off`: keep the traditional time-only display.

`showdate` is accepted as an alias for compatibility.

## Example session

```text
wifi OfficeWiFi mypassword
hostname kitchen-clock
date auto
```

After saving these settings, reboot the device or restart the networking flow to apply them.
