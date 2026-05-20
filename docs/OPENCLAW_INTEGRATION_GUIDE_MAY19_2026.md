# OpenClaw Integration — NatureOS ↔ Agent ↔ OpenClaw

**Date:** 2026-05-19
**Status:** Canonical wiring for OpenClaw on every MycoBrain compute companion
**Companion:** [`PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md`](PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md), [`PORT_8787_HTTP_API_SPEC_MAY19_2026.md`](PORT_8787_HTTP_API_SPEC_MAY19_2026.md)

OpenClaw runs as a separate process on the same host as the agent (Jetson, Pi). It exposes its own local HTTP API at `http://127.0.0.1:8000`. The MycoBrain agent is the **only thing** that talks to OpenClaw — everything else goes through the agent. This keeps OpenClaw's API surface internal, lets us put NatureOS-issued JWT auth in front of every claw action, and gives us an audit trail per request.

---

## Sequence — claw action from NatureOS

```
Operator (browser at mycosoft.com/natureos/devices/<id>)
   │
   │  1. Click "Grasp" — JS sends POST /api/devices/<id>/openclaw/action
   ▼
NatureOS website (mycosoft.com)
   │
   │  2. Verify operator JWT, lookup device record, fetch agent URL
   │  3. Issue per-action JWT (scope=openclaw:action, sub=<operator>)
   │  4. POST <agent_url>/openclaw/action with Authorization: Bearer <jwt>
   ▼
mycobrain-agent on Jetson/Pi (port 8787)
   │
   │  5. Verify JWT, audit log "received", attach request_id
   │  6. POST http://127.0.0.1:8000/grasp with X-API-Key: <local_key>
   ▼
OpenClaw process on same host (port 8000)
   │
   │  7. Execute physical action, return result
   ▼
mycobrain-agent
   │
   │  8. Audit log "completed" with result
   │  9. Publish mycosoft/devices/<id>/openclaw/state to MQTT
   │  10. Return result to NatureOS
   ▼
NatureOS website
   │
   │  11. Update UI, push status to operator
```

Step 9 means **every other dashboard sees the claw move in real time** without polling the agent.

---

## Agent-side configuration

The agent reads from env (file: `/etc/mycobrain/agent.env` on Linux, system env vars on Windows):

```bash
# OpenClaw discovery
OPENCLAW_ENABLED=true
OPENCLAW_BASE_URL=http://127.0.0.1:8000
OPENCLAW_API_KEY=<local_secret_known_only_to_agent_and_openclaw>
OPENCLAW_TIMEOUT_MS=5000

# How the agent surfaces it
OPENCLAW_PROXY_PATH=/openclaw   # under :8787

# Audit
OPENCLAW_AUDIT_PATH=/var/log/mycobrain/openclaw_audit.jsonl
```

If `OPENCLAW_ENABLED=false` or `OPENCLAW_BASE_URL` is unreachable on startup, the agent advertises `openclaw.available: false` and rejects `/openclaw/*` writes with HTTP 409.

---

## OpenClaw API mapping

The agent translates its own canonical action vocabulary to OpenClaw's local API. Default mapping (overridable per host in `agents/src/mycobrain_agent/openclaw/tasks.py`):

| Agent action | OpenClaw call (assumed) | Notes |
|--------------|------------------------|-------|
| `open` | `POST /gripper/open` | |
| `close` | `POST /gripper/close` body `{"force_n":...}` | Force defaults to OpenClaw safe value |
| `home` | `POST /motion/home` | |
| `move_to` | `POST /motion/move` body `{joints...}` | Joint names match the canonical schema |
| `grasp` | `POST /tasks/grasp` body `{"force_n":...,"timeout_ms":...}` | High-level task; OpenClaw plans the motion |
| `release` | `POST /tasks/release` | |
| `calibrate` | `POST /maintenance/calibrate` body `{"mode":"quick"|"full"}` | |
| `estop` | `POST /safety/estop` | Hardware-latched; requires `clear_estop` |
| `clear_estop` | `POST /safety/clear_estop` | |

> **Open question for Garret:** confirm OpenClaw's real endpoint paths. If they differ, only `agents/src/mycobrain_agent/openclaw/client.py` changes — every other layer (NatureOS UI, agent API, MQTT) stays the same.

---

## Audit log shape

JSONL, one record per claw lifecycle event (`received` → `started` → `completed`/`failed`).

```json
{"id": 8422, "phase": "received", "ts": "2026-05-19T14:22:30Z", "device_id": "mycobrain-jet-a-001", "request_id": "natureos-7d34c0e1", "user_subject": "morgan@mycosoft.com", "action": "grasp", "params": {"force_n": 5.0, "timeout_ms": 4000}}
{"id": 8422, "phase": "started",  "ts": "2026-05-19T14:22:30Z"}
{"id": 8422, "phase": "completed","ts": "2026-05-19T14:22:32Z", "result": {...}}
```

The same record IDs are surfaced through `GET /status` (`audit_tail_id`) so the UI can deep-link into the audit panel.

---

## Safety policies enforced in the agent

1. **No claw action without auth** — even on LAN, claw writes always require JWT or pair token.
2. **Estop dominates** — if Side A or OpenClaw emits an estop event, all subsequent `/openclaw/action` requests return 423 Locked until `clear_estop` is called.
3. **Single-flight** — the agent serializes claw actions per device; concurrent requests get 429.
4. **Rate limit** — defaults: 5 actions per 10s burst, 60 actions per 5 min sustained. Configurable.
5. **Recording on by default** — every action is in the audit log. There is no "off the record" mode.

---

## NatureOS UI contract

The `/natureos/devices/<id>` page renders an OpenClaw panel iff `info.openclaw.available === true`. Panel contents:

- Live position readout (from `mycosoft/devices/<id>/openclaw/state` MQTT subscription)
- Quick-action buttons: Home / Open / Close / Grasp / Release / Estop
- Calibration drawer
- Joint sliders (when joint control mode is on)
- Recent actions list (calls `GET /api/devices/<id>/openclaw/audit?limit=50`)
- Estop toast banner (red) when latched

UI spec lives in [`../natureos/openclaw-ui.md`](../natureos/openclaw-ui.md).

---

## Standalone devices without OpenClaw

Device #4 (the legacy bench MycoBrain) has no claw. The agent reports `openclaw.available: false`. NatureOS hides the panel. No code path is exercised. Nothing to do.

---

## How to verify end-to-end

Once the agent is running on a Jetson/Pi with OpenClaw on the same host:

```bash
# 1. Confirm OpenClaw is up locally
curl -s http://127.0.0.1:8000/healthz

# 2. Confirm agent sees it
curl -s http://localhost:8787/openclaw/status | jq

# 3. Issue a benign action (home — never moves into anything)
curl -s -X POST http://localhost:8787/openclaw/action \
  -H "Authorization: Bearer $JWT" \
  -H "Content-Type: application/json" \
  -d '{"action":"home","params":{},"request_id":"manual-1","user_subject":"morgan@mycosoft.com"}' | jq

# 4. Tail audit
tail -f /var/log/mycobrain/openclaw_audit.jsonl

# 5. Confirm MQTT received the state update
mosquitto_sub -h <broker> -u mycobrain -P <pw> -t 'mycosoft/devices/+/openclaw/state' -v
```

A clean run touches all five layers in this list.
