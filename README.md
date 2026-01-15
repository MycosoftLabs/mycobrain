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

## 📁 Firmware

### Side A (Standard Operation)
```
firmware/sideA_firmware.cpp
```
Standard environmental monitoring mode.

### Side B (Science/Comms)
```
firmware/sideB_firmware.cpp
```
Advanced science and communication mode with:
- Extended telemetry
- OTA updates
- Remote configuration

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

- [Firmware Features](../WEBSITE/website/docs/MYCOBRAIN_FIRMWARE_FEATURES.md)
- [Integration Guide](../WEBSITE/website/docs/MYCOBRAIN_INTEGRATION_COMPLETE.md)
- [Sensor Library](../WEBSITE/website/docs/MYCOBRAIN_SENSOR_LIBRARY.md)

## 🔨 Building Firmware

1. Install Arduino IDE 2.x
2. Add ESP32 board support
3. Install libraries:
   - BSEC2
   - Adafruit NeoPixel
   - ArduinoJson
   - WebSockets
4. Open `sideA_firmware.cpp` or `sideB_firmware.cpp`
5. Select ESP32S3 Dev Module
6. Configure settings as above
7. Upload

## 📝 Changelog

### 2026-01-15
- Integrated with CREP dashboard
- Added real-time device markers on map
- Enhanced WebSocket reconnection logic
- Added dual BME688 sensor support

## 📜 License

Copyright © 2026 Mycosoft. All rights reserved.
