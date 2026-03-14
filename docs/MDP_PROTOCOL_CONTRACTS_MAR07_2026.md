# MDP Internal Rail and Gateway Upstream Protocol Contracts

**Date:** March 7, 2026  
**Status:** Active  
**Source:** Copied from MAS; firmware lives in mycobrain repo  
**Related:** `JETSON_MYCOBRAIN_PRODUCTION_DEPLOY_MAR13_2026.md`, `firmware/common_mdp/include/mdp_codec.h`

---

## Overview

This doc specifies the **MDP internal rail contracts** (Side A ↔ Jetson16GB ↔ Side B) and the **gateway upstream publish contracts** (4GB Jetson gateway → MAS, Mycorrhizae, MINDEX, website) for the two-Jetson MycoBrain field architecture.

---

## 1. MDP Internal Rail Contracts

The internal device rail uses **MDP v1** exclusively. All frames are COBS-framed with CRC-16.

### 1.1 Legs and Endpoints

| Leg | Source | Dest | Transport | Endpoints (src, dst) |
|-----|--------|------|-----------|----------------------|
| Side A → Jetson16 | Side A | Jetson16 | UART/Serial | SIDE_A (0xA1) → GATEWAY (0xC0) |
| Jetson16 → Side A | Jetson16 | Side A | UART/Serial | GATEWAY (0xC0) → SIDE_A (0xA1) |
| Jetson16 → Side B | Jetson16 | Side B | UART/Serial | GATEWAY (0xC0) → SIDE_B (0xB1) |
| Side B → Jetson16 | Side B | Jetson16 | UART/Serial | SIDE_B (0xB1) → GATEWAY (0xC0) |

**Reference:** `Mycorrhizae/mycorrhizae-protocol/docs/MDP_V1_SPECIFICATION_FEB23_2026.md`

### 1.2 Message Types (MDP)

| Type | Value | Use |
|------|-------|-----|
| TELEMETRY | 0x01 | Sensor data, streaming samples |
| COMMAND | 0x02 | Commands to Side A or Side B |
| ACK | 0x03 | Acknowledgment (when ACK_REQUESTED) |
| EVENT | 0x05 | Device events (estop, fault, link up/down) |
| HELLO | 0x06 | Handshake, identity, capability exchange |

### 1.3 Side A Command Families (Jetson16 → Side A)

Deterministic commands the Jetson cortex may send to Side A. Payload is JSON in MDP frame.

| Command | Payload shape | Side A response |
|---------|---------------|-----------------|
| `read_sensors` | `{"cmd":"read_sensors","params":{"sensors":["bme1","bme2"]}}` | TELEMETRY with sensor readings |
| `enable_peripheral` | `{"cmd":"enable_peripheral","params":{"id":"bme688_1","en":true}}` | ACK |
| `disable_peripheral` | `{"cmd":"disable_peripheral","params":{"id":"bme688_1"}}` | ACK |
| `output_control` | `{"cmd":"output_control","params":{"id":"led1","value":1}}` | ACK |
| `estop` | `{"cmd":"estop","params":{}}` | ACK + EVENT |
| `health` | `{"cmd":"health","params":{}}` | EVENT with health status |
| `stream_sensors` | `{"cmd":"stream_sensors","params":{"rate_hz":1,"sensors":["bme1"]}}` | TELEMETRY stream |

### 1.4 Side B Command Families (Jetson16 → Side B)

Transport directives only. Side B does not interpret application logic.

| Command | Payload shape | Side B response |
|---------|---------------|-----------------|
| `lora_send` | `{"cmd":"lora_send","params":{"payload":"...","qos":1}}` | ACK, EVENT on link state |
| `ble_advertise` | `{"cmd":"ble_advertise","params":{"en":true,"interval_ms":100}}` | ACK |
| `wifi_connect` | `{"cmd":"wifi_connect","params":{"ssid":"...","pass":"..."}}` | ACK, EVENT on connect |
| `sim_send` | `{"cmd":"sim_send","params":{"dest":"...","payload":"..."}}` | ACK |
| `transport_status` | `{"cmd":"transport_status","params":{}}` | EVENT with link/queue state |

### 1.5 Side A → Jetson16 (Upstream)

Side A sends:

- **TELEMETRY**: Sensor readings (`{"ai1":1.2,"temp":22.5,...}`)
- **EVENT**: Estop, fault, peripheral ready
- **HELLO**: Identity, capability manifest, firmware version

### 1.6 Side B → Jetson16 (Upstream)

Side B sends:

- **EVENT**: Link state (LoRa/WiFi/BLE/SIM up/down), queue depth, retry counts
- **TELEMETRY**: Only if Side B has its own diagnostics (e.g. RSSI)
- **ACK**: For COMMANDs with ACK_REQUESTED

### 1.7 Sequence and ACK Rules

- `seq` increments per sender; `ack` echoes received `seq` when acknowledging
- Use `ACK_REQUESTED` flag for commands that require confirmation
- Jetson16 arbitrates: only one COMMAND in flight per leg unless pipelining is explicitly specified

---

## 2. Gateway Upstream Publish Contracts

The 4GB Jetson gateway terminates device traffic from many Side Bs (LoRa, BLE, WiFi, SIM) and publishes upstream. It does **not** replace canonical device identity.

### 2.1 MAS Device Registry

**Endpoint:** `POST http://{MAS_HOST}:8001/api/devices/heartbeat`

See MAS docs for heartbeat schema and device relay format.

### 2.2 Mycorrhizae Channels / Envelopes

**Protocol:** MMP v1. Gateway publishes device telemetry as MMP envelopes. See Mycorrhizae docs.

### 2.3 MINDEX Telemetry / FCI

Gateway forwards device telemetry to MINDEX API. Payload shape must match existing telemetry schema.

### 2.4 Identity Rules (Invariants)

- **Canonical `device_id`** always belongs to the MycoBrain / Side A identity
- Gateway has its own `gateway_id`; it may use `device_id: "gateway-{id}"` for self-registration
- When relaying device data, the `device_id` must be the **original device identity**, never the gateway

---

## 3. Related Docs

- `JETSON_MYCOBRAIN_PRODUCTION_DEPLOY_MAR13_2026.md` — Flash procedure, Jetson setup, verification
- `firmware/common_mdp/include/mdp_codec.h` — MDP codec implementation
- Mycorrhizae: `MDP_V1_SPECIFICATION_FEB23_2026.md`, `MMP_V1_SPECIFICATION_FEB23_2026.md`
