# MycoBrain Unified Device Fleet — Master Plan

**Date:** 2026-05-19
**Owner:** Morgan (CEO) / RJ (COO) / Garret (CTO)
**Status:** Active build plan
**Supersedes guidance in:** `FIRMWARE_AND_JETSON_INDEX_MAR07_2026.md`, `JETSON_FIRMWARE_IMPLEMENTATION_GUIDE_MAR07_2026.md` (these remain valid for ESP32-S3 firmware; this doc adds the host/agent/integration layer)

---

## Why this plan exists

The mycobrain repo already contains production-grade **ESP32-S3 firmware** (Side A + Side B, MDP v1) and a documented **Jetson cortex/gateway** architecture. What's missing — and what this plan delivers — is the **unified host-agent layer** that:

1. Runs on every "compute companion" (Jetson Orin, older Jetson, Raspberry Pi, or just a Windows/macOS/Linux PC over USB serial).
2. Exposes a **single, identical HTTP API on port 8787** so `mycosoft.com/natureos/devices` can manage every MycoBrain the same way.
3. Wires **OpenClaw** control through the agent → Device Manager → NatureOS so the operator can authenticate at NatureOS and drive the claw.
4. Publishes telemetry to the production **MQTT broker** at `wss://mqtt.mycosoft.com` (LAN: `mqtt://<broker>:1883`).

Today we have **4 physical MycoBrain devices** in the field — different host hardware, different code states — and they need to look identical to MYCA / NatureOS / MINDEX.

---

## The four MycoBrains (audit summary)

Full audit in [`AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md`](AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md). Short version:

| # | Host | MycoBrain firmware | Listening | OpenClaw | State | Action |
|---|------|---------------------|-----------|----------|-------|--------|
| 1 | Jetson (Orin-class) | Side A + Side B MDP v2.0.0 | http://192.168.0.228:8787 | YES | **LIVE** | Re-skin onto the unified agent (port stays 8787) |
| 2 | Raspberry Pi | Side A + Side B MDP v2.0.0 | http://192.168.0.123:8787 | YES | **LIVE** | Re-skin onto the unified agent + Pi adapter |
| 3 | Older Jetson (Nano 4GB or pre-Orin) | board plugged, **no code** | — | — | **DEAD** | Cold-bootstrap with the unified agent |
| 4 | PC over USB serial | Legacy `MycoBrain_SideA` (JSON-over-serial, no MDP, no Jetson) | site WebSocket only | NO | **LIVE (legacy)** | Run the standalone adapter on this PC, keep legacy buttons working, upgrade firmware path later |

The unified agent is the same Python codebase for #1, #2, #3, and #4. The only difference is which **adapter** is selected at boot.

---

## Architecture overview

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        MycoBrain physical device                          │
│  ┌────────────────────────┐    UART/MDP    ┌────────────────────────┐    │
│  │ Side A (ESP32-S3)      │◄──────────────►│ Side B (ESP32-S3)      │    │
│  │ BME688 x2 / soil       │   COBS+CRC16   │ LoRa/WiFi/BLE/SIM      │    │
│  │ Sensor MCU             │                │ Router MCU             │    │
│  └────────────────────────┘                └───────────┬────────────┘    │
└───────────────────────────────────────────────────────│──────────────────┘
                                                        │ UART/MDP
┌───────────────────────────────────────────────────────▼──────────────────┐
│                  Compute companion (Jetson / Pi / PC)                     │
│  ┌──────────────────────────────────────────────────────────────────┐    │
│  │                       mycobrain-agent  (Python)                  │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │    │
│  │  │ core         │  │ adapters     │  │ openclaw             │   │    │
│  │  │ ───────────  │  │ ───────────  │  │ ──────────────────── │   │    │
│  │  │ mdp_codec    │  │ jetson_orin  │  │ client (127.0.0.1)   │   │    │
│  │  │ serial_bridge│  │ jetson_legacy│  │ task adapters        │   │    │
│  │  │ mqtt_client  │  │ raspberry_pi │  │ telemetry overlay    │   │    │
│  │  │ registry     │  │ standalone   │  │                      │   │    │
│  │  │ identity     │  │ (selected at │  │                      │   │    │
│  │  │ heartbeat    │  │  startup)    │  │                      │   │    │
│  │  └──────────────┘  └──────────────┘  └──────────────────────┘   │    │
│  │  ┌──────────────────────────────────────────────────────────┐   │    │
│  │  │           HTTP API on :8787 (FastAPI)                    │   │    │
│  │  │  GET  /status   GET  /info   POST /command               │   │    │
│  │  │  GET  /openclaw/status   POST /openclaw/action           │   │    │
│  │  │  WS   /ws/telemetry                                      │   │    │
│  │  └──────────────────────────────────────────────────────────┘   │    │
│  └──────────────────────────────────────────────────────────────────┘    │
└───────────────┬───────────────────────────────┬───────────────────────────┘
                │ MQTT pub/sub                  │ HTTPS (mTLS / JWT)
                ▼                               ▼
┌────────────────────────────────┐  ┌────────────────────────────────────┐
│  mqtt.mycosoft.com             │  │  mycosoft.com / natureos           │
│  (WSS, Cloudflare-fronted)     │  │  /natureos/devices                 │
│  /natureos/devices receives    │  │  Device Manager UI                 │
│  presence + telemetry          │  │  /api/devices/* backend            │
└────────────────────────────────┘  └────────────────────────────────────┘
                                                │
                                                ▼
                              MYCA · MINDEX · MAS · NLM · Fusarium
```

The **only** code that changes per device variant is the small adapter module that knows how to enumerate the local serial port, drive any host-specific hardware (e.g. GPIO on Pi), and select the right OpenClaw socket. Everything else is identical.

---

## Folder plan (additions to current repo)

```
mycobrain/
├── agents/                              # NEW — unified Python host agent
│   ├── pyproject.toml                   # Single installable package: mycobrain-agent
│   ├── README.md
│   ├── src/mycobrain_agent/
│   │   ├── __init__.py
│   │   ├── __main__.py                  # `python -m mycobrain_agent`
│   │   ├── config.py                    # Env-driven config
│   │   ├── identity.py                  # Canonical device_id (from Side A HELLO)
│   │   ├── core/
│   │   │   ├── mdp_codec.py             # Python port of common_mdp/mdp_codec.h
│   │   │   ├── serial_bridge.py         # Side A/B ↔ host UART
│   │   │   ├── mqtt_client.py           # Paho async, WSS or LAN
│   │   │   ├── registry.py              # Local device record + cache
│   │   │   ├── heartbeat.py             # POST /api/devices/heartbeat (MAS)
│   │   │   └── audit.py                 # JSONL audit log
│   │   ├── adapters/
│   │   │   ├── base.py                  # Adapter Protocol
│   │   │   ├── jetson_orin.py           # Orin Nano Super / AGX (8GB+ / 32GB)
│   │   │   ├── jetson_legacy.py         # Nano 4GB / pre-Orin / TX2
│   │   │   ├── raspberry_pi.py          # Pi 4 / Pi 5
│   │   │   └── standalone.py            # PC over USB-CDC (no second MCU)
│   │   ├── openclaw/
│   │   │   ├── client.py                # HTTP client → 127.0.0.1:8000
│   │   │   ├── auth.py                  # API-key forwarding from NatureOS
│   │   │   └── tasks.py                 # Task registry and dispatch
│   │   ├── http/
│   │   │   ├── server.py                # FastAPI app, port 8787
│   │   │   ├── auth_middleware.py       # NatureOS-issued JWT verification
│   │   │   └── routes/
│   │   │       ├── status.py            # /status, /info, /health
│   │   │       ├── command.py           # /command (Side A/B passthrough)
│   │   │       ├── openclaw.py          # /openclaw/*
│   │   │       ├── mdp.py               # /mdp/frames (live tail)
│   │   │       └── ws.py                # /ws/telemetry
│   │   └── upstream/
│   │       ├── mas.py                   # MAS heartbeat
│   │       ├── mindex.py                # MINDEX FCI telemetry
│   │       ├── nlm.py                   # NLM translate
│   │       └── mycorrhizae.py           # MMP envelopes
│   ├── tests/
│   │   ├── test_mdp_codec.py
│   │   ├── test_adapters.py
│   │   └── test_http_routes.py
│   └── deploy/
│       ├── systemd/
│       │   ├── mycobrain-agent.service  # Linux service (Jetson + Pi)
│       │   └── install.sh
│       ├── windows/
│       │   └── install-service.ps1      # NSSM-based Windows service (standalone PC)
│       ├── env/
│       │   ├── jetson_orin.env.example
│       │   ├── jetson_legacy.env.example
│       │   ├── raspberry_pi.env.example
│       │   └── standalone.env.example
│       └── docker/
│           ├── Dockerfile.arm64         # Jetson + Pi
│           └── Dockerfile.amd64         # standalone PC fallback
│
├── natureos/                            # NEW — contracts the website honors
│   ├── README.md
│   ├── api-contract.openapi.yaml        # OpenAPI spec for /api/devices/* and agent :8787
│   ├── device-schema.json               # Canonical Device record (used everywhere)
│   ├── auth.md                          # JWT issuance, device pairing, mTLS option
│   ├── ui-flows.md                      # /natureos/devices UI wireflows
│   └── openclaw-ui.md                   # OpenClaw control panel spec
│
├── docs/
│   ├── PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md      # THIS FILE
│   ├── AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md          # NEW — state of each device
│   ├── PORT_8787_HTTP_API_SPEC_MAY19_2026.md        # NEW — the agent API
│   ├── MQTT_TOPIC_SCHEMA_MAY19_2026.md              # NEW — topics every agent uses
│   ├── OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md     # NEW — how NatureOS drives the claw
│   ├── NATUREOS_DEVICES_INTEGRATION_MAY19_2026.md   # NEW — website-side spec
│   ├── RASPBERRY_PI_ADAPTER_GUIDE_MAY19_2026.md     # NEW — Pi-specific bring-up
│   ├── STANDALONE_SERIAL_AGENT_GUIDE_MAY19_2026.md  # NEW — PC-over-USB variant
│   └── (existing Mar 2026 docs remain authoritative for ESP32 firmware)
│
├── scripts/
│   ├── flash-mycobrain-production.ps1   # (already documented)
│   ├── agent-install-jetson.sh          # NEW — install agent on any Jetson
│   ├── agent-install-pi.sh              # NEW — install agent on Raspberry Pi
│   ├── agent-install-standalone.ps1     # NEW — install agent on Windows
│   └── bootstrap-dead-jetson.sh         # NEW — turn an empty Jetson into device #3
│
└── (existing firmware/, MQTT/, deploy/, tools/, mycobrain/, jetson_mycobrain/)
```

Nothing in the existing repo gets deleted. The new `agents/` package replaces what was conceptually in MAS's `mycosoft_mas/edge/` for the mycobrain-specific paths, while MAS keeps its higher-level orchestration. (Long-term we can either keep the agent here and let MAS import it, or move it under MAS — TBD with Garret.)

---

## The unified HTTP API on port 8787

Every MycoBrain compute companion answers the same calls. Full OpenAPI in [`PORT_8787_HTTP_API_SPEC_MAY19_2026.md`](PORT_8787_HTTP_API_SPEC_MAY19_2026.md). Headlines:

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/status` | Health + uptime + Side A/B link state |
| `GET` | `/info` | Identity (device_id, host_kind, fw versions, capabilities) |
| `GET` | `/telemetry/latest` | Most recent BME688 / soil / Side A reading |
| `POST` | `/command` | Send MDP command to Side A or Side B (JSON body) |
| `GET` | `/openclaw/status` | Claw state, calibration, current task |
| `POST` | `/openclaw/action` | Issue claw action (open/close/move/grasp/release/home) |
| `WS` | `/ws/telemetry` | Live telemetry + MDP event stream |
| `GET` | `/mdp/frames` | Live MDP frame tail (debug) |
| `GET` | `/healthz` | Liveness for systemd / k8s |
| `GET` | `/readyz` | Readiness (Side A HELLO received) |

All write endpoints (`POST` / WS commands) require a **JWT issued by NatureOS** (subject = MYCA user) or the on-device pairing token. Read endpoints from the LAN can be unauthenticated for backwards compatibility (configurable per env).

---

## OpenClaw integration

OpenClaw runs **on the Jetson/Pi itself** at `http://127.0.0.1:8000` (the same convention as the documented MAS edge). The agent does three things:

1. **Proxies** the NatureOS-authenticated `/openclaw/*` calls through to OpenClaw with the local API key — the user never sees the local key.
2. **Overlays** OpenClaw state into the agent's `/status` and MQTT presence message so MYCA always knows whether a claw is attached and ready.
3. **Records every action** to the JSONL audit log used by the proposal/approval/apply pattern already in place for Jetson on-device operator.

Full sequence in [`OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md`](OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md).

If a device has no OpenClaw (e.g. standalone PC variant), `/openclaw/status` returns `{"available": false}` and `POST /openclaw/action` returns 409.

---

## MQTT topic schema

Single broker (`wss://mqtt.mycosoft.com` public, `mqtt://<lan-broker>:1883` LAN). Topic root: `mycosoft/`. Full schema in [`MQTT_TOPIC_SCHEMA_MAY19_2026.md`](MQTT_TOPIC_SCHEMA_MAY19_2026.md).

```
mycosoft/devices/{device_id}/presence       # retained {online, host_kind, fw, ip}
mycosoft/devices/{device_id}/telemetry      # BME688, soil, side A events
mycosoft/devices/{device_id}/events         # estop, fault, link state
mycosoft/devices/{device_id}/openclaw/state # claw position, holding state
mycosoft/devices/{device_id}/cmd            # commands inbound (subscribed by agent)
mycosoft/devices/{device_id}/ack            # command acks (published by agent)
mycosoft/fleet/heartbeat                    # global heartbeat ticker
```

`mqtt-status.mycosoft.com` reads `mycosoft/devices/+/presence` to render the live fleet.

---

## NatureOS `/natureos/devices` integration

The website at `mycosoft.com/natureos/devices` already has the slot. This plan defines what backs it. Full doc: [`NATUREOS_DEVICES_INTEGRATION_MAY19_2026.md`](NATUREOS_DEVICES_INTEGRATION_MAY19_2026.md).

Backend endpoints the website needs:

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/api/devices/network` | List every MycoBrain with last presence |
| `GET` | `/api/devices/{id}` | Full device record |
| `POST` | `/api/devices/{id}/command` | Proxy to agent `:8787/command` |
| `POST` | `/api/devices/{id}/openclaw/action` | Proxy to agent `:8787/openclaw/action` |
| `POST` | `/api/devices/heartbeat` | Inbound from agents (already in MAS) |
| `GET` | `/api/devices/{id}/telemetry?from=&to=` | Historical pull from MINDEX |
| `POST` | `/api/devices/{id}/pair` | Pairing handshake (issues JWT) |

The website UI sections that must work (all of them) once this plan lands:

- **Fleet list** — every device, presence dot, last seen, host kind, OpenClaw badge
- **Detail page** — telemetry charts, MDP frame tail, OpenClaw panel, config editor
- **Command console** — send Side A / Side B / OpenClaw commands with audit history
- **Pairing flow** — claim a new device, install agent, register

---

## Phased rollout

### Phase 1 — Scaffold and lock contracts  *(this PR / this session)*

- [x] Master plan (this doc)
- [x] Audit doc of the 4 devices
- [x] Agent skeleton: `agents/src/mycobrain_agent/` with core + adapters + http stubs
- [x] HTTP API spec (`PORT_8787_HTTP_API_SPEC_MAY19_2026.md`)
- [x] OpenClaw integration spec
- [x] NatureOS integration spec
- [x] MQTT topic schema
- [x] Per-host install scripts (skeleton)
- [x] Updated top-level README pointer

### Phase 2 — Make the live two work on the unified agent  *(Garret + Morgan)*

- [ ] Run unified agent in dry-run on the Jetson at `.228` against existing Side A/B
- [ ] Confirm port 8787 stays compatible with the current `/status` shape (or add a compatibility shim)
- [ ] Repeat on the Pi at `.123`
- [ ] Cut over MQTT publish to `mqtt.mycosoft.com`

### Phase 3 — Bring up the dead Jetson and the standalone

- [ ] Older Jetson: run `agent-install-jetson.sh` with `ADAPTER=jetson_legacy`
- [ ] Standalone PC: run `agent-install-standalone.ps1` against the COM port that holds the legacy MycoBrain
- [ ] Confirm both register in `mycosoft.com/natureos/devices`

### Phase 4 — NatureOS UI

- [ ] Implement the seven `/api/devices/*` endpoints listed above
- [ ] Wire `/natureos/devices` list + detail + command panel + OpenClaw panel
- [ ] Verify pairing flow end-to-end with a fresh device

### Phase 5 — Hardening

- [ ] mTLS option for agent ↔ NatureOS (replace JWT for unattended sites)
- [ ] OTA path for the agent itself (the firmware OTA is already separate)
- [ ] Per-device feature flags surfaced in NatureOS
- [ ] Move audit logs into MINDEX

---

## Open questions to resolve with Garret + RJ

1. **Port 8787 origin** — none of the existing docs reference port 8787. We need to confirm whether the running services on `.228` and `.123` are an older internal build or whether someone changed the port from 8110/8120. The plan locks 8787 as canonical going forward to match what's deployed.
2. **OpenClaw API surface** — we assume `http://127.0.0.1:8000` with simple REST. Confirm the actual OpenClaw build's endpoint shape and lock the agent client to it.
3. **Agent home — mycobrain repo vs MAS repo** — currently the plan puts the unified agent here in `mycobrain/agents/` because it's the device-side code. If MAS already owns `ondevice_operator.py` and `gateway_router.py`, we should consolidate. Suggested resolution: this agent is the canonical device-side runtime; MAS imports it for the cortex/gateway profiles.
4. **Legacy MycoBrain (#4)** — keep the JSON-over-serial path running for backward compat with the existing site buttons, OR upgrade to MDP firmware now? Plan defaults to keep-and-bridge: the `standalone` adapter speaks both protocols.

---

## Related (existing) docs

- [Firmware README](../firmware/README.md) — Side A + Side B + MDP build/flash
- [MDP Protocol Contracts](MDP_PROTOCOL_CONTRACTS_MAR07_2026.md) — the wire format on the rail
- [Jetson Firmware Implementation Guide](JETSON_FIRMWARE_IMPLEMENTATION_GUIDE_MAR07_2026.md)
- [Jetson Production Deploy](JETSON_MYCOBRAIN_PRODUCTION_DEPLOY_MAR13_2026.md)
- [MQTT CEO Deploy Guide](../MQTT/CEO_DEPLOY_GUIDE.md)
- [Firmware + Jetson Index](FIRMWARE_AND_JETSON_INDEX_MAR07_2026.md)
