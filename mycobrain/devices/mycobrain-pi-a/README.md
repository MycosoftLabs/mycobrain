# `mycobrain-pi-a` — Raspberry Pi + OpenClaw + Hyphae1

## Identity

| Field | Value |
|-------|-------|
| `device_id` | `mycobrain-pi-a-001` |
| Role | `hyphae1` |
| Host kind | `raspberry_pi` |
| Host IP | `192.168.0.123` |
| Owner | morgan@mycosoft.org |
| OpenClaw | yes |

## Firmware

- Side A: `firmware/MycoBrain_SideA_MDP/` env `hyphae1` (dual BME688 + soil moisture ADC)
- Side B: typically single-MCU on Pi; Side B may be absent
- Captive portal (PR #7) — pending merge

## SSH

`ssh jetson@192.168.0.123` (yes, the Pi was set up with the `jetson` user too). Credentials in Claude memory.

## Bring-up

See [`deploy.md`](deploy.md).

## NatureOS card

`https://mycosoft.com/natureos/devices/mycobrain-pi-a-001`
