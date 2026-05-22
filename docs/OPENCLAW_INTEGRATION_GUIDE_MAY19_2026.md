# OpenClaw Integration — Reconciled With Actual Implementation

**Date:** 2026-05-19 (rev. 2026-05-21)
**Status:** Reconciled with `claude/integrate-seeed-claw-SPwlV` branch on GitHub
**Companion:** [`PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md`](PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md), [`PORT_8787_HTTP_API_SPEC_MAY19_2026.md`](PORT_8787_HTTP_API_SPEC_MAY19_2026.md)

**REVISION (2026-05-21):** The original draft of this guide assumed OpenClaw was a REST service on `http://127.0.0.1:8000` with high-level actions like `grasp` / `move_to`. That was wrong. The actual implementation, on the open branch `claude/integrate-seeed-claw-SPwlV`, is:

- OpenClaw is a **Node.js daemon** (`openclaw` npm package) running on the Jetson as systemd unit `mycobrain-openclaw.service`
- It exposes a **WebSocket dashboard** at `ws://127.0.0.1:18789` (the `webchat` channel) — not a REST API
- It talks to **Side B over `/dev/ttyTHS1` UART** using a **newline-delimited JSON** protocol that Side B's firmware translates to MDP COMMAND frames for Side A
- Claw commands are **MDP IDs `0x0030`–`0x003F`** defined in `firmware/common/mdp_claw.h`
- Actions are **simple verbs**: `claw_grip`, `claw_release`, `claw_position`, `claw_status` — not high-level `grasp`/`release`

This document is now aligned with that reality. The agent's OpenClaw client (`agents/src/mycobrain_agent/openclaw/client.py`) has been rewritten to match.

---

## Sequence — claw action from NatureOS

```
Operator (browser at mycosoft.com/natureos/devices)
   │
   │  1. POST /api/devices/<id>/openclaw/action
   ▼
NatureOS website
   │
   │  2. Verify operator JWT, mint per-action JWT, proxy to agent
   ▼
mycobrain-agent on Jetson/Pi (port 8787)        ─ owns /dev/ttyTHS1 ─
   │
   │  3. Verify JWT, audit log, encode MDP COMMAND with id 0x0030+
   │  4. Send MDP frame to Side A via Side B (the agent's serial bridge)
   ▼
Side B ESP32-S3 (UART relay)
   │
   │  5. Forwards COMMAND frame to Side A over UART2
   ▼
Side A ESP32-S3 (Sensor MCU)
   │
   │  6. Servo control via PCA9685 (or LEDC PWM on GPIO 13)
   │  7. Reply with ACK + status (TELEMETRY frame on claw_status)
   ▼
mycobrain-agent
   │
   │  8. Publish openclaw/state to MQTT (retained)
   │  9. Return to NatureOS
```

The OpenClaw daemon (Node.js, port 18789) is **parallel**, not in series — its `mycobrain-control` skill calls the **same MDP claw commands** when invoked by voice/chat. The agent and the OpenClaw daemon are two front-doors into the same MDP rail. Coordination notes below.

---

## MDP Claw Command Family (from `firmware/common/mdp_claw.h`)

| MDP ID | Command | Params | Side A response |
|--------|---------|--------|-----------------|
| `0x0030` | `claw_grip` | — | ACK, then EVENT with new position |
| `0x0031` | `claw_release` | — | ACK |
| `0x0032` | `claw_position` | `{"angle": 0–180}` | ACK |
| `0x0033` | `claw_status` | — | TELEMETRY: `{position, is_closed, force_adc, mode, calibrated}` |
| `0x0034` | `claw_calibrate` | (TBD) | ACK |
| `0x0026` | `drone_latch_payload` | — | Alias for `claw_grip` (drone-context naming) |
| `0x0027` | `drone_release_payload` | — | Alias for `claw_release` |

These IDs sit in the `0x0030–0x003F` range reserved for claw control. They are sent as MDP COMMAND frames (msg_type `0x02`) targeted at `EP_SIDE_A` (`0xA1`). The agent serializes them like any other Side A command.

---

## Agent's HTTP API — action mapping

The agent's canonical action vocabulary maps to MDP commands. Earlier this doc claimed a richer API; reality is simpler.

| Agent action (`POST /openclaw/action`) | MDP command | Notes |
|---------------------------------------|-------------|-------|
| `grip` | `claw_grip` (0x0030) | renamed from `close` for clarity |
| `release` | `claw_release` (0x0031) | renamed from `open` |
| `position` | `claw_position` (0x0032) | params: `{angle}` |
| `status` | `claw_status` (0x0033) | returns position + force_adc |
| `calibrate` | `claw_calibrate` (0x0034) | when firmware finishes the calibration path |
| `estop` | (existing) `estop` Side A command | Bypasses claw layer; trips ALL outputs |
| `clear_estop` | `clear_estop` | |

Removed from the earlier draft (since they don't exist in the firmware):

- ~~`open`~~ — use `release`
- ~~`close`~~ — use `grip`
- ~~`home`~~ — not implemented; achieve via `position` with `angle: release_angle`
- ~~`move_to`~~ — not implemented; positions are single-axis only on Nemo claw
- ~~`grasp`~~ — would require force-feedback closed-loop; not in firmware yet

The agent will return `405 Method Not Allowed` for any retired action with a hint pointing to the new name. This keeps NatureOS's older UI alive during the transition.

---

## Agent-side configuration (revised)

```bash
# OpenClaw daemon discovery — for awareness only; the agent doesn't need to call it
OPENCLAW_DAEMON_WS=ws://127.0.0.1:18789
OPENCLAW_PROBE_ENABLED=true   # if true, agent occasionally pings the daemon to surface its presence

# Serial port ownership — agent is the canonical owner
MYCOBRAIN_SIDE_A_PORT=/dev/ttyTHS1
MYCOBRAIN_SIDE_B_PORT=/dev/ttyTHS2

# Audit
OPENCLAW_AUDIT_PATH=/var/log/mycobrain/openclaw_audit.jsonl
```

Removed (no longer applicable):
- `OPENCLAW_BASE_URL=http://127.0.0.1:8000` — there is no HTTP service at that URL
- `OPENCLAW_API_KEY` — OpenClaw daemon uses different auth (its own webchat tokens)

---

## Serial port arbitration

Three potential consumers of `/dev/ttyTHS1` exist on a fully-equipped Jetson:

1. `mycobrain-ondevice-operator.service` (Python, MAS repo) — original on-device operator
2. `mycobrain-openclaw.service` (Node.js, this branch) — OpenClaw daemon
3. `mycobrain-agent.service` (Python, `agents/` in this repo) — the new unified agent

**Only one can hold the port open at a time.** Recommended end-state:

> **mycobrain-agent owns /dev/ttyTHS1.** Both the OpenClaw daemon (via its skill HTTP calls) and the legacy ondevice operator (deprecated, retired) should call the agent's `POST /command` endpoint instead of opening serial directly.

Transition plan:

| Phase | What's running | Serial owner |
|-------|----------------|--------------|
| Today (pre-merge) | `mycobrain-ondevice-operator.service` | ondevice-operator |
| Phase 2 | Add `mycobrain-openclaw.service` from claude/integrate-seeed-claw branch | **conflict** — OpenClaw and ondevice-operator both try to hold the port; need to disable one |
| Phase 3 | Add `mycobrain-ag