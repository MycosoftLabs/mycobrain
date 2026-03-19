# 4GB Jetson + LilyGO Gateway Architecture

**Date:** March 7, 2026  
**Status:** Current  
**Related:** Jetson-Backed MycoBrain Field Architecture plan, MYCOBRAIN_JETSON_GATEWAY_BUILD_PLAN_MAR07_2026.md

## Overview

The **on-site 4GB Jetson gateway** is the **communications gateway** for many devices' Side B traffic. It is **not** the AI cortex. It aggregates LoRa, SIM, BLE, and WiFi from LilyGO nodes and device Side Bs, concentrates transport, and publishes upstream to MAS, Mycorrhizae, and MINDEX.

## Role Summary

| Responsibility | Owner | Notes |
|----------------|-------|-------|
| Sensing, control, device identity | Side A (on each device) | Canonical device_id |
| AI cortex, command arbitration | 16GB Jetson (on each device) | Per-device |
| Transport execution (LoRa, BLE, WiFi, SIM) | Side B (on each device) | Sends to gateway |
| **Gateway aggregation, upstream publish** | **4GB Jetson + LilyGO** | This doc |

## Gateway Node Architecture

```
[Device 1 Side B] ──LoRa──┐
[Device 2 Side B] ──LoRa──┼──► [LilyGO LoRa/SIM/BLE/WiFi] ──USB/Serial──► [4GB Jetson Gateway]
[Device 3 Side B] ──BLE───┘              │                                      │
[Device N Side B] ──WiFi── (optional)    └── SIM backhaul (cellular) ───────────┤
                                                                                │
                                                        Ethernet / WiFi         │
                                                                                ▼
                                                                     [MAS 192.168.0.188:8001]
                                                                     [Mycorrhizae Protocol]
                                                                     [MINDEX 192.168.0.189:8000]
                                                                     [Website Ingest API]
```

## 4GB Jetson Gateway Owns

### 1. Site-Local Gateway Aggregation

- Receives telemetry and events from many devices via LoRa, BLE, WiFi
- Concentrates traffic before upstream publish
- Sits physically near the server (or on the same LAN)
- Single point of ingress for field devices when they cannot reach MAS directly

### 2. LilyGO LoRa/SIM/BLE/WiFi Integration

- **LilyGO nodes** (e.g., T-Echo, T-Beat, or custom LoRa+cellular modules) connect to the Jetson via USB or UART
- **LoRa**: Receives packets from device Side Bs (SX1262, 915 MHz or regional)
- **SIM/Cellular**: Optional backhaul when Ethernet/WiFi to server is unavailable; LilyGO can have built-in modem
- **BLE**: Proximity workflows; devices in range can push via BLE to gateway
- **WiFi**: Devices in WiFi range can push directly to gateway HTTP/UDP endpoint

### 3. Server-Adjacent Transport Concentration

- Gateway terminates device traffic and converts to upstream publish format
- One gateway serves many devices
- Reduces load on MAS/Mycorrhizae by batching and normalizing

### 4. Store-and-Forward Buffering

- When upstream (MAS, Mycorrhizae, MINDEX) is unavailable:
  - Buffer telemetry and events locally
  - Retry with backoff
  - Flush when connectivity restored
- Prevents data loss during network outages

### 5. Upstream Publish Contract

Publishes into:

- **MAS**: Heartbeat and device summaries (`POST /api/devices/heartbeat`, device registry)
- **Mycorrhizae**: Channels/envelopes for FCI telemetry, events
- **MINDEX**: Telemetry persistence, FCI data
- **Website Ingest API**: Fallback when needed (`/api/telemetry/ingest` or equivalent)

## Identity Rules

- **Canonical device_id** belongs to each MycoBrain / Side A — never replaced by the gateway
- Gateway may have: `gateway_id`, `lilygo_node_id`, `transport_cluster_id`
- Upstream messages carry `device_id` from Side A; gateway does not usurp device identity

## LilyGO Node Role

- **LoRa RX**: Receives from device Side Bs (SX1262, matching frequency and spreading factor)
- **SIM (optional)**: Cellular backhaul when gateway has no Ethernet/WiFi to server
- **BLE**: Local proximity; devices can advertise/push to gateway when in range
- **WiFi**: Gateway may expose a local WiFi AP or connect to existing WiFi for device pushes

LilyGO hardware examples (for reference; specific BOM in build plan):

- T-Echo: LoRa + cellular
- T-Beat: LoRa + BLE
- Custom: SX1262 + ESP32 + modem module

## Connection to Build Plan

This architecture aligns with:

- **Tier 3 — Gateway Node** in [MYCOBRAIN_JETSON_GATEWAY_BUILD_PLAN_MAR07_2026.md](MYCOBRAIN_JETSON_GATEWAY_BUILD_PLAN_MAR07_2026.md)
- Jetson Nano B01 4GB as the base
- LoRa receiver (SX1262 or RAK3172/Seeed) for uplink
- LilyGO integration extends the build plan with explicit LoRa/SIM/BLE/WiFi aggregation

## Implementation Phases (from Plan)

- **Phase 5**: 4GB Jetson + LilyGO gateway node
  - Define server-adjacent gateway process
  - LoRa intake from many devices
  - SIM fallback and local store-and-forward
  - Upstream publish to MAS / Mycorrhizae / MINDEX / ingest

## References

- [MYCOBRAIN_JETSON_GATEWAY_BUILD_PLAN_MAR07_2026.md](MYCOBRAIN_JETSON_GATEWAY_BUILD_PLAN_MAR07_2026.md)
- [DEVICE_JETSON16_CORTEX_ARCHITECTURE_MAR07_2026.md](DEVICE_JETSON16_CORTEX_ARCHITECTURE_MAR07_2026.md)
- Gateway router: **MAS repo** `mycosoft_mas/edge/gateway_router.py`
