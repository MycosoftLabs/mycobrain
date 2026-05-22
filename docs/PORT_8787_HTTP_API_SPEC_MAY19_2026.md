# MycoBrain Agent HTTP API — Port 8787

**Date:** 2026-05-19
**Status:** Canonical contract — every MycoBrain agent honors this surface
**Companion plan:** [`PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md`](PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md)
**OpenAPI machine-readable spec:** [`../natureos/api-contract.openapi.yaml`](../natureos/api-contract.openapi.yaml)

The agent runs on every MycoBrain compute companion (Jetson, Pi, standalone PC). It listens on **port 8787** for HTTP and WebSocket. The website at `mycosoft.com/natureos/devices` calls these endpoints either directly (LAN) or through the MAS proxy.

---

## Conventions

- **Base URL:** `http://<host>:8787` (LAN). Public traffic goes through MAS, never directly.
- **Content type:** `application/json` for both directions unless noted.
- **Errors:** RFC 7807 problem+json shape: `{"type", "title", "status", "detail", "instance"}`.
- **Timestamps:** ISO-8601 UTC with `Z` suffix.
- **device_id:** Canonical identity, always Side A's identity (per MDP rules).

### Auth modes

Three modes, configured per agent via env:

| Mode | Header | Use case |
|------|--------|----------|
| `none` | — | LAN-only, dev mode |
| `pair_token` | `X-Pair-Token: <token>` | One-time-set local secret for unattended setups |
| `jwt` | `Authorization: Bearer <jwt>` | NatureOS-issued JWT, default for prod |

Read endpoints (`GET /status`, `GET /info`, `GET /telemetry/latest`, `GET /healthz`, `GET /readyz`) can be configured to skip auth (`AGENT_PUBLIC_READS=true`) for legacy dashboards.

All `POST` endpoints and the `/ws/telemetry` WebSocket always require auth.

---

## Endpoints

### `GET /status`

Returns liveness, link health, and a summary that drives the dashboard dot/badge.

```json
{
  "device_id": "mycobrain-jet-a-001",
  "host_kind": "jetson_orin",       // jetson_orin | jetson_legacy | raspberry_pi | standalone
  "agent_version": "1.0.0",
  "uptime_s": 138245,
  "side_a": { "linked": true, "last_hello_ms_ago": 2340, "fw_version": "side-a-mdp-2.0.0", "role": "mushroom1" },
  "side_b": { "linked": true, "last_heartbeat_ms_ago": 1200, "fw_version": "side-b-mdp-2.0.0" },
  "openclaw": { "available": true, "ready": true, "task": null },
  "mqtt": { "connected": true, "broker": "wss://mqtt.mycosoft.com" },
  "transports": { "lora": false, "ble": false, "wifi": true, "sim": false }
}
```

### `GET /info`

Static-ish identity and capability data, used for the device detail page header.

```json
{
  "device_id": "mycobrain-jet-a-001",
  "nickname": "mycobrain-jet-a",
  "host_kind": "jetson_orin",
  "host_model": "Jetson Orin Nano Super 8GB",
  "agent_version": "1.0.0",
  "side_a": {
    "fw_version": "side-a-mdp-2.0.0",
    "role": "mushroom1",
    "sensors": ["bme688_ambient", "bme688_environment"],
    "outputs": ["led1", "buzzer", "mosfet_1", "mosfet_2", "mosfet_3"]
  },
  "side_b": {
    "fw_version": "side-b-mdp-2.0.0",
    "transports": ["wifi"]
  },
  "openclaw": {
    "available": true,
    "endpoint": "http://127.0.0.1:8000",
    "model": "openclaw-v1"
  },
  "capabilities": [
    "mdp_command",
    "openclaw_control",
    "telemetry_stream",
    "frame_tail"
  ],
  "paired_to": "mycosoft.com",
  "paired_at": "2026-05-19T12:34:56Z"
}
```

### `GET /telemetry/latest`

Latest reading from Side A. Used for the "card" view on the fleet list.

```json
{
  "device_id": "mycobrain-jet-a-001",
  "captured_at": "2026-05-19T14:22:01Z",
  "sensors": {
    "bme688_ambient":   { "temp_c": 22.4, "humidity_pct": 51.2, "pressure_hpa": 1013.0, "iaq": 78, "co2_eq_ppm": 480 },
    "bme688_environment": { "temp_c": 23.1, "humidity_pct": 60.7, "pressure_hpa": 1012.7, "iaq": 92, "co2_eq_ppm": 510 },
    "soil_moisture":      { "value": 0.41, "raw": 1834 }
  }
}
```

If no telemetry has been received yet, returns 204 No Content.

### `POST /command`

Send an MDP command to Side A or Side B. The agent encodes, frames, sends, and (if `ack_requested`) waits for the ACK.

**Request:**
```json
{
  "target": "side_a",                      // side_a | side_b
  "cmd": "read_sensors",
  "params": { "sensors": ["bme1", "bme2"] },
  "ack_requested": true,
  "timeout_ms": 2000
}
```

**Response (target=side_a, cmd=read_sensors):**
```json
{
  "ok": true,
  "seq": 12834,
  "ack": { "received_at": "2026-05-19T14:22:05Z", "success": true },
  "telemetry": { "...": "as in /telemetry/latest" }
}
```

**Supported commands** mirror MDP §1.3 and §1.4:

| target | cmd | params |
|--------|-----|--------|
| `side_a` | `read_sensors` | `{ "sensors": ["bme1","bme2","soil"] }` |
| `side_a` | `stream_sensors` | `{ "rate_hz": 1, "sensors": ["bme1"] }` |
| `side_a` | `output_control` | `{ "id": "led1", "value": 1 }` |
| `side_a` | `enable_peripheral` / `disable_peripheral` | `{ "id": "bme688_1", "en": true }` |
| `side_a` | `estop` / `clear_estop` | `{}` |
| `side_a` | `health` | `{}` |
| `side_b` | `lora_send` | `{ "payload": "...", "qos": 1 }` |
| `side_b` | `ble_advertise` | `{ "en": true, "interval_ms": 100 }` |
| `side_b` | `wifi_connect` | `{ "ssid": "...", "pass": "..." }` |
| `side_b` | `sim_send` | `{ "dest": "...", "payload": "..." }` |
| `side_b` | `transport_status` | `{}` |

Errors:
- `400` — unknown `target` or `cmd`
- `409` — agent not ready (Side A not yet hello'd)
- `503` — link down to Side A or B
- `504` — ACK timeout

### `GET /openclaw/status`

Claw state. Available only if `host_kind` has OpenClaw and the OpenClaw process is reachable.

```json
{
  "available": true,
  "ready": true,
  "calibrated": true,
  "position": { "joint_1": 0.34, "joint_2": -0.12, "gripper": "closed" },
  "holding": false,
  "current_task": null,
  "last_action_at": "2026-05-19T14:18:30Z",
  "audit_tail_id": 8421
}
```

When `available: false`, all fields after `available` may be omitted.

### `POST /openclaw/action`

Issue a claw action. The agent proxies to OpenClaw at `127.0.0.1:8000` with the local API key, then records to the audit log.

**Request:**
```json
{
  "action": "grasp",
  "params": { "force_n": 5.0, "timeout_ms": 4000 },
  "request_id": "natureos-7d34c0e1",
  "user_subject": "morgan@mycosoft.com"
}
```

**Supported actions** (reconciled 2026-05-21 with the actual MDP claw commands in `firmware/common/mdp_claw.h` on the `claude/integrate-seeed-claw-SPwlV` branch):

| action | params | MDP command | MDP ID |
|--------|--------|-------------|--------|
| `grip` | `{}` | `claw_grip` | `0x0030` |
| `release` | `{}` | `claw_release` | `0x0031` |
| `position` | `{ "angle": 0–180 }` | `claw_position` | `0x0032` |
| `status` | `{}` | `claw_status` | `0x0033` |
| `calibrate` | `{}` | `claw_calibrate` | `0x0034` |
| `estop` | `{}` | `estop` | (Side A cross-cutting) |
| `clear_estop` | `{}` | `clear_estop` | (Side A cross-cutting) |

**Retired action names** (returns 405 with hint): `open` → `release`, `close` → `grip`, `home` → `position`, `move_to` → `position`, `grasp` → `grip`. The earlier high-level vocabulary (grasp / move_to / home) doesn't match the firmware — the Nemo claw is single-axis servo with a release angle and a grip angle, not a multi-joint arm. See [`OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md`](OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md) for the full reconciliation.

**Response:**
```json
{
  "ok": true,
  "request_id": "natureos-7d34c0e1",
  "started_at": "2026-05-19T14:22:30Z",
  "completed_at": "2026-05-19T14:22:32Z",
  "audit_id": 8422,
  "result": { "...": "OpenClaw-defined" }
}
```

Errors:
- `409` — `{"available": false}` host
- `423` — claw locked / estop active
- `502` — OpenClaw unreachable
- `504` — action timeout

### `WS /ws/telemetry`

WebSocket stream. After upgrade the client receives one JSON message per line.

Server pushes:

```json
{ "kind": "telemetry", "...": "as in /telemetry/latest" }
{ "kind": "event", "event": "estop", "device_id": "...", "ts": "..." }
{ "kind": "openclaw_state", "...": "as in /openclaw/status" }
{ "kind": "mdp_frame", "direction": "rx", "type": "TELEMETRY", "src": "0xA1", "dst": "0xC0", "seq": 1234, "payload": {...} }
```

Client may send (for live control):

```json
{ "kind": "subscribe", "topics": ["telemetry","event","openclaw_state"] }
{ "kind": "unsubscribe", "topics": ["mdp_frame"] }
{ "kind": "command", ... }   // same shape as POST /command
{ "kind": "openclaw_action", ... }  // same shape as POST /openclaw/action
```

Backpressure: server drops `mdp_frame` first, then telemetry batches; events and openclaw_state are never dropped.

### `GET /mdp/frames?since=<seq>&limit=200`

Live tail of MDP frames (rotating buffer in memory, default 2000 frames). For debug panels.

```json
[
  { "dir": "rx", "type": "TELEMETRY", "src": "0xA1", "dst": "0xC0", "seq": 1233, "payload": {...}, "ts": "..." },
  { "dir": "tx", "type": "COMMAND",   "src": "0xC0", "dst": "0xA1", "seq": 12834, "payload": {...}, "ts": "..." }
]
```

### `GET /healthz` / `GET /readyz`

- `/healthz` — 200 if the agent process is running. (Liveness.)
- `/readyz` — 200 if Side A HELLO has been received AND MQTT is connected (configurable). (Readiness.)

### `POST /pair`

One-shot pairing handshake invoked by `mycosoft.com/natureos/devices` during "Add device." Returns a JWT the website stores as the device credential. After pairing, the agent refuses `/pair` calls until reset.

**Request:**
```json
{
  "claimed_by": "morgan@mycosoft.com",
  "natureos_pubkey": "ed25519:...",
  "nonce": "..."
}
```

**Response:**
```json
{
  "device_id": "mycobrain-jet-a-001",
  "host_kind": "jetson_orin",
  "agent_pubkey": "ed25519:...",
  "signed_nonce": "...",
  "jwt": "eyJhbGciOi..."
}
```

After pairing the agent enforces `AGENT_AUTH_MODE=jwt` for all writes regardless of prior config.

---

## What changes vs the docs from March 2026

| Old (March docs) | New (this spec) | Why |
|------------------|-----------------|-----|
| On-device operator on **8110** | **8787** for everything | Match deployed reality on .228 and .123 |
| Gateway router on **8120** | **8787** (same agent, different env)