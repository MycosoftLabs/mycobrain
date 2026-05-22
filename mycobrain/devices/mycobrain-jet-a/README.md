# `mycobrain-jet-a` — Jetson + OpenClaw + Mushroom1

## Identity

| Field | Value |
|-------|-------|
| `device_id` | `mycobrain-jet-a-001` |
| Role | `mushroom1` |
| Host kind | `jetson_orin` |
| Host IP | `192.168.0.228` |
| Owner | morgan@mycosoft.org |
| OpenClaw | yes |

## Firmware

- Side A: `firmware/MycoBrain_SideA_MDP/` env `mushroom1`, fw `side-a-mdp-2.1.0`
- Side B: `firmware/MycoBrain_SideB_MDP/`, fw `side-b-mdp-2.0.0`
- Captive portal (PR #7) — pending merge
- OpenClaw firmware (PR #6) — pending merge

## SSH

`ssh jetson@192.168.0.228` — credentials in Claude memory (`mycobrain_jetson_ssh.md`), rotation-pending.

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
