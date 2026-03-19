# MycoBrain Firmware v2.0.0 (MDP Production)

**Date:** March 2026  
**Status:** Production  
**Related:** [MDP Protocol Contracts](../docs/MDP_PROTOCOL_CONTRACTS_MAR07_2026.md), [Jetson Production Deploy](../docs/JETSON_MYCOBRAIN_PRODUCTION_DEPLOY_MAR13_2026.md)

---

## Overview

Production MycoBrain firmware uses **MDP v1** (COBS framing, CRC-16) for the Side A ↔ Jetson ↔ Side B rail. Shared MDP codec lives in `common_mdp/`.

| Build | Path | Roles / Env | Purpose |
|-------|------|-------------|---------|
| **Side A** | `MycoBrain_SideA_MDP/` | `mushroom1`, `hyphae1` | Sensor MCU (BME688 x2, soil for hyphae1), MDP telemetry, commands |
| **Side B** | `MycoBrain_SideB_MDP/` | `esp32-s3-devkitc-1` | Router MCU (UART bridge Side A ↔ Jetson), LoRa/WiFi/BLE transport |
| **Shared** | `common_mdp/` | — | MDP codec (`mdp_codec.h`), COBS, CRC-16 |

---

## Directory Structure

```
firmware/
├── common_mdp/           # Shared MDP codec (include in Side A/B)
│   └── include/
│       └── mdp_codec.h
├── MycoBrain_SideA_MDP/  # Side A v2.0.0 (PlatformIO)
│   ├── platformio.ini    # envs: mushroom1, hyphae1
│   ├── src/main.cpp
│   └── include/
├── MycoBrain_SideB_MDP/  # Side B v2.0.0 (PlatformIO)
│   ├── platformio.ini    # env: esp32-s3-devkitc-1
│   └── src/main.cpp
├── MycoBrain_SideA/      # Legacy (JSON, no MDP)
└── MycoBrain_SideB/      # Legacy (JSON, no MDP)
```

---

## Flash Procedure

Use the flash script from the **mycobrain repo root**:

```powershell
# Mushroom 1 (dual BME688, no soil)
.\scripts\flash-mycobrain-production.ps1 -Board SideA -Role mushroom1 -Port COM7

# Hyphae 1 (dual BME688 + soil moisture)
.\scripts\flash-mycobrain-production.ps1 -Board SideA -Role hyphae1 -Port COM7

# Side B (single build, no role)
.\scripts\flash-mycobrain-production.ps1 -Board SideB -Port COM8
```

**Prerequisites:** PlatformIO (`pio`), ESP32-S3 USB drivers (CP210x/CH340).

---

## Jetson Integration

| Device | Jetson | Role |
|--------|--------|------|
| **Mushroom 1** | AGX Orin 32GB | On-device operator |
| **Hyphae 1** | Orin Nano Super 8GB | On-device operator |
| **Gateway** | Orin Nano 4GB | Gateway router |

- **Side A** sends MDP frames (TELEMETRY, HELLO, EVENT) to Side B.
- **Side B** bridges Side A ↔ Jetson over UART; Jetson runs `ondevice_operator` or `gateway_router`.
- **Jetson** parses MDP, forwards telemetry to MAS/MINDEX/NLM; see [Jetson Production Deploy](../docs/JETSON_MYCOBRAIN_PRODUCTION_DEPLOY_MAR13_2026.md).

Deploy scripts and edge Python code live in the **MAS repo** (`deploy/jetson/`, `edge/`). See [deploy/jetson/README.md](../deploy/jetson/README.md) for reference.

---

## MDP and NLM Interactions

- **MDP** defines the internal rail: Side A ↔ Jetson ↔ Side B. See [MDP Protocol Contracts](../docs/MDP_PROTOCOL_CONTRACTS_MAR07_2026.md).
- **NLM** (Nature Learning Model) receives translated telemetry from the Jetson operator via MAS `/api/translate`.
- **MINDEX** FCI receives telemetry via `/api/fci/telemetry`.
- **MAS** device registry: `POST /api/devices/heartbeat`.

All upstream contracts (MAS, MINDEX, NLM, Mycorrhizae) are documented in the MDP Protocol Contracts doc.

---

## Legacy Firmware (SideA/SideB)

`MycoBrain_SideA/` and `MycoBrain_SideB/` use JSON-over-serial (no MDP). They are retained for backward compatibility. For new deployments, use the MDP production builds.

---

## License

See main project license.
