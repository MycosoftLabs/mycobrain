> AWS topology correction 2026-11-27: local production servers, LAN addresses and VM/Proxmox/NAS deployment instructions are retired. Use [AWS_INFRA_REFERENCE.md](../../../AWS_INFRA_REFERENCE.md). Preserve historical evidence and optional device/external connectors.

# `mycobrain-pi-a` — Raspberry Pi + OpenClaw + Hyphae1

> **Network update (2026-11-27):** Device addresses below were on the garage LAN, which was decommissioned 2026-10-15 (Mycosoft has no local servers; MINDEX/MAS/website run on AWS). Confirm each device's current network address live before connecting; never assume the old 192.168.0.x address.


## Identity

| Field | Value |
|-------|-------|
| `device_id` | `mycobrain-pi-a-001` |
| Role | `hyphae1` |
| Host kind | `raspberry_pi` |
| Host IP | `<device IP — confirm live>` |
| Owner | morgan@mycosoft.org |
| OpenClaw | yes |

## Firmware

- Side A: `firmware/MycoBrain_SideA_MDP/` env `hyphae1` (dual BME688 + soil moisture ADC)
- Side B: typically single-MCU on Pi; Side B may be absent
- Captive portal (PR #7) — pending merge

## SSH

`ssh jetson@<device-ip>` (yes, the Pi was set up with the `jetson` user too). Credentials in Claude memory.

## Bring-up

See [`deploy.md`](deploy.md).

## NatureOS card

`https://mycosoft.com/natureos/devices/mycobrain-pi-a-001`
