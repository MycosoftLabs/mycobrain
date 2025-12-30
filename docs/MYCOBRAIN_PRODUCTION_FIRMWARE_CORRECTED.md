# MycoBrain Production Firmware - Corrected Technical Reference

## ⚠️ CRITICAL CORRECTIONS

This document corrects critical hardware pin mapping errors and protocol inconsistencies found in previous documentation.

---

## Hardware Configuration (VERIFIED - ESP32-S3 MycoBrain)

### Side-A (Sensor MCU) Pin Mapping

**VERIFIED FROM SCHEMATIC - DO NOT CHANGE**

```
I2C:          SDA=GPIO5, SCL=GPIO4
Analog:       AIN1=GPIO6, AIN2=GPIO7, AIN3=GPIO10, AIN4=GPIO11
MOSFETs:      OUT1=GPIO12, OUT2=GPIO13, OUT3=GPIO14 (digital outputs)
NeoPixel:     GPIO15 (SK6805, 1 onboard pixel)
Buzzer:       GPIO16 (piezo buzzer, PWM-driven)
```

**⚠️ IMPORTANT**: Previous documentation incorrectly listed analog pins as GPIO34/35/36/39 (classic ESP32 pins). These are **WRONG** for ESP32-S3 and will cause firmware to read incorrect pins or fail.

### Side-B (Router MCU) - Status: EXPERIMENTAL

**⚠️ WARNING**: Side-B router firmware is **experimental** and may not be physically wired in current hardware revisions.

**Theoretical UART Configuration** (if physically implemented):
- Side-B UART: RX=GPIO16, TX=GPIO17 (Side-B's GPIO16, NOT Side-A's)
- Side-A UART: Would need separate pins (NOT GPIO16, which is buzzer)
- **Physical Wiring Required**: A_TX → B_RX, B_TX → A_RX
- **Baud Rate**: 115200 8N1
- **PC Connection**: Typically connects to Side-A USB CDC directly

**Current Reality**: Most deployments use Side-A USB CDC directly. Side-B router is optional/experimental.

---

## Protocol Specification

### Command Format

**Current Implementation**: **Plaintext CLI commands** (NOT JSON commands)

The firmware accepts plaintext commands like:
```
mode machine
dbg off
fmt json
scan
led rgb 255 0 0
buzzer pattern coin
status
```

**JSON Commands**: Some commands can also be sent as JSON:
```json
{"cmd":"ping"}
{"cmd":"set_mosfet","mosfet_index":0,"state":true}
```

**⚠️ IMPORTANT**: The firmware supports **both** formats:
- Plaintext commands (primary, recommended)
- JSON commands (optional, for service compatibility)

### Response Format

**Machine Mode**: NDJSON (newline-delimited JSON)

Every line is a valid JSON object:
```json
{"type":"ack","cmd":"mode","message":"machine","ts":12345}
{"type":"err","cmd":"unknown","error":"Invalid command","ts":12346}
{"type":"telemetry","ts":12347,"board_id":"AA:BB:CC:DD:EE:FF","ai1_voltage":3.3,...}
{"type":"periph_list","ts":12348,"board_id":"AA:BB:CC:DD:EE:FF","peripherals":[...],"count":2}
{"type":"status","ts":12349,"board_id":"AA:BB:CC:DD:EE:FF","status":"ready",...}
```

**Message Types**:
- `ack` - Acknowledgment
- `err` - Error response
- `telemetry` - Sensor data
- `periph_list` - Peripheral discovery results
- `status` - Device status

**Human Mode**: Mixed format (banners, help text, JSON responses)

---

## Firmware Projects

### 1. Side-A Production Firmware

**Location**: `firmware/MycoBrain_SideA/MycoBrain_SideA_Production.ino`

**Purpose**: Primary sensor and control MCU

**Status**: ✅ **Production Ready**

**Features**:
- BME688 environmental sensors (I2C)
- 4 analog inputs (GPIO6, 7, 10, 11)
- 3 MOSFET outputs (GPIO12, 13, 14)
- NeoPixel LED control (GPIO15)
- Buzzer control (GPIO16)
- Machine mode with NDJSON output
- Plaintext + JSON command support

**Protocol**: Serial USB CDC, 115200 baud

### 2. Side-B Router Firmware

**Location**: `firmware/MycoBrain_SideB/MycoBrain_SideB.ino`

**Purpose**: Communication bridge (EXPERIMENTAL)

**Status**: ⚠️ **Experimental** - May not be physically wired

**Features**:
- UART routing between PC and Side-A
- Command forwarding
- Telemetry forwarding
- Connection monitoring

**⚠️ NOTE**: Most deployments use Side-A USB CDC directly. Side-B router is optional.

### 3. ScienceComms Firmware

**Location**: `firmware/MycoBrain_ScienceComms/`

**Purpose**: Advanced science communication capabilities

**Status**: ✅ **Production Ready** (separate firmware)

**Features**:
- Optical modem (LiFi via NeoPixel)
- Acoustic modem (FSK via buzzer)
- Stimulus engine
- Peripheral discovery
- JSON-CLI interface

**⚠️ NOTE**: This is a **separate firmware** that must be flashed instead of Side-A Production. It is NOT merged into Side-A.

---

## Verified Configuration

### Firmware Versions

- **Side-A Production**: v1.0.0 (commit: see git log)
- **Side-B Router**: v1.0.0-production (experimental)
- **ScienceComms**: v1.0.0

### Arduino-ESP32 Core

- **Version**: 3.0.x (ESP32-S3 support required)
- **Board**: ESP32-S3-DevKitC-1
- **Partition Scheme**: 16MB flash, 3MB app / 9.9MB FATFS

### Tested Hardware

- **Board Revision**: MycoBrain V1 (dual ESP32-S3)
- **BME688**: Bosch BME688 (I2C addresses 0x76, 0x77)
- **NeoPixel**: SK6805 (1 pixel, GPIO15)
- **Buzzer**: Piezo buzzer (GPIO16, MOSFET-driven)

### Test Matrix

✅ **Verified Features**:
- [x] I2C scanning
- [x] Analog input reading (GPIO6, 7, 10, 11)
- [x] MOSFET control (GPIO12, 13, 14)
- [x] NeoPixel basic control (GPIO15)
- [x] Buzzer patterns (GPIO16)
- [x] Machine mode initialization
- [x] NDJSON telemetry output
- [x] Peripheral discovery
- [x] Command parsing (plaintext + JSON)

⚠️ **Not Yet Verified**:
- [ ] BME688 full sensor reading (library integration pending)
- [ ] NeoPixelBus library integration (currently basic GPIO control)
- [ ] Side-B UART physical wiring
- [ ] MDP v1 protocol (future)

---

## Peripheral Discovery

### I2C Discovery

**Reality Check**: I2C is the **control/discovery plane**. Streaming data may use SPI/I2S/UART.

**Supported Peripherals**:
- BME688 (0x76, 0x77) - Environmental sensor
- SHT40 (0x44, 0x45) - Temperature/humidity
- BH1750 (0x23) - Light sensor
- SGP40 (0x59) - VOC sensor
- SSD1306 (0x3C, 0x3D) - OLED display
- ADS1115 (0x48, 0x49) - ADC
- MCP23017 (0x20, 0x21) - GPIO expander
- PCA9685 (0x40) - PWM driver

**⚠️ NOT I2C**:
- NeoPixel arrays (one-wire timing, not I2C)
- Cameras (I2C for config, SPI/parallel/USB for data)
- Microphones (I2S, not I2C)
- LiDAR (may use I2C for config, UART/SPI for data)

**NeoPixel Arrays**: Require descriptor dongle or configuration-based declaration (not auto-discovered via I2C scan).

---

## NeoPixel Status

**Current Implementation**: Basic GPIO control (digital on/off, basic patterns)

**NeoPixelBus Library**: **NOT YET INTEGRATED** (roadmap item)

**Roadmap**: Upgrade to NeoPixelBus library for:
- Full RGB color control
- Advanced patterns (rainbow, chase, sparkle)
- Brightness control
- Animation support

**⚠️ Contradiction Resolved**: Previous docs claimed NeoPixelBus was present, but it's actually a future enhancement.

---

## Integration Points

### Service Integration

**Service**: `services/mycobrain/mycobrain_dual_service.py`

**Connection**: Direct USB CDC to Side-A (typically COM5 on Windows)

**Protocol**: Plaintext commands + NDJSON responses

**API Endpoints**:
- `POST /devices/connect` - Connect to device
- `GET /devices` - List connected devices
- `POST /devices/{id}/command` - Send command
- `GET /devices/{id}/status` - Get device status
- `GET /devices/{id}/telemetry` - Get latest telemetry

### Website Integration

**Component**: Device Manager dashboard

**Initialization Sequence**:
1. `mode machine` - Switch to machine mode
2. `dbg off` - Disable debug output
3. `fmt json` - Set JSON format (NDJSON)
4. `scan` - Discover I2C peripherals
5. `status` - Get device status

**Widget Support**:
- LED Control Widget (RGB, patterns, optical TX)
- Buzzer Control Widget (presets, tones, acoustic TX)
- Peripheral Grid (auto-discovery)
- Telemetry Charts (real-time streaming)
- Communication Panel (LoRa/WiFi/BLE/Mesh)

---

## Deployment Notes

### Which Firmware to Use?

**For Production Sensor/Control Use**:
- Flash **Side-A Production Firmware**
- Use USB CDC connection directly
- Side-B router NOT required

**For Science Communication Experiments**:
- Flash **ScienceComms Firmware** (replaces Side-A)
- Provides optical/acoustic modem capabilities
- Cannot run simultaneously with Side-A Production

**For Dual-MCU Router Setup** (Experimental):
- Flash **Side-A Production** on Side-A
- Flash **Side-B Router** on Side-B
- Requires physical UART wiring between MCUs
- PC connects to Side-B USB CDC

---

## Known Limitations

1. **BME688**: Sensor reading library not fully integrated (placeholder)
2. **NeoPixelBus**: Not yet integrated (basic GPIO control only)
3. **Side-B Router**: Experimental, may not be physically wired
4. **MDP v1**: Future protocol, not yet implemented
5. **OTA Updates**: Not yet implemented
6. **WiFi**: Not yet configured

---

## File References

**Firmware**:
- `firmware/MycoBrain_SideA/MycoBrain_SideA_Production.ino`
- `firmware/MycoBrain_SideB/MycoBrain_SideB.ino`
- `firmware/MycoBrain_ScienceComms/`

**Configuration**:
- `firmware/MycoBrain_ScienceComms/include/config.h` (verified pin definitions)

**Services**:
- `services/mycobrain/mycobrain_dual_service.py`

**Documentation**:
- `docs/firmware/WEBSITE_INTEGRATION_UPDATES.md`
- `docs/protocols/MDP_V1_SPEC.md` (future protocol)

---

## Version History

- **v1.0.0** (2024-12): Initial production release
  - Fixed analog pin mapping (GPIO6/7/10/11)
  - Added machine mode support
  - Added NDJSON format
  - Clarified protocol (plaintext + JSON)
  - Documented Side-B experimental status
  - Resolved NeoPixelBus contradiction

---

**Document Version**: 1.0.0 (Corrected)  
**Last Updated**: 2024-12  
**Status**: Production Ready (with noted limitations)

