# Device 16GB Jetson Cortex Architecture

**Date:** March 7, 2026  
**Status:** Current  
**Related:** Jetson-Backed MycoBrain Field Architecture plan, JETSON_OPTIONAL_CORTEX_PATH_MAR07_2026.md, MYCOBRAIN_JETSON_GATEWAY_BUILD_PLAN_MAR07_2026.md

## Overview

The **on-device 16GB Jetson** is the **real cortex** on each MycoBrain-equipped device. It sits between Side A (sensing/control) and Side B (transport), owns local AI/inference, arbitrates all commands, and serves as the device-resident operator that MYCA can talk to.

## Role Summary

| Responsibility | Owner | Notes |
|----------------|-------|-------|
| Sensing, peripheral control, actuation, device identity | Side A | Authoritative source of device_id |
| Camera, audio, Whisper, vision, inference | 16GB Jetson | Runs jetson_server.py |
| Edge operator runtime, decision-making, command arbitration | 16GB Jetson | All commands to Side A/B pass through |
| Transport execution (LoRa, BLE, WiFi, SIM) | Side B | Receives directives from Jetson |
| Upstream publish (MAS, Mycorrhizae, MINDEX) | 4GB Gateway | Not the on-device Jetson |

## Data Flow

```
Side A (sensors) ──MDP──► 16GB Jetson ──MDP──► Side B (transport)
     ▲                         │                      │
     │                         │                      │
     └── deterministic         │   local summaries    └── LoRa/BLE/WiFi/SIM
         commands              │   operator state         to 4GB Gateway
                               ▼
                         MAS (heartbeat)
                         MYCA (operator chat)
                         local audit trail
```

## 16GB Jetson Owns

### 1. Local Inference Stack

- **Camera**: Capture, streaming, vision inference (via jetson_server.py)
- **Audio**: Capture, Whisper transcription (faster-whisper on Jetson)
- **Vision**: Image stats, model inference (extensible)
- **NLM / TinyML**: Local model deployment (edge/tinyml.py, edge/fpga.py as future paths)

### 2. Edge Operator Runtime

- Full edge operator runtime on top of `jetson_server.py`
- Local decision-making from sensors + vision + audio
- Fuses Side A telemetry with camera/audio for context
- Can propose actions, scripts, firmware updates — **must obey safety gates** (see operator-safety doc)

### 3. Command Arbitration

- **All commands to Side A or Side B must pass through the Jetson**
- The Jetson validates, rate-limits, and logs before forwarding
- No direct PC-to-Side-A or PC-to-Side-B bypass when Jetson is present
- Arbitration ensures:
  - Deterministic command ordering
  - No conflicting actuation
  - Audit trail for every command

### 4. MDP Internal Rail

- **Side A ↔ Jetson**: MDP v1
  - Side A → Jetson: TELEMETRY, EVENT frames (sensor reads, peripheral events)
  - Jetson → Side A: COMMAND frames (read_sensors, peripheral control, actuation, health, estop)
- **Jetson ↔ Side B**: MDP v1
  - Jetson → Side B: COMMAND frames (transport directives: send LoRa, BLE broadcast, WiFi connect, etc.)
  - Side B → Jetson: TELEMETRY, EVENT frames (link state, retry status, delivery ACKs)

Reference: MDP_V1_SPECIFICATION (Mycorrhizae protocol docs)

### 5. Local Buffering, Logging, Audit Trail

- Local ring buffer for telemetry and commands
- Persistent audit log for all commands issued to Side A/Side B
- Enables rollback and forensic analysis (see operator-safety)

### 6. MYCA Integration

- MYCA can talk to the on-device Jetson (operator session)
- Jetson reports summaries, operator state, and local decisions
- **Code/firmware mutation** requires explicit approval and safety gates — never auto-apply

## Identity Rules

- **Canonical device_id** belongs to MycoBrain / Side A — never replaced by the Jetson
- Jetson may have: `edge_operator_id`, `operator_session_id`, `local_model`, `camera_id`
- Heartbeats to MAS use the **device_id** from Side A; Jetson does not usurp identity

## Current Implementation vs. Target

| Component | Current | Target |
|-----------|---------|--------|
| Camera, audio, Whisper | jetson_server.py ✓ | Same; extend with vision models |
| MDP serial to Side A | Not in jetson_server | Add MDP client/arbitration layer |
| MDP serial to Side B | Not in jetson_server | Add MDP client/directive layer |
| Edge operator runtime | Not implemented | Design in Phase 2 |
| Command arbitration | N/A | All commands through Jetson |
| Audit trail | N/A | Local ring buffer + log |

## Implementation Phases (from Plan)

- **Phase 1**: Architecture and protocol unification (this doc)
- **Phase 2**: On-device 16GB Jetson runtime — design edge operator on jetson_server.py, safety gates, audit logging, sensor+camera+audio fusion
- **Phase 6**: Stack and UI integration — registry/device-network fields for edge visibility

## References

- [JETSON_OPTIONAL_CORTEX_PATH_MAR07_2026.md](JETSON_OPTIONAL_CORTEX_PATH_MAR07_2026.md)
- [MYCOBRAIN_JETSON_GATEWAY_BUILD_PLAN_MAR07_2026.md](MYCOBRAIN_JETSON_GATEWAY_BUILD_PLAN_MAR07_2026.md)
- Edge operator and jetson_server: **MAS repo** `mycosoft_mas/edge/`
