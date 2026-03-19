# Jetson + MycoBrain Hardware Plan

**Date:** March 9, 2026  
**Author:** MYCA Coding Agent  
**Status:** Draft — Pending CEO Approval  

---

## Overview

Two MycoBrain + Jetson configurations sharing the same architecture but at different performance tiers:

| Device | Jetson Module | Budget | Role |
|--------|--------------|--------|------|
| **Mushroom 1** | Jetson AGX Orin 32GB | ~$1,200 | Primary fungal biocomputer — full AI inference, multi-sensor fusion, FCI processing |
| **Hyphae 1** | Jetson Orin Nano Super 8GB | ~$275 | Edge sensor node — lightweight inference, environmental monitoring, mesh relay |

Both devices pair a Jetson (AI compute) with an ESP32-S3 MycoBrain board (sensor I/O, LoRa, bioelectric interface). The Jetson handles inference and orchestration; the ESP32 handles real-time sensor acquisition and radio.

---

## Architecture: Shared Design

```
┌─────────────────────────────────────────────────────┐
│                   Jetson Module                      │
│  ┌───────────┐  ┌──────────┐  ┌──────────────────┐  │
│  │ AI Engine │  │ Container│  │  MAS Agent        │  │
│  │ (TensorRT)│  │ Runtime  │  │  (FastAPI + WS)   │  │
│  └─────┬─────┘  └────┬─────┘  └────────┬─────────┘  │
│        │              │                  │            │
│        └──────────────┼──────────────────┘            │
│                       │                               │
│              USB-C / UART Serial                      │
│              (115200 baud, MDP v1)                    │
└───────────────────────┬───────────────────────────────┘
                        │
┌───────────────────────┴───────────────────────────────┐
│                ESP32-S3 MycoBrain Board                │
│  ┌──────────┐  ┌──────────┐  ┌────────┐  ┌────────┐  │
│  │ BME688   │  │ BME688   │  │  FCI   │  │  LoRa  │  │
│  │ AMB 0x77 │  │ ENV 0x76 │  │ Driver │  │ SX1262 │  │
│  └──────────┘  └──────────┘  └────────┘  └────────┘  │
│  ┌──────────┐  ┌──────────┐  ┌────────┐              │
│  │ NeoPixel │  │  Piezo   │  │  BLE   │              │
│  │ GPIO 15  │  │ GPIO 46  │  │        │              │
│  └──────────┘  └──────────┘  └────────┘              │
└───────────────────────────────────────────────────────┘
```

**USB connections to Jetson:**
- **MycoBrain (ESP32-S3)** — USB serial → typically `/dev/ttyUSB0` or `/dev/ttyACM0` (MDP v1, 115200 baud)
- Use `ls /dev/tty{USB,ACM}*` on the Jetson to discover ports after plugging in devices

**Communication flow:**
1. ESP32 acquires sensor data at hardware speed (I2C, ADC, SPI)
2. ESP32 sends telemetry to Jetson via USB serial (MDP v1 protocol, COBS framing, CRC16)
3. Jetson runs AI inference on fused sensor data (TensorRT)
4. Jetson reports to MAS Orchestrator (192.168.0.188:8001) over WiFi/Ethernet
5. Both devices register via `POST /api/registry/devices` and heartbeat via `POST /api/devices/heartbeat`

---

## Firmware Paths (mycobrain repo)

- **MDP firmware:** `firmware/MycoBrain_SideA_MDP/`, `firmware/MycoBrain_SideB_MDP/`, `firmware/common_mdp/`
- **Flash script:** `scripts/flash-mycobrain-production.ps1`
- See `docs/FIRMWARE_ARCHITECTURE_FEB10_2026.md` and `firmware/README.md`

---

## References

- [NVIDIA Jetson AGX Orin](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-orin/)
- [NVIDIA Jetson Orin Nano Super Dev Kit](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-orin/nano-super-developer-kit/)
- MAS devices: `mycosoft_mas/devices/mushroom1.py`, `mycosoft_mas/devices/base.py`
- Full BOM, deployment phases, network topology: see MAS `docs/JETSON_MYCOBRAIN_HARDWARE_PLAN_MAR09_2026.md` (extended version)
