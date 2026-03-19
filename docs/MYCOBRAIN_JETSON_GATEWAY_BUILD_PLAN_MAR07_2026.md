# MycoBrain → Jetson → MYCA Server Gateway Build Plan

**Date:** March 7, 2026  
**Status:** Plan (Ready for Build)  
**Scope:** Hardware, firmware, and connection plan for three Jetson tiers: Mushroom 1, Hyphae 1, Gateway

---

## Table of Contents

1. [Overview](#1-overview)
2. [Tier Assignment and Capabilities](#2-tier-assignment-and-capabilities)
3. [Tier 1 — Mushroom 1 (Yahboom Orin NX Super)](#3-tier-1--mushroom-1-yahboom-orin-nx-super)
4. [Tier 2 — Hyphae 1 (Waveshare Jetson Nano/Xavier NX)](#4-tier-2--hyphae-1-waveshare-jetson-nanoxavier-nx)
5. [Tier 3 — Gateway Node (Jetson Nano B01 4GB)](#5-tier-3--gateway-node-jetson-nano-b01-4gb)
6. [Connection Architecture](#6-connection-architecture)
7. [MycoBrain Firmware Configuration per Tier](#7-mycobrain-firmware-configuration-per-tier)
8. [Firmware Paths (mycobrain repo)](#8-firmware-paths-mycobrain-repo)
9. [Related Documents](#9-related-documents)

---

## 1. Overview

This plan connects **MycoBrain** dual-ESP32 hardware to **NVIDIA Jetson** boards at three capability tiers, forming a gateway into the **MYCA server** (MAS at 192.168.0.188:8001, MycoBrain service on port 8003).

| Tier | Device Role | Jetson Board | Compute | Primary Use |
|------|-------------|--------------|---------|-------------|
| 1 | **Mushroom 1** | Yahboom Orin NX Super 157TOPS | 117–157 TOPS | Direct USB → PC → Switch → Server (testing) |
| 2 | **Hyphae 1** | Waveshare Jetson Nano/Xavier NX | ~0.5–21 TOPS | Via Gateway (WiFi + LoRa testing) |
| 3 | **Gateway** | Jetson Nano B01 4GB | ~0.5 TOPS | Gateway node into MYCA server |

---

## 2. Tier Assignment and Capabilities

### Tier 1 — Mushroom 1 (Best)
- **Jetson:** Yahboom Jetson Orin NX Super (157 TOPS, 8GB/16GB RAM)
- **Role:** Primary lab device, full AI (voice, vision), direct server connection

### Tier 2 — Hyphae 1 (Second Best)
- **Jetson:** Waveshare Jetson Nano/Xavier NX
- **Role:** Field/mobile device, WiFi + LoRa backhaul

### Tier 3 — Gateway (Third Best)
- **Jetson:** Jetson Nano B01 4GB
- **Role:** LoRa/WiFi gateway, aggregates Hyphae 1 and other remote devices

---

## 7. MycoBrain Firmware Configuration per Tier

### Device Roles (NVS / build flags)

| Tier | Device | `device_role` | Side B Flags |
|------|--------|---------------|--------------|
| 1 | Mushroom 1 | `mushroom1` | `ENABLE_WIFI=1`, `ENABLE_LORA=0` or `1` |
| 2 | Hyphae 1 | `hyphae1` | `ENABLE_LORA=1`, `ENABLE_WIFI=1` |
| 3 | Gateway | `gateway` | `ENABLE_LORA=1` (rx only), USB serial out |

---

## 8. Firmware Paths (mycobrain repo)

```
mycobrain/firmware/
├── MycoBrain_SideA_MDP/   # Sensors, telemetry, MDP commands
├── MycoBrain_SideB_MDP/   # LoRa, WiFi, transport directives
├── common_mdp/            # Shared MDP implementation
└── README.md
```

### Build and Flash

```powershell
# Side A (Mushroom 1)
cd mycobrain/firmware/MycoBrain_SideA_MDP
pio run
pio run -t upload --upload-port COM7

# Side B (Mushroom 1)
cd mycobrain/firmware/MycoBrain_SideB_MDP
pio run
pio run -t upload --upload-port COM8
```

See `scripts/flash-mycobrain-production.ps1` and `firmware/README.md`.

---

## 9. Related Documents

- **Mycobrain repo:** `docs/FIRMWARE_ARCHITECTURE_FEB10_2026.md`, `docs/MDP_PROTOCOL_CONTRACTS_MAR07_2026.md`
- **Jetson architecture:** `docs/DEVICE_JETSON16_CORTEX_ARCHITECTURE_MAR07_2026.md`, `docs/GATEWAY_JETSON4_LILYGO_ARCHITECTURE_MAR07_2026.md`
- **Deployment:** `deploy/jetson/` (canonical in MAS repo; reference in mycobrain)
- **MAS:** `docs/MYCOBRAIN_TO_MAS_FLOW_MAR07_2026.md` — MycoBrain→MAS heartbeat and device registry flow
