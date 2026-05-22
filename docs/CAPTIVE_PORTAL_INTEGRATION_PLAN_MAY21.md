# Captive Portal + MDP Unified Firmware Integration Plan

**Date:** 2026-05-21
**Status:** Plan — execution starting in PR #7
**Source branch:** `feature/esp32-captive-portal-web-ui` (in `mycobrain`, 1 commit ahead of main, 31 behind)
**Target:** `firmware/MycoBrain_SideA_MDP/` (the canonical production firmware on main)
**Author:** Morgan (via Claude)

## The discovery

Berto built a complete "device manager" stack on the ESP32 itself in the `feature/esp32-captive-portal-web-ui` branch — captive portal, WiFi STA+AP, HTTP REST API, WebSocket live telemetry, persistent config, web UI served from LittleFS. It landed in `firmware/side_a/` (the legacy build), not `firmware/MycoBrain_SideA_MDP/` (the production MDP build), so the production firmware has none of it. Meanwhile mycosoft.com/natureos/devices is built to talk to that exact protocol over WiFi. Today the production MDP firmware can ONLY be reached through the Jetson + MDP rail; if there's no Jetson, the device is invisible to the site.

## Architecture target

```
                                                                ┌─ NatureOS /natureos/devices
                                                                │   (device manager UI)
                                                                │
                                                                │  HTTP REST + WebSocket
                                                                ▼
┌──────────────────────────────────────────────────────────────────────┐
│  MycoBrain Side A ESP32-S3 (firmware/MycoBrain_SideA_MDP/)            │
│                                                                       │
│  ┌──────────────────────────┐    ┌──────────────────────────┐        │
│  │   MDP rail (existing)    │    │   Captive portal (NEW)   │        │
│  │   --------------------   │    │   --------------------   │        │
│  │   COBS+CRC16 framing     │    │   AsyncWebServer :80     │        │
│  │   over UART2 to Side B   │    │   AsyncWebSocket /ws     │        │
│  │   Telemetry frames @ 1Hz │    │   DNS captive redirect   │        │
│  │   Command handlers       │    │   WiFi mgr (AP+STA)      │        │
│  │   HELLO + role mgr       │    │   Config + calibration   │        │
│  └────────────┬─────────────┘    └─────────────┬────────────┘        │
│               │                                  │                     │
│               │      Shared sensor state         │                     │
│               └──────────┬───────────────────────┘                     │
│                          │                                              │
│                  ┌───────▼─────────┐                                   │
│                  │  Sensor sampler │                                   │
│                  │  (BME688 x2,    │                                   │
│                  │   soil, AI[4])  │                                   │
│                  └─────────────────┘                                   │
└──────────────────────────────────────────────────────────────────────┘
                          ▲
                          │ UART (MDP)
                          │
                  Side B ─→ Jetson ─→ NatureOS via mycobrain-agent on :8787
```

The two telemetry paths are **complementary, not exclusive**:

| Deployment | MDP path | WiFi path | Telemetry route |
|------------|----------|-----------|-----------------|
| Field box with Jetson (Mushroom 1, Hyphae 1) | active | optional | MDP → Jetson agent → MQTT/NatureOS |
| Standalone device on a network | inactive | active | WiFi → HTTP/WS → NatureOS device manager directly |
| Hybrid (Jetson present, WiFi also on) | active | active | Both — NatureOS dedupes by `device_id` |
| Initial bring-up (no WiFi creds, no Jetson yet) | inactive | active (AP mode) | Captive portal on `mycobrain-XXXX.local`; operator's phone configures STA credentials |

## What gets ported, and where

Source files copied/adapted from `firmware/side_a/` (feature branch) into `firmware/MycoBrain_SideA_MDP/`:

```
src/portal/
├── portal_manager.{h,cpp}        ← orchestrator: begin/loop/stop
├── wifi_manager.{h,cpp}          ← AP + STA, reconnect logic
├── http_server.{h,cpp}           ← AsyncWebServer routes
├── websocket_server.{h,cpp}      ← AsyncWebSocket
├── telemetry_broadcast.{h,cpp}   ← rate-limited WS push
└── dns_server.{h,cpp}            ← captive portal DNS hijack
src/config/
├── config_schema.h               ← CalibrationConfig, PinConfig, ThresholdConfig, WiFiConfig
├── config_manager.{h,cpp}        ← NVS-backed persistence
└── calibration.{h,cpp}           ← per-channel offset/gain application
src/telemetry/
└── telemetry_json.{h,cpp}        ← TelemetryV1 struct → JsonObject
data/
├── index.html                    ← single-page UI
├── app.js                        ← live telemetry + config forms
└── styles.css
```

**Adapter glue** added new to bridge the two stacks:

```
src/integration/
├── mdp_to_portal_bridge.{h,cpp}  ← every MDP TELEMETRY frame the firmware sends out
│                                    is also pushed into TelemetryBroadcast → WS clients
├── portal_to_mdp_bridge.{h,cpp}  ← HTTP POST /api/command translates to MDP commands
│                                    so a NatureOS-via-WiFi action lands the same way
│                                    a NatureOS-via-Jetson action does
└── shared_state.{h,cpp}           ← single in-RAM struct that both paths read/write
```

The `MycoBrain_SideA_MDP/src/main.cpp` keeps its current MDP loop and adds, at the top of `setup()`:

```cpp
// existing MDP setup ...
ConfigManager::begin();      // load persistent config from NVS
WiFiManager::begin(ConfigManager::wifi());
PortalManager::begin();      // HTTP + WS + DNS only start once WiFi has an IP (AP or STA)
```

In `loop()`:

```cpp
PortalManager::loop();       // non-blocking; tens of microseconds when idle
WiFiManager::loop();
// existing MDP loop continues
```

When the MDP firmware emits a `TELEMETRY` frame on the UART, it also calls `MdpToPortalBridge::publish(telemetry)` which calls `TelemetryBroadcast::broadcastTelemetry(...)` — same data, two pipes.

## platformio.ini changes for the MDP build

Current `firmware/MycoBrain_SideA_MDP/platformio.ini` lib_deps:

```ini
lib_deps =
    boschsensortec/BSEC2 Software Library@^1.4.8120
    adafruit/Adafruit BME680 Library@^2.0.5
    adafruit/Adafruit Unified Sensor@^1.1.14
    fastled/FastLED@^3.6.0
    bblanchon/ArduinoJson@^7.0.4
```

Add for the portal:

```ini
lib_deps =
    ...existing...
    ESP32Async/AsyncTCP@^3.2.0
    ESP32Async/ESPAsyncWebServer@^3.0.0
    # LittleFS is in arduino-esp32 core; no extra dep
```

Also enable LittleFS partition + WiFi:

```ini
board_build.partitions = default_16MB.csv  # already there
board_build.filesystem = littlefs           # add
build_flags =
    -DCORE_DEBUG_LEVEL=0
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCAPTIVE_PORTAL=1                     # add — toggles portal compile-in
```

Existing build_flags from the role envs (mushroom1, hyphae1) inherit this base.

## HTTP API contract (from the captive portal code)

This is what NatureOS device manager talks to directly. Aligns with the agent's :8787 spec **at the route names**, just one layer down (per-device direct).

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/` | Captive portal landing (serves `index.html` from LittleFS) |
| `GET` | `/api/telemetry` | Latest reading as JSON |
| `GET` | `/api/sensors` | Sensor inventory + I2C scan results |
| `GET` | `/api/wifi/status` | AP IP, STA IP, RSSI, connected clients |
| `POST` | `/api/wifi/config` | Set STA credentials, switch mode |
| `POST` | `/api/calibration` | Per-channel offsets/gains |
| `POST` | `/api/pins` | Runtime pin remap (analog/MOSFET) |
| `POST` | `/api/thresholds` | Sensor high/low thresholds |
| `WS` | `/ws` | Live telemetry @ ≤10 Hz |

After integration, the **`POST /api/command`** route is added to mirror the agent's `:8787/command` shape — so the website can issue Side-B-style transport commands too if it ever wants to (LoRa send, BLE advertise, etc.).

## WebSocket protocol

Server → client messages (server pushes, no client request needed):

```json
{ "type": "telemetry", "ts": "...", "device_id": "mycobrain-...", "sensors": {...}, "ai_volts": [..], "mos": [..] }
{ "type": "event", "kind": "wifi_connected", "ip": "192.168.1.42" }
{ "type": "config_changed", "section": "wifi", "by": "http://1.2.3.4" }
```

Client → server messages (optional, for live control):

```json
{ "type": "subscribe", "topics": ["telemetry","event"] }
{ "type": "command", "target": "side_a", "cmd": "output_control", "params": {...} }
```

## How NatureOS routes telemetry across both paths

The site's device manager already discovers MycoBrains via:
1. MQTT presence (from the unified agent, `mycosoft/devices/+/presence`) — works when Jetson is present
2. **NEW: direct HTTP scan** — when a device is reachable on the LAN but no agent has registered it, the site polls `http://<ip>/api/wifi/status` and `http://<ip>/api/telemetry`, then synthesizes the same Device record (`device-schema.json`) as if the agent had reported it

Dedupe rule: if both an agent presence AND a direct-WiFi handshake see the same `device_id` (from Side A's HELLO), they're collapsed into one device card. The card shows two "transport" badges: `MDP via Jetson` and `WiFi direct`.

## Step-by-step execution

1. Create branch `feat/captive-portal-mdp-merge` off current main
2. Copy all `src/portal/`, `src/config/`, `src/telemetry/` files from `firmware/side_a/` into `firmware/MycoBrain_SideA_MDP/`
3. Copy `data/` LittleFS assets
4. Update `platformio.ini` (lib_deps, filesystem, build_flags)
5. Modify `MycoBrain_SideA_MDP/src/main.cpp`:
   - Add includes
   - Wire `ConfigManager::begin()` + `WiFiManager::begin()` + `PortalManager::begin()` in `setup()`
   - Add `PortalManager::loop()` + `WiFiManager::loop()` in `loop()` (before/after the MDP loop is fine — both non-blocking)
   - On each MDP TELEMETRY emit, also call `TelemetryBroadcast::broadcastTelemetry(...)` for the WS path
6. Add `src/integration/` bridge files (MDP↔Portal interop, shared sensor state struct)
7. Add new MDP command IDs reserved for portal config-change events (so Jetson sees config changes too): `0x0040`–`0x004F` range
8. Update `docs/PORT_8787_HTTP_API_SPEC_MAY19_2026.md` to note the device-direct HTTP path
9. Update `agents/src/mycobrain_agent/upstream/` to optionally probe LAN for direct-WiFi devices and report them to MAS heartbeat
10. Add a build-time toggle `-DCAPTIVE_PORTAL=0` to compile out the portal for memory-constrained scenarios (Hyphae 1 has 8MB flash — fits easily; this is for future smaller boards)
11. Push as PR #7 against main, with a request for review focused on the bridge layer

## Risks and mitigations

| Risk | Mitigation |
|------|-----------|
| LittleFS partition consumes flash needed by BSEC2 NN tables | `default_16MB.csv` has 12MB+ app space — measure before final lib_deps lock |
| AsyncWebServer + ESP32 BLE + WiFi memory pressure | Mushroom 1 / Hyphae 1 have 8MB PSRAM; explicitly enable PSRAM-backed buffers in `WebSocketServer::begin()` |
| Telemetry duplication causes confusion in MQTT | Agent's MQTT publisher already dedupes by `(device_id, seq)`; new portal-source frames use a different `src_path` field NatureOS UI shows separately |
| WiFi STA mid-flight pulls power; affects sensor accuracy | Add `WiFiManager::setLowPowerMode(true)` for battery-only deployments; ConfigManager exposes the toggle |
| Captive portal AP defaults to open WiFi (security) | Default AP password = device serial last 6 hex; require password change on first STA config save |
| `firmware/side_a/` build is still broken on CI | Independent of this work; fix in a separate PR if Garret wants the legacy variant kept |

## Out of scope (next PR after this lands)

- Port the OpenClaw skills' JSON-over-UART bridge into the portal's HTTP layer so the OpenClaw daemon can ALSO talk to the device via WiFi directly (currently UART-only)
- Per-deployment provisioning UI in NatureOS that pushes calibration profiles to the device's `POST /api/calibration`
- Firmware OTA via the portal (`POST /api/ota` with signed bundle)

---

## After this PR lands

A field MycoBrain has three independent paths into Mycosoft's stack:

1. **UART → Jetson → mycobrain-agent → MQTT/HTTP → NatureOS** (production "MDP" path)
2. **WiFi → device's own HTTP/WS → NatureOS device manager** (Berto's path, now also on MDP firmware)
3. **OpenClaw daemon → JSON over UART → Side B → Side A** (claw-only control, unchanged)

All three can run simultaneously. NatureOS sees one device card and the operator picks which transport to control through based on what's healthy.
