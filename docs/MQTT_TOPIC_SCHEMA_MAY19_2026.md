# MycoBrain MQTT Topic Schema

**Date:** 2026-05-19
**Broker:** `mqtt.mycosoft.com` (WSS via Cloudflare, public), `mqtt://<broker>:1883` LAN
**Auth:** Username `mycobrain` + password (set per `MQTT/CEO_DEPLOY_GUIDE.md`)
**Status page:** `https://mqtt-status.mycosoft.com/`

Every MycoBrain agent (Jetson, Pi, standalone) publishes to and subscribes from these topics. Anything that wants to know fleet state subscribes to `mycosoft/devices/+/...`. Anything that wants to *command* a device publishes to that device's `/cmd` topic — but the canonical control plane stays HTTP `:8787` for auditability; MQTT command in is only for headless edge-to-edge automation.

---

## Topic root

```
mycosoft/
├── devices/
│   └── {device_id}/
│       ├── presence       # retained — last-known state
│       ├── telemetry      # rolling — sensor batches
│       ├── events         # rolling — estop, fault, link state, openclaw lifecycle
│       ├── openclaw/
│       │   ├── state      # retained — current claw pose, holding, task
│       │   └── action     # rolling — completed action records (for live UI)
│       ├── mdp/frames     # opt-in, high volume — raw MDP frame stream for debug
│       ├── cmd            # inbound — commands the agent will execute
│       └── ack            # outbound — responses to cmd
└── fleet/
    ├── heartbeat          # global 30s tick: which device_ids reported in this window
    └── alerts             # rolling — fleet-level alerts
```

`{device_id}` follows MDP identity rules: it is **Side A's identity**, never the host or gateway. Host kind appears as a field inside `presence`.

---

## Payload shapes

### `mycosoft/devices/{id}/presence` *(retained, QoS 1)*

Published once on connect, again on any change, and via Last-Will when the agent dies.

```json
{
  "device_id": "mycobrain-jet-a-001",
  "nickname": "mycobrain-jet-a",
  "host_kind": "jetson_orin",
  "host_ip": "192.168.0.228",
  "agent_url": "http://192.168.0.228:8787",
  "agent_version": "1.0.0",
  "side_a_fw": "side-a-mdp-2.0.0",
  "side_b_fw": "side-b-mdp-2.0.0",
  "openclaw_available": true,
  "online": true,
  "last_seen": "2026-05-19T14:22:01Z"
}
```

Last-Will publishes `{"online": false, "last_seen": "<ts>", ...}` retained.

### `mycosoft/devices/{id}/telemetry` *(rolling, QoS 0)*

Same shape as `GET /telemetry/latest`. Typical rate 1 Hz when streaming, 1/min otherwise.

```json
{
  "device_id": "mycobrain-jet-a-001",
  "captured_at": "2026-05-19T14:22:01Z",
  "sensors": {
    "bme688_ambient":   { "temp_c": 22.4, "humidity_pct": 51.2, "pressure_hpa": 1013.0, "iaq": 78 },
    "bme688_environment": { "temp_c": 23.1, "humidity_pct": 60.7, "pressure_hpa": 1012.7, "iaq": 92 }
  }
}
```

### `mycosoft/devices/{id}/events` *(rolling, QoS 1)*

```json
{ "device_id": "...", "ts": "...", "kind": "estop",   "source": "side_a", "detail": {...} }
{ "device_id": "...", "ts": "...", "kind": "link_up", "source": "side_b", "detail": { "transport": "lora" } }
```

### `mycosoft/devices/{id}/openclaw/state` *(retained, QoS 1)*

Same shape as `GET /openclaw/status`.

### `mycosoft/devices/{id}/openclaw/action` *(rolling, QoS 1)*

```json
{ "device_id": "...", "ts": "...", "audit_id": 8422, "phase": "completed", "action": "grasp", "result": {...}, "user_subject": "morgan@mycosoft.com" }
```

### `mycosoft/devices/{id}/cmd` *(inbound, QoS 1)*

The agent subscribes. Payload mirrors `POST /command`:

```json
{ "target": "side_a", "cmd": "read_sensors", "params": {...}, "request_id": "edge-7", "ack_requested": true }
```

The agent rejects commands that aren't from a peer it has handshaken with (MMP envelope) — see `mycobrain/myco-iot-stack/spec/myco-envelope-v1.md`.

### `mycosoft/devices/{id}/ack` *(outbound, QoS 1)*

```json
{ "device_id": "...", "request_id": "edge-7", "ok": true, "ts": "...", "result": {...} }
```

### `mycosoft/fleet/heartbeat` *(rolling, QoS 0)*

Published by anything monitoring the fleet (the agent itself doesn't publish here — `mqtt-status.mycosoft.com` does).

```json
{ "ts": "2026-05-19T14:22:00Z", "online": ["mycobrain-jet-a-001","mycobrain-pi-a-001"], "count": 2 }
```

---

## Subscription patterns

| Subscriber | Subscribes to | Why |
|------------|---------------|-----|
| `mqtt-status.mycosoft.com` | `mycosoft/devices/+/presence` | Render live fleet dots |
| `mycosoft.com/natureos/devices` (server-side) | `mycosoft/devices/+/{presence,telemetry,events,openclaw/state}` | Push updates to browser via SSE/WS |
| MAS heartbeat aggregator | `mycosoft/devices/+/presence` | Mirror into MAS registry |
| MINDEX FCI | `mycosoft/devices/+/telemetry` | Long-term timeseries store |
| Agent on each device | `mycosoft/devices/{me}/cmd` | Inbound commands |

---

## QoS, retention, and last-will conventions

| Topic | QoS | Retained | Last-Will |
|-------|-----|----------|-----------|
| presence | 1 | YES | YES (sets `online: false`) |
| telemetry | 0 | no | no |
| events | 1 | no | no |
| openclaw/state | 1 | YES | YES (sets `available: false`) |
| openclaw/action | 1 | no | no |
| cmd | 1 | no | no |
| ack | 1 | no | no |
| fleet/heartbeat | 0 | no | no |

---

## ACLs (mosquitto)

The single shared `mycobrain` user is fine for v1. For multi-tenant futures the mosquitto config supports per-topic ACL; placeholder in `MQTT/mycobrain-mqtt-prod/config/`. Until then:

- All agents auth as `mycobrain`
- All subscribers auth as `mycobrain`
- Public Internet path: WSS only, via Cloudflare tunnel
- LAN path: 1883 inside trusted network

---

## Migration note

Until every agent is moved to this schema, leave the legacy `mycobrain/#` topic root publishing as well — the broker doesn't care, MQTT-status will be updated to consume from both during the migration window.
