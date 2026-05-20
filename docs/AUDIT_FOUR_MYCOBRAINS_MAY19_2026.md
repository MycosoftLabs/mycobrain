# MycoBrain Fleet Audit — Four Devices

**Date:** 2026-05-19
**Auditor:** Morgan (via Claude)
**Companion plan:** [`PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md`](PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md)

This document captures the as-built state of the four MycoBrain units in operation today and the action each needs to land on the unified agent.

---

## Quick table

| # | Nickname | Host | IP/Port | MycoBrain FW | OpenClaw | Current code | Talks to | Status | Priority action |
|---|----------|------|---------|--------------|----------|--------------|----------|--------|-----------------|
| 1 | **mycobrain-jet-a** | Jetson (Orin-class) | `192.168.0.228:8787` | Side A + Side B MDP v2.0.0 (assumed) | YES | Custom edge service exposing :8787 | MQTT broker | **LIVE** | Wrap existing service with `mycobrain-agent`; preserve :8787 |
| 2 | **mycobrain-pi-a** | Raspberry Pi (4 or 5) | `192.168.0.123:8787` | Side A + Side B MDP v2.0.0 (assumed) | YES | Custom edge service exposing :8787 (probably Python) | MQTT broker | **LIVE** | Same — re-host onto unified agent with `raspberry_pi` adapter |
| 3 | **mycobrain-jet-b** | Older Jetson (Nano 4GB or pre-Orin) | not assigned | board present, no firmware loaded | n/a | **none** | nothing | **DEAD** | Bootstrap: flash Side A + Side B, install agent with `jetson_legacy` adapter |
| 4 | **mycobrain-bench** | Windows PC (this machine, COM port) | local only | Legacy `MycoBrain_SideA/` (JSON-over-serial, no MDP, no Side B) | NO | site WebSocket consumer (handles legacy buttons) | `ws://mycosoft.com:8765` | **LIVE (legacy)** | Install agent with `standalone` adapter; legacy WS keeps working via bridge mode |

---

## Device 1 — mycobrain-jet-a (Jetson @ 192.168.0.228)

**Likely identity:** This matches the **Gateway Jetson 4GB** described in `JETSON_MYCOBRAIN_PRODUCTION_DEPLOY_MAR13_2026.md` — that doc explicitly bakes `GATEWAY_HOST=192.168.0.123` into the example env, which would place 192.168.0.228 as the **on-device operator** Jetson (Mushroom 1 or Hyphae 1 role).

**What's confirmed from docs:**
- Side A (sensor MCU) ↔ Side B (router MCU) ↔ Jetson UART rail
- OpenClaw is expected at `http://127.0.0.1:8000` on this host (per `OPENCLAW_BASE_URL` env)
- Docs reference port 8110 for the operator, but the device is on **8787**

**Likely truth:** the on-Jetson service was changed to listen on 8787 (perhaps to avoid conflict with another service on 8110, or because 8787 is the chosen "MycoBrain agent" port for this Mycosoft generation). The unified agent **adopts 8787 as canonical** going forward.

**Action items:**
- [ ] SSH to the Jetson and capture `systemctl status` of whatever exposes 8787 today
- [ ] Diff its API surface against [`PORT_8787_HTTP_API_SPEC_MAY19_2026.md`](PORT_8787_HTTP_API_SPEC_MAY19_2026.md)
- [ ] Install the unified agent alongside, swap over, retire the old service
- [ ] Confirm MQTT publish hits `mqtt.mycosoft.com` (currently visible at `mqtt-status.mycosoft.com`)

**Verification commands (run locally on Jetson once we're on it):**
```bash
# Identify the service binding 8787
sudo lsof -iTCP:8787 -sTCP:LISTEN
sudo systemctl status $(systemctl list-units --type=service --no-legend | grep -i mycobrain | awk '{print $1}')

# Tail what it's emitting
journalctl -u mycobrain-* -f --no-pager
```

---

## Device 2 — mycobrain-pi-a (Raspberry Pi @ 192.168.0.123)

**Identity:** The user-stated "Pi + OpenClaw" device. Note that `gateway-router.env.example` lists `GATEWAY_HOST=192.168.0.123` — so historically this IP was the *gateway Jetson*, but the user has confirmed it's a Raspberry Pi today. Either the Pi has taken over the gateway role, or the IP was reassigned.

**Assumptions to verify:**
- Same Side A + Side B firmware as the Jetson units (or Side A only via direct USB — the Pi may be in a single-MCU topology to save BOM)
- OpenClaw running locally at `127.0.0.1:8000`
- Some Python service on `:8787`

**Pi-specific gotchas to handle in the `raspberry_pi` adapter:**
- Serial port is `/dev/ttyAMA0` or `/dev/serial0` (UART), or `/dev/ttyUSB0` if using a USB-UART bridge
- Pi 4/5 have GPIO available — the adapter exposes optional GPIO control through `/command` with `target: "gpio"`
- ARM64 — Docker image must be linux/arm64
- No CUDA — TAC-O / NLM inference paths are stub-only on this device

**Action items:**
- [ ] SSH in, confirm which Side A/B variant is wired
- [ ] Install agent via `scripts/agent-install-pi.sh`
- [ ] Wire `raspberry_pi` adapter
- [ ] Confirm OpenClaw works through the proxy

---

## Device 3 — mycobrain-jet-b (older Jetson, dead)

**Identity:** User describes "older jetson and mycobrain has no code on it at all but plugged in and connected but not alive." The hardware ladder documented in `JETSON_MYCOBRAIN_HARDWARE_PLAN_MAR09_2026.md` includes Jetson Nano 4GB and Xavier NX as second-tier; this is most likely one of those.

**Cold-boot plan (`scripts/bootstrap-dead-jetson.sh` will encode this):**

1. **Identify the Jetson model**
   ```bash
   cat /etc/nv_tegra_release
   uname -r
   lscpu | grep Architecture
   ```
2. **Set up the OS + Python**
   ```bash
   sudo apt update && sudo apt install -y python3.11 python3.11-venv python3-pip git
   ```
3. **Flash Side A + Side B firmware** (from another machine with PlatformIO, since older Jetsons can be flaky for USB device-mode)
   ```powershell
   .\scripts\flash-mycobrain-production.ps1 -Board SideA -Role mushroom1 -Port COMx
   .\scripts\flash-mycobrain-production.ps1 -Board SideB -Port COMy
   ```
4. **Install the agent**
   ```bash
   bash scripts/agent-install-jetson.sh --adapter jetson_legacy
   ```
5. **Wire OpenClaw**
   - Install OpenClaw per its own runbook on 127.0.0.1:8000
   - The agent picks it up automatically once available
6. **Verify**
   ```bash
   curl http://localhost:8787/status
   curl http://localhost:8787/info
   ```
7. **Register with NatureOS** — visit `mycosoft.com/natureos/devices`, click "Pair new device," follow the JWT flow.

**Action items:**
- [ ] Power on, identify model
- [ ] Flash + install
- [ ] Pair

---

## Device 4 — mycobrain-bench (standalone over USB serial, this PC)

**Identity:** The MycoBrain currently plugged into the user's Windows PC over USB-CDC. User describes it as "4th mycobrain has old firmware we made device work with site buttons coms ect no jetson or openclaw yet."

**What "old firmware" means here:** This is the **legacy `firmware/MycoBrain_SideA/`** build (JSON-over-serial, not MDP). It pre-dates the MDP migration. It exchanges JSON lines over USB-CDC and the website knows how to talk to it directly via its WebSocket bridge.

**Why we don't reflash it (yet):**
- It works today.
- The legacy buttons in the website expect the JSON contract.
- Migration to MDP is a bigger ask — board has only one MCU here, no Side B; the JSON path is fine for bench-top use.

**Plan: dual-protocol standalone adapter**

The `standalone` adapter in the unified agent supports **both protocols**:

- If the device sends MDP frames (COBS + CRC16, magic byte): treat as MDP and forward through the normal path
- If it sends newline-delimited JSON: treat as legacy and translate to the unified telemetry envelope

This means:
- The legacy site buttons keep working — agent runs alongside, doesn't intercept the existing WebSocket
- NatureOS sees this device in the fleet list just like the others
- When/if we upgrade the firmware to MDP, no change needed on the agent side

**Action items:**
- [ ] Identify the COM port (likely COM3–COM10; user said "plugged into this pc over serial")
- [ ] Install agent via `scripts/agent-install-standalone.ps1 -Port COMx`
- [ ] Confirm telemetry appears in NatureOS
- [ ] Keep the legacy `ws://mycosoft.com:8765` path online

**Quick port enumeration on Windows:**
```powershell
Get-PnpDevice -PresentOnly | Where-Object { $_.Class -eq 'Ports' } | Format-Table -AutoSize
# or
[System.IO.Ports.SerialPort]::GetPortNames()
```

---

## Cross-cutting findings

### Finding 1 — Port mismatch (docs vs reality)

`JETSON_FIRMWARE_IMPLEMENTATION_GUIDE_MAR07_2026.md` documents:
- On-device operator → port **8110**
- Gateway router → port **8120**

`JETSON_MYCOBRAIN_PRODUCTION_DEPLOY_MAR13_2026.md` also references port **8080** in the verification checklist.

But the user's two live devices are both on **8787**.

**Resolution:** lock 8787 as canonical in the unified agent (matches deployed reality). Old ports become aliases via a tiny FastAPI middleware that returns 308 redirects from 8080/8110/8120 to 8787, so existing dashboards keep working.

### Finding 2 — Pi variant is undocumented

None of the docs mention Raspberry Pi as a supported host. The `JETSON_MYCOBRAIN_HARDWARE_PLAN_MAR09_2026.md` ladder is Jetson-only. This audit adds the Pi as a first-class tier and the unified agent's `raspberry_pi` adapter formalizes it.

### Finding 3 — No mycobrain-side Python agent today

The mycobrain repo only contains:
- `tools/python/mdp_decode.py` (debug)
- `tools/python/mdp_send_cmd.py` (debug)
- `mycobrain/tools/python/monitor_mycobrain.py` (debug)
- `deploy/jetson/taco_inference.py` (stub)
- `MQTT/deploy_mqtt_prod_guest.py` (deploy script)

The "real" service that the running Jetson and Pi devices use lives in **MAS repo** (`mycosoft_mas/edge/ondevice_operator.py`, `gateway_router.py`). The unified agent in this plan brings that code home to the mycobrain repo for the device-side runtime, leaving MAS for fleet orchestration above it. This keeps device firmware + device runtime in one repo and matches the user's instruction to "make sure you get all code related to the mycobrain+jetson combo."

### Finding 4 — OpenClaw control panel doesn't exist in NatureOS yet

The integration is wired *up to* the Device Manager but the **`/natureos/devices` UI does not yet expose claw control**. This is one of the deliverables called out in the plan's Phase 4.

### Finding 5 — MQTT broker is correctly externalized but agents may not be using it yet

`MQTT/CEO_DEPLOY_GUIDE.md` is dated 2026-04-07/08 and documents the Cloudflare-WSS path. We do not yet know whether the two live devices have been switched from whatever transport they used at deploy time to the new WSS broker. Verification step in Phase 2.

---

## What we already have, what we still need

**Already in repo and good:**
- ESP32-S3 Side A + Side B firmware (MDP v2.0.0)
- MDP codec spec + Python decoder
- MQTT broker deploy package
- Architecture docs (9 docs in `docs/`)
- Jetson deploy reference

**Filled in by this plan:**
- Master plan (this PR)
- Audit (this doc)
- HTTP API spec for port 8787
- OpenClaw integration guide
- NatureOS `/natureos/devices` spec
- MQTT topic schema
- Raspberry Pi adapter guide
- Standalone (PC over serial) guide
- Folder scaffold for `agents/` and `natureos/`
- Install scripts (Jetson / Pi / Windows)

**Still owed (Phase 2+):**
- Actual implementation of agent core, adapters, HTTP routes (skeletons land in this PR, real code comes next)
- The seven new `/api/devices/*` website endpoints
- NatureOS UI for `/natureos/devices`
- Firmware OTA flow for the agent itself
