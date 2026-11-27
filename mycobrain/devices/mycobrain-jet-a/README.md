> AWS topology correction 2026-11-27: local production servers, LAN addresses and VM/Proxmox/NAS deployment instructions are retired. Use [AWS_INFRA_REFERENCE.md](../../../AWS_INFRA_REFERENCE.md). Preserve historical evidence and optional device/external connectors.

# `mycobrain-jet-a` — Jetson + OpenClaw + Mushroom1

> **Network update (2026-11-27):** Device addresses below were on the garage LAN, which was decommissioned 2026-10-15 (Mycosoft has no local servers; MINDEX/MAS/website run on AWS). Confirm each device's current network address live before connecting; never assume the old 192.168.0.x address.


## Identity

| Field | Value |
|-------|-------|
| `device_id` | `mycobrain-jet-a-001` |
| Role | `mushroom1` |
| Host kind | `jetson_orin` |
| Host IP | `<device IP — confirm live>` |
| Owner | morgan@mycosoft.org |
| OpenClaw | yes |

## Firmware

- Side A: `firmware/MycoBrain_SideA_MDP/` env `mushroom1`, fw `side-a-mdp-2.1.0`
- Side B: `firmware/MycoBrain_SideB_MDP/`, fw `side-b-mdp-2.0.0`
- Captive portal (PR #7) — pending merge
- OpenClaw firmware (PR #6) — pending merge

## SSH

`ssh jetson@<device-ip>` — credentials in Claude memory (`mycobrain_jetson_ssh.md`), rotation-pending.

## Bring-up

See [`deploy.md`](deploy.md). Currently runs a custom service on `:8787` that pre-dates the unified agent — cutover to `mycobrain-agent.service` is the immediate next step.

## NatureOS card

`https://mycosoft.com/natureos/devices/mycobrain-jet-a-001`

After PR #9 lands, the card shows:
- Live telemetry (BME688 ambient + environment)
- OpenClaw control panel (grip/release/position/calibrate/estop)
- Command console (MDP target=side_a or side_b)
- MDP frame tail
- Audit log
