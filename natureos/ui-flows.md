# UI Flows — `/natureos/devices`

Six core flows. Each section names the screen, the data it pulls, and the network calls.

---

## 1. Fleet list (`/natureos/devices`)

```
┌── Header: "Devices" · [+ Pair new device]
│
│  [Filter ▾]  [Search]
│
│  ┌────────────────────────────────────────────────────────────────┐
│  │ ● mycobrain-jet-a    jetson_orin   192.168.0.228  OpenClaw ✓  │
│  │   side-a-mdp-2.0.0   /  side-b-mdp-2.0.0          last 5s     │
│  ├────────────────────────────────────────────────────────────────┤
│  │ ● mycobrain-pi-a     raspberry_pi  192.168.0.123  OpenClaw ✓  │
│  ├────────────────────────────────────────────────────────────────┤
│  │ ○ mycobrain-jet-b    jetson_legacy (offline)                  │
│  ├────────────────────────────────────────────────────────────────┤
│  │ ● mycobrain-bench    standalone    (USB)          legacy mode │
│  └────────────────────────────────────────────────────────────────┘
```

- Initial: `GET /api/devices/network`
- Live: server-side MQTT subscribe to `mycosoft/devices/+/presence`, fan-out to browser via SSE

## 2. Device detail (`/natureos/devices/{id}`)

Header strip: nickname · host model · agent version · pair date · "Online" badge.

Tabs:

- **Live** — last reading card + last 10 events + claw panel (if available)
- **Telemetry** — charts for temp/humidity/IAQ/CO2/soil (range picker)
- **Console** — Side A + Side B commands with history
- **MDP frames** — live frame tail (subscribes to `WS /api/devices/{id}/stream` with `kind=mdp_frame`)
- **Config** — agent env editor + restart button (requires admin scope)
- **Audit** — every action, who/when/result

Calls:
- `GET /api/devices/{id}` (header)
- `GET /api/devices/{id}/telemetry?from=now-1h` (Live + Telemetry)
- `WS /api/devices/{id}/stream` (everything live)
- `GET /api/devices/{id}/audit?limit=50` (Audit tab)

## 3. Command console flow

1. User selects target (Side A / Side B), cmd, fills params
2. Browser → `POST /api/devices/{id}/command`
3. Spinner; result inline within 2s (ACK or timeout)
4. Result appended to console history; audit log entry appears in Audit tab on next subscribe push

## 4. OpenClaw flow

See `openclaw-ui.md`.

## 5. Pair-new-device wizard

1. User clicks "+ Pair new device" on the fleet list
2. Modal: "Reachable IP and port" — defaults to a scan if backend has LAN-scan endpoint
3. Browser → `POST /api/devices/new/pair { agent_url, claimed_by }`
4. Server → agent `:8787/pair` with claim payload
5. Server gets back device_id, host info; mints long-lived JWT, stores device record
6. UI shows success + device card appears in fleet list

## 6. Offline / error states

| State | UI |
|-------|----|
| Device offline >60s | Grey dot, "Offline 3m ago", commands disabled with tooltip |
| Side A link down | Yellow dot, banner "Side A link down — telemetry stale" |
| Agent unreachable but MQTT presence current | Same dot, banner "Agent HTTP unreachable; presence via MQTT" |
| MQTT broker unreachable from website | Switch to 1Hz `/status` polling per device, banner at top |
| 401 from agent | "Your session expired" + reauth |
| 423 (estop latched) on claw action | Inline modal: "Estop is latched on the device. Clear it first." |

---

## Empty states

- No devices yet → big "Pair your first MycoBrain" CTA, link to install scripts
- No telemetry yet → empty chart with "Waiting for first Side A reading…"
- No claw → panel hidden, not greyed
