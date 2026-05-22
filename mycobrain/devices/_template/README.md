# `<device-nickname>`

> **Skeleton.** Copy this folder to a new name (`mycobrain/devices/<nickname>/`) and fill in the placeholders below.

## Identity

| Field | Value |
|-------|-------|
| `device_id` | `mycobrain-<nickname>-001` |
| Role | `mushroom1` / `hyphae1` / `gateway` / `standalone` |
| Host kind | `jetson_orin` / `jetson_legacy` / `raspberry_pi` / `standalone` |
| Host IP | `192.168.0.XXX` |
| MAC | `XX:XX:XX:XX:XX:XX` |
| Owner | morgan@mycosoft.org |
| Paired at | YYYY-MM-DDTHH:MM:SSZ |
| OpenClaw | yes / no |

## Firmware

- Side A: `firmware/MycoBrain_SideA_MDP/` env `<role>` (e.g. `pio run -e mushroom1`)
- Side B: `firmware/MycoBrain_SideB_MDP/`
- Last flashed: YYYY-MM-DD, fw version `side-a-mdp-2.1.0` / `side-b-mdp-2.0.0`

## Bring-up

See [`deploy.md`](deploy.md).

## Calibration

Stored in [`calibration.json`](calibration.json). Apply on the device with:

```bash
curl -X POST http://<device-ip>:8787/command \
  -H "Authorization: Bearer $JWT" \
  -d @calibration.json
```

## Current state

See [`status.md`](status.md) — updated after each probe.

## NatureOS device card

`https://mycosoft.com/natureos/devices/<device_id>`
