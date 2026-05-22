# `mycobrain-bench` — Standalone over USB on Morgan's PC

## Identity

| Field | Value |
|-------|-------|
| `device_id` | `mycobrain-bench-001` (pending) |
| Role | `standalone` |
| Host kind | `standalone` (Windows PC) |
| Host IP | LAN-only (Morgan's workstation IP) |
| Port | `COM4` |
| Owner | morgan@mycosoft.org |
| OpenClaw | no |

## Firmware

- **Currently**: legacy `firmware/MycoBrain_SideA/` (JSON-over-serial, pre-MDP, single MCU)
- **Target**: upgrade to `firmware/MycoBrain_SideA_MDP/` when ready

The `standalone` adapter speaks BOTH protocols — legacy JSON AND MDP — so the upgrade is non-blocking.

## Bring-up

See [`deploy.md`](deploy.md). Single PowerShell command via `scripts/agent-install-standalone.ps1`.

## Legacy WebSocket coexistence

The legacy site buttons at `mycosoft.com` connect to `ws://mycosoft.com:8765/ws/<device_id>`. The unified agent does NOT intercept that connection — both paths run in parallel until the website is updated to use `:8787` (PR #9).

## NatureOS card

After pairing: `https://mycosoft.com/natureos/devices/mycobrain-bench-001`. OpenClaw panel is hidden (no claw on this device).
