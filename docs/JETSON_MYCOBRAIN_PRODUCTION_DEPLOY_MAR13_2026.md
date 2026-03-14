# Jetson + MycoBrain Production Deployment Guide

**Date:** March 13, 2026  
**Status:** Production reference  
**Related:** `MDP_PROTOCOL_CONTRACTS_MAR07_2026.md`, `firmware/README.md`

---

## Overview

This guide covers deploying production-ready **Mushroom 1** and **Hyphae 1** boxes: Side A firmware, Side B firmware, Jetson on-device operator, Jetson gateway router, flash procedure, and verification.

| Device | Jetson | Role |
|--------|--------|------|
| **Mushroom 1** | AGX Orin 32GB | On-device operator (full AI, multi-sensor) |
| **Hyphae 1** | Orin Nano Super 8GB | On-device operator (lightweight inference) |
| **Gateway** | Orin Nano 4GB | Gateway router (store-and-forward) |

---

## Firmware (mycobrain repo)

| Category | Path |
|----------|------|
| Shared MDP lib | `firmware/common_mdp/include/mdp_codec.h` |
| Side A firmware | `firmware/MycoBrain_SideA_MDP/` |
| Side B firmware | `firmware/MycoBrain_SideB_MDP/` |
| Flash script | `scripts/flash-mycobrain-production.ps1` |

---

## Flash Procedure

### Prerequisites

- **PlatformIO** (`pip install platformio` or VS Code PlatformIO extension)
- USB drivers for ESP32-S3 (CP210x/CH340)
- COM port access (Windows: COM7, COM8; Linux: `/dev/ttyUSB*`)

### Step 1: Flash Side A First

```powershell
# From mycobrain repo root
# Mushroom 1 (dual BME688, no soil)
.\scripts\flash-mycobrain-production.ps1 -Board SideA -Role mushroom1 -Port COM7

# Hyphae 1 (dual BME688 + soil moisture)
.\scripts\flash-mycobrain-production.ps1 -Board SideA -Role hyphae1 -Port COM7
```

### Step 2: Flash Side B

```powershell
.\scripts\flash-mycobrain-production.ps1 -Board SideB -Port COM8
```

Side B has no role (single build). Ensure COM8 is the Side B board.

### Flash Script Options

| Parameter | Values | Required |
|-----------|--------|----------|
| `-Board` | `SideA`, `SideB` | Yes |
| `-Role` | `mushroom1`, `hyphae1` | Side A only (default: mushroom1) |
| `-Port` | `COM3`, `COM7`, etc. | No (auto-detect) |

---

## Jetson Setup (MAS repo)

On-device operator and gateway deploy scripts live in **MAS repo**:

| Category | Path (MAS) |
|----------|------------|
| Telemetry pipeline | `mycosoft_mas/edge/telemetry_pipeline.py` |
| On-device operator | `mycosoft_mas/edge/ondevice_operator.py` |
| Gateway router | `mycosoft_mas/edge/gateway_router.py` |
| On-device deploy | `deploy/jetson-ondevice/` |
| Gateway deploy | `deploy/jetson-gateway/` |

### On-Device Jetson (Mushroom 1 / Hyphae 1)

1. Copy **MAS repo** to Jetson.
2. Run: `./deploy/jetson-ondevice/install.sh`
3. Edit `/etc/mycobrain/operator.env` from `env.mushroom1` or `env.hyphae1` template.
4. Restart: `sudo systemctl restart mycobrain-operator`

### Gateway Jetson

1. Copy MAS repo to Jetson.
2. Run: `./deploy/jetson-gateway/install.sh`
3. Edit `/etc/mycobrain/gateway.env` from `env.gateway` template.
4. Restart: `sudo systemctl restart mycobrain-gateway`

---

## Wiring Diagram

```
Side A (ESP32-S3)          Side B (ESP32-S3)           Jetson
─────────────────          ─────────────────           ──────
UART1 TX (GPIO17) ────────► UART_RX (Side B) ──────────► /dev/ttyUSB0
UART1 RX (GPIO18) ◄──────── UART_TX (Side B) ◄────────── Jetson UART TX
```

---

## Verification Checklist

### Firmware

- [ ] Side A: HELLO frame with correct `role` (mushroom1/hyphae1) and `fw_version: side-a-mdp-2.0.0`
- [ ] Side B: Heartbeat frames every 5s with `fw_version: side-b-mdp-2.0.0`
- [ ] Telemetry frames contain BME688 data

### On-Device Operator

- [ ] Health: `curl http://<jetson-ip>:8080/health` returns 200
- [ ] Telemetry latest: `curl http://<jetson-ip>:8080/telemetry/latest`
- [ ] MAS registry: Device appears in `GET /api/devices/network`
- [ ] NLM translate: Telemetry reaches MAS NLM `/api/translate`

---

## Related Documentation

- `MDP_PROTOCOL_CONTRACTS_MAR07_2026.md` — MDP rail contracts, gateway upstream
- `firmware/README.md` — Firmware layout, flash procedure
