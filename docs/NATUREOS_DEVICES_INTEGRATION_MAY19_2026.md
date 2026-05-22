# NatureOS `/natureos/devices` Integration

**Date:** 2026-05-19
**Status:** Spec for the website-side of the device fleet
**Companion:** [`PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md`](PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md), [`PORT_8787_HTTP_API_SPEC_MAY19_2026.md`](PORT_8787_HTTP_API_SPEC_MAY19_2026.md), [`OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md`](OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md)

This is the contract between `mycosoft.com/natureos/devices` (the website, owned in the natureos / mycosoft-site repo) and the **mycobrain-agent** running on each device. The website never talks to OpenClaw or Side A/B directly — it talks to the agent.

---

## What "works fully in all sections" means

The page `mycosoft.com/natureos/devices` has the following sections. Each must light up for **every device kind** (Jetson, Pi, standalone) when this lands.

| Section | What it shows | Backed by |
|---------|---------------|-----------|
| **Fleet list** | Every paired MycoBrain with a presence dot, host kind badge, OpenClaw badge, last seen | `GET /api/devices/network` |
| **Detail header** | Identity, host, agent + firmware versions, location | `GET /api/devices/{id}` |
| **Live telemetry** | BME688 charts, soil if present, latest reading | `GET /api/devices/{id}/telemetry` + MQTT push |
| **MDP frame tail** | Live rolling MDP frame window for debug | WS to agent `/ws/telemetry` (proxied) |
| **Command console** | Send any Side A or Side B command with audit history | `POST /api/devices/{id}/command` |
| **OpenClaw panel** | Joint state, quick actions, calibration, action history | `POST /api/devices/{id}/openclaw/action` + MQTT push |
| **Config editor** | Edit agent env, deploy, restart service | `GET/POST /api/devices/{id}/config` |
| **Pair new device** | Wizard: scan, claim, JWT issue | `POST /api/devices/{id}/pair` |
| **Audit log** | Every action/command, who did it, when, result | `GET /api/devices/{id}/audit` |

---

## Backend endpoints (website server-side)

These live in whatever runtime serves `mycosoft.com` (likely the existing Next.js / Node backend). Each one is a thin proxy from the website auth context to the agent's `:8787` surface plus the MAS device registry.

### `GET /api/devices/network`

Returns every paired device with its latest presence record. Backed by the MAS device registry — does NOT call every agent.

```json
{
  "devices": [
    {
      "device_id": "mycobrain-jet-a-001",
      "nickname": "mycobrain-jet-a",
      "host_kind": "jetson_orin",
      "host_ip": "192.168.0.228",
      "agent_url": "http://192.168.0.228:8787",
      "online": true,
      "last_seen": "2026-05-19T14:22:01Z",
      "openclaw_available": true,
      "side_a_fw": "side-a-mdp-2.0.0",
      "side_b_fw": "side-b-mdp-2.0.0",
      "location": "lab-bench-3"
    },
    ...
  ]
}
```

### `GET /api/devices/{id}`

Full record. Pulls `/info` + `/status` from the agent if online; falls back to last cached presence if offline.

### `POST /api/devices/{id}/command`

Body matches the agent's `POST /command`. Website validates the operator's NatureOS auth, mints a per-request JWT scoped to `device:command`, proxies, returns result.

### `POST /api/devices/{id}/openclaw/action`

Same pattern — JWT scoped to `openclaw:action`. The full sequence is in [`OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md`](OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md).

### `GET /api/devices/{id}/telemetry?from=...&to=...&resolution=...`

Time-series. Hits MINDEX FCI for history, optionally hits the agent's `/telemetry/latest` for the most recent point.

### `WS /api/devices/{id}/stream`

WebSocket the browser uses. Server-side this fan-outs:
- Live updates from MQTT subscriptions on `mycosoft/devices/{id}/...`
- Optional passthrough to the agent's `/ws/telemetry` for MDP frame tail

### `POST /api/devices/heartbeat`

Inbound from agents (every 30s). MAS already implements this; the unified agent honors the same shape.

```json
{
  "device_id": "...",
  "host_kind": "...",
  "agent_version": "...",
  "agent_url": "http://192.168.0.228:8787",
  "online": true,
  "side_a": {...},
  "side_b": {...},
  "openclaw": {...},
  "ts": "..."
}
```

### `POST /api/devices/{id}/pair`

Pair wizard. The website calls the agent's `POST /pair`, exchanges keys, mints the long-lived JWT, stores the device record. From this point on the website holds the credential and the agent will reject pair attempts until reset.

### `GET /api/devices/{id}/audit?limit=&phase=`

Audit log. Aggregated from agent's local JSONL into MINDEX. Filterable by phase (received / completed / failed) and action.

---

## UI flows

### Flow A — operator opens fleet page

1. GET `/api/devices/network`
2. Render grid (or list) with dots
3. Open SSE/WS subscription on `mycosoft/devices/+/presence` (server-side) → push presence changes to browser

### Flow B — operator opens device detail

1. GET `/api/devices/{id}` for header
2. WS `/api/devices/{id}/stream` for live telemetry + events + openclaw_state
3. GET `/api/devices/{id}/telemetry?from=now-1h` for the chart
4. GET `/api/devices/{id}/audit?limit=20` for the action timeline

### Flow C — operator sends Side A command (e.g. set LED red)

1. Operator clicks "LED → red" in command console
2. Browser → `POST /api/devices/{id}/command` `{target:"side_a",cmd:"output_control",params:{"id":"led1","value":"#ff0000"},ack_requested:true}`
3. Server mints JWT, proxies to agent `:8787/command`
4. Agent sends MDP COMMAND to Side A
5. Side A ACKs
6. Audit appears in the timeline within 1–2s (MQTT)

### Flow D — operator grasps with OpenClaw

Identical to Flow C but `POST /api/devices/{id}/openclaw/action`. Full step-by-step sequence in `OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md`.

### Flow E — operator pairs a brand-new device

1. Power on device, agent starts in `unpaired` mode (no JWT enforcement on `/pair` only)
2. Operator clicks "Pair new device" in NatureOS
3. NatureOS shows local network scan or asks for `agent_url`
4. Browser sends `POST /api/devices/<placeholder>/pair` → website calls `POST /pair` on the agent
5. Agent returns its `device_id`, public key, host info
6. Website creates the device record, signs JWT, stores
7. Operator confirms in UI; device shows up green in the fleet

---

## Auth

The website is the JWT issuer. The agent is the verifier.

- Per-operator session JWT — short-lived (`exp` 15 min), refreshable
- Per-request scoped JWT — minted for each action (`exp` 60s, scope strictly limited)
- Device-paired JWT — long-lived (`exp` 1 year), stored encrypted in the website DB

Public keys are exchanged at pairing time and rotated via a `POST /api/devices/{id}/rotate-key` flow (not in scope for v1 but the agent supports it).

Optional **mTLS** mode for unattended sites: the agent serves `:8787` with a per-device certificate signed by Mycosoft's internal CA; the website's outbound proxy uses a client cert. This is one knob in env (`AGENT_TLS_MODE=mtls`).

---

## Failure modes the UI must handle

| Condition | What the UI shows |
|-----------|-------------------|
| Device offline (no presence in 60s) | Grey dot, "Offline 3m ago", commands disabled with tooltip |
| Agent up, Side A link down | Yellow dot, "Side A link down — telemetry stale" |
| Agent up, OpenClaw unavailable | Hide OpenClaw panel entirely |
| Agent up, OpenClaw estop latched | Red banner with "Clear estop" button |
| Pair wizard timeout | Surface diagnostic: which step failed, agent log snippet |
| Command 504 (ACK timeout) | Inline error in console + audit entry `phase: failed` |
| MQTT broker unreachable from website | Use direct agent polling as fallback (1Hz `/status`) |

---

## What lives where (file map)

```
mycosoft-site (existing repo, presumably)
├── pages/natureos/devices/index.tsx           # Fleet list
├── pages/natureos/devices/[id].tsx            # Detail page
├── pages/api/devices/network.ts               # Backend
├── pages/api/devices/[id]/...                 # Backend
├── lib/agent-proxy.ts                         # JWT minting + proxying to :8787
└── components/devices/                        # Reusable UI parts
    ├── FleetGrid.tsx
    ├── DeviceHeader.tsx
    ├── TelemetryChart.tsx
    ├── MdpFrameTail.tsx
    ├── CommandConsole.tsx
    ├── OpenClawPanel.tsx
    └── PairWizard.tsx
```

The website-side scaffold is **not** included in this PR (it lives in a separate repo) but the contracts in this doc and the OpenAPI in `natureos/api-contract.openapi.yaml` are the canonical source the frontend team builds against.

---

## Verification path

When all of this is wired:

1. From browser, visit `mycosoft.com/natureos/devices` — see 4 devices (3 online, 1 offline if standalone PC is off)
2. Click `mycobrain-jet-a` — telemetry charts populate within 5s
3. Click "Grasp" in OpenClaw panel — claw moves, action appears in audit timeline within 2s, MQTT subscribers see the state change
4. Click "Pair new device," follow wizard with a freshly-flashed device — appears in fleet within 30s
5. Pull network on one device for 90s — UI flips to grey, agent reconnects, UI flips back

That's the acceptance test.
