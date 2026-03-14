# MycoBrain - Environmental IoT Sensor Platform

> **Version**: 2.0.0  
> **Last Updated**: 2026-01-15T14:30:00Z

## Overview

MycoBrain is Mycosoft's IoT environmental sensing platform based on the ESP32-S3 microcontroller. It provides real-time environmental monitoring for:

- Temperature & Humidity
- Air Quality (VOCs, CO2)
- Barometric Pressure
- Light Levels
- Volatile Compound Analysis (smell training)

## 🔧 Hardware Configuration

### ESP32-S3 Dev Module Settings (Arduino IDE)

| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module |
| USB CDC on boot | Enabled |
| USB DFU on boot | Enabled |
| USB Mode | UART0/Hardware CDC |
| JTAG Adapter | Integrated USB JTAG |
| PSRAM | OPI PSRAM |
| CPU Frequency | 240 MHz |
| Flash Mode | QIO @ 80 MHz |
| Flash Size | 16 MB |
| Partition Scheme | 16MB flash, 3MB app/9.9MB FATFS |
| Upload Speed | 921600 |

### Pin Assignments

| Function | GPIO |
|----------|------|
| NeoPixel LED (SK6805) | 15 |
| Buzzer | 16 |
| I2C SDA | 5 |
| I2C SCL | 4 |
| MOSFET Outputs | 12, 13, 14 |
| Analog Inputs | 6, 7, 10, 11 |
| Serial TXD0/RXD0 | 43/44 |
| Serial TXD1/RXD1 | 17/18 |
| Serial TXD2/RXD2 | 8/9 |

### BME688 Dual Sensor Setup

Two BME688 sensors with different I2C addresses:
- **AMB (Ambient)**: Address 0x77
- **ENV (Environment)**: Address 0x76

Solder bridge on each sensor determines address.

## 📁 Firmware (v2.0.0 MDP Production)

Production firmware uses **MDP v1** (COBS framing, CRC-16) for Side A ↔ Jetson ↔ Side B.

| Build | Path | Purpose |
|-------|------|---------|
| **Side A** | `firmware/MycoBrain_SideA_MDP/` | Sensor MCU (mushroom1, hyphae1 roles) |
| **Side B** | `firmware/MycoBrain_SideB_MDP/` | Router MCU (UART bridge) |
| **Shared** | `firmware/common_mdp/` | MDP codec |

See **[firmware/README.md](firmware/README.md)** for:
- Flash procedure (`scripts/flash-mycobrain-production.ps1`)
- Jetson integration (Mushroom 1, Hyphae 1, Gateway)
- MDP and NLM interactions

## 🔌 WebSocket Protocol

MycoBrain devices connect to the website via WebSocket:

```
ws://mycosoft.com:8765/ws/{device_id}
```

### Message Format

```json
{
  "type": "telemetry",
  "device_id": "mycobrain-001",
  "timestamp": "2026-01-15T14:30:00Z",
  "data": {
    "temperature": 22.5,
    "humidity": 65.0,
    "pressure": 1013.25,
    "iaq": 85,
    "co2_equivalent": 450,
    "voc_equivalent": 0.5
  }
}
```

### Commands

| Command | Description |
|---------|-------------|
| `led:color:RRGGBB` | Set LED color |
| `buzzer:tone:freq:duration` | Play tone |
| `config:interval:ms` | Set telemetry interval |
| `reboot` | Restart device |
| `ota:url` | Start OTA update |

## 🔗 Website Integration

MycoBrain devices are managed via:
- `/devices` - Device list and status
- `/devices/{id}` - Device details
- `/dashboard/crep` - Real-time map view

## 📡 API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/mycobrain/devices` | GET | List devices |
| `/api/mycobrain/devices/{id}` | GET | Device details |
| `/api/mycobrain/devices/{id}/telemetry` | GET | Historical data |
| `/api/mycobrain/devices/{id}/command` | POST | Send command |

## 🔧 CLI Commands (Firmware)

| Command | Description |
|---------|-------------|
| `status` | Show device status |
| `sensors` | Read all sensors |
| `wifi:ssid:password` | Configure WiFi |
| `i2c <sda> <scl> [hz]` | Scan I2C bus |
| `led <r> <g> <b>` | Set LED color |
| `reboot` | Restart device |

## 📚 Documentation

- [Firmware README](firmware/README.md) — v2.0.0 layout, flash procedure, Jetson integration
- [MDP Protocol Contracts](docs/MDP_PROTOCOL_CONTRACTS_MAR07_2026.md) — MDP rail and gateway upstream
- [Jetson Production Deploy](docs/JETSON_MYCOBRAIN_PRODUCTION_DEPLOY_MAR13_2026.md) — BOM, wiring, Jetson setup
- [Firmware Architecture](docs/FIRMWARE_ARCHITECTURE_FEB10_2026.md)

## 🔨 Building and Flashing Firmware

1. Install **PlatformIO** (`pip install platformio` or VS Code PlatformIO extension)
2. USB drivers for ESP32-S3 (CP210x/CH340)
3. From repo root:
   ```powershell
   # Mushroom 1
   .\scripts\flash-mycobrain-production.ps1 -Board SideA -Role mushroom1 -Port COM7
   # Hyphae 1
   .\scripts\flash-mycobrain-production.ps1 -Board SideA -Role hyphae1 -Port COM7
   # Side B
   .\scripts\flash-mycobrain-production.ps1 -Board SideB -Port COM8
   ```

See [firmware/README.md](firmware/README.md) for full procedure.

## 📝 Changelog

### 2026-01-15
- Integrated with CREP dashboard
- Added real-time device markers on map
- Enhanced WebSocket reconnection logic
- Added dual BME688 sensor support

## 📜 License

Copyright © 2026 Mycosoft. All rights reserved.
