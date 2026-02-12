# MycoBrain Firmware Architecture

**Date:** February 10, 2026  
**Status:** Active  
**Applies to:** MycoBrain ESP32-S3 Dual-Core Devices

## Overview

The MycoBrain firmware is designed for a dual-ESP32 architecture that separates concerns between two processors:

- **Side A (Peripherals & Sensors):** Always sensing, efficient power use
- **Side B (Communications):** Sometimes communicating, manages all radios

This separation optimizes power consumption, library footprint, and real-time responsiveness.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          MycoBrain Board                                  │
├──────────────────────────────┬───────────────────────────────────────────┤
│         Side A (ESP32)       │           Side B (ESP32)                   │
├──────────────────────────────┼───────────────────────────────────────────┤
│ • Sensors (BME688, etc.)     │ • WiFi                                    │
│ • NeoPixel LED               │ • Bluetooth Low Energy                    │
│ • Buzzer                     │ • LoRa (SX1262)                           │
│ • Power Management           │ • Acoustic/Optical Modems                 │
│ • I2C Peripherals            │ • Device-to-Device Comms                  │
│ • Battery/Solar Monitoring   │ • Gateway Functions                       │
├──────────────────────────────┴───────────────────────────────────────────┤
│                   UART (MDP Protocol - COBS + CRC16)                      │
└──────────────────────────────────────────────────────────────────────────┘
```

## Device Identity System

Each MycoBrain device can be configured with a **device role** and **display name** to identify its function in the Mycosoft ecosystem.

### Device Roles

| Role | Description | Use Case |
|------|-------------|----------|
| `mushroom1` | Mushroom 1 outdoor sensor station | Field-deployed monitoring station |
| `sporebase` | SporeBase spore and aerosol collector | Controlled environment automation |
| `hyphae1` | Hyphae 1 modular sensor box | Distributed environmental sensing |
| `alarm` | indoor home Alarm environmental alert | Safety and early warning system |
| `gateway` | Network gateway/hub device | Aggregates data from field devices |
| `mycodrone` | MycoDrone central brain | Aerial platform controller |
| `standalone` | Generic MycoBrain board | Development and testing |

### Configuration via NVS

Device identity is stored in ESP32 Non-Volatile Storage (NVS):

```cpp
// NVS keys
#define NVS_NAMESPACE "mycobrain"
#define NVS_DEVICE_ROLE_KEY "dev_role"
#define NVS_DEVICE_DISPLAY_NAME_KEY "dev_disp"

// Load on boot
void loadDeviceIdentity() {
  prefs.begin(NVS_NAMESPACE, true);
  String role = prefs.getString(NVS_DEVICE_ROLE_KEY, "standalone");
  String name = prefs.getString(NVS_DEVICE_DISPLAY_NAME_KEY, "");
  prefs.end();
}
```

### Configuration Commands (MDP/JSON)

```json
// Set device role
{"command": "set_device_role", "role": "mushroom1"}

// Set display name
{"command": "set_device_display_name", "name": "Mushroom 1 - Field Station Alpha"}

// Get current identity
{"command": "get_device_identity"}
// Response:
{"type": "device_identity", "device_role": "mushroom1", "device_display_name": "...", "ts": 12345}
```

## Side A Firmware

### Purpose

Side A handles all sensors and peripherals, operating in an "always sensing" mode:

- Environmental sensors (BME688 temperature, humidity, pressure, gas)
- Visual feedback (NeoPixel RGB LED)
- Audio feedback (MOSFET-driven buzzer)
- Power management (battery/solar monitoring)
- I2C expansion bus

### Key Files

| File | Purpose |
|------|---------|
| `firmware/side_a/src/main.cpp` | Main application (PlatformIO dual-ESP32) |
| `firmware/side_a/include/config.h` | Hardware pin definitions |
| `firmware/side_a/include/pixel.h` | NeoPixel module interface |
| `firmware/side_a/src/pixel.cpp` | NeoPixel implementation (NeoPixelBus) |
| `firmware/side_a/include/buzzer.h` | Buzzer module interface |
| `firmware/side_a/src/buzzer.cpp` | Buzzer implementation (LEDC PWM) |
| `firmware/MycoBrain_SideA/MycoBrain_SideA_Production.ino` | Arduino single-ESP32 variant |

### Hardware Pin Map (Side A)

```cpp
// From config.h
#define PIN_NEOPIXEL     15    // WS2812/SK6805 data
#define PIN_BUZZER       16    // MOSFET gate
#define PIN_I2C_SDA      21
#define PIN_I2C_SCL      22
#define PIN_BATTERY_ADC  34    // Battery voltage divider
#define PIN_SOLAR_ADC    35    // Solar panel voltage
```

### NeoPixel Module

Non-blocking RGB LED control with named patterns:

```cpp
namespace Pixel {
  void init();
  void setColor(uint8_t r, uint8_t g, uint8_t b);
  void setBrightness(uint8_t brightness);
  void off();
  void startPattern(const char* patternName);  // "pulse", "rainbow", "alert", etc.
  void updatePattern();  // Call in loop() for animation
  String getStatus();
}
```

**Available Patterns:**
- `solid` - Static color
- `pulse` - Breathing effect
- `rainbow` - Color cycling
- `alert` - Red flashing
- `success` - Green pulse
- `connecting` - Blue pulse

### Buzzer Module

Non-blocking audio feedback with preset patterns:

```cpp
namespace Buzzer {
  void init();
  void tone(uint16_t freq, uint16_t durationMs);
  void stop();
  void playPattern(BuzzerPattern pattern);
  void updatePattern();  // Call in loop() for sequences
  String getStatus();
}

enum BuzzerPattern {
  PATTERN_NONE = 0,
  PATTERN_BEEP,      // Single beep
  PATTERN_SUCCESS,   // Ascending tones
  PATTERN_ERROR,     // Descending tones
  PATTERN_ALERT,     // Urgent alarm
  PATTERN_COIN,      // Mario coin sound
  PATTERN_STARTUP    // Boot chime
};
```

### MDP Commands (Side A)

| Command ID | Name | Data |
|------------|------|------|
| 0x0001 | CMD_STATUS | (none) - Request status |
| 0x0004 | CMD_SET_DEVICE_ROLE | role (string, null-term) |
| 0x0005 | CMD_SET_DEVICE_DISPLAY_NAME | name (string, null-term) |
| 0x0006 | CMD_GET_DEVICE_IDENTITY | (none) |
| 0x0010 | CMD_PIXEL_SET_COLOR | r(u8), g(u8), b(u8) |
| 0x0011 | CMD_PIXEL_SET_BRIGHTNESS | brightness(u8) |
| 0x0012 | CMD_PIXEL_PATTERN | pattern name (string) |
| 0x0013 | CMD_PIXEL_OFF | (none) |
| 0x0020 | CMD_BUZZER_TONE | freq(u16), duration_ms(u16) |
| 0x0021 | CMD_BUZZER_PATTERN | pattern name (string) |
| 0x0022 | CMD_BUZZER_STOP | (none) |

## Side B Firmware

### Purpose

Side B handles all communications, operating in "sometimes communicating" mode to optimize power:

- LoRa long-range radio (SX1262)
- WiFi for high-bandwidth local communication
- Bluetooth Low Energy for proximity/mesh
- Future: Acoustic/optical modems

### Build Flags

Communication modules are enabled/disabled via PlatformIO build flags:

```ini
# platformio.ini
[env:mycobrain-side-b]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

build_flags =
  -DENABLE_LORA=1      # LoRa always enabled for Side B
  -DENABLE_WIFI=0      # WiFi disabled by default (power)
  -DENABLE_BLE=0       # BLE disabled by default (power)

# WiFi variant for gateway/stationary devices
[env:mycobrain-side-b-wifi]
extends = env:mycobrain-side-b
build_flags =
  -DENABLE_LORA=1
  -DENABLE_WIFI=1
  -DENABLE_BLE=0

# BLE variant for proximity/mesh
[env:mycobrain-side-b-ble]
extends = env:mycobrain-side-b
build_flags =
  -DENABLE_LORA=1
  -DENABLE_WIFI=0
  -DENABLE_BLE=1

# Full comms (all radios - high power consumption)
[env:mycobrain-side-b-full]
extends = env:mycobrain-side-b
build_flags =
  -DENABLE_LORA=1
  -DENABLE_WIFI=1
  -DENABLE_BLE=1
```

### Key Files

| File | Purpose |
|------|---------|
| `firmware/side_b/src/main.cpp` | Main application with conditional comms |
| `firmware/side_b/platformio.ini` | Build variants and flags |
| `firmware/common/mdp_types.h` | MDP protocol definitions |
| `firmware/common/mdp_utils.h` | COBS/CRC encoding utilities |

### Hardware Pin Map (Side B - LoRa)

```cpp
// SX1262 LoRa module
constexpr int LORA_RST  = 7;
constexpr int LORA_BUSY = 12;
constexpr int LORA_SCK  = 18;
constexpr int LORA_NSS  = 17;
constexpr int LORA_MISO = 19;
constexpr int LORA_MOSI = 20;
constexpr int LORA_DIO1 = 21;
constexpr float LORA_FREQ_MHZ = 915.0;  // US ISM band
```

### WiFi Configuration

When `ENABLE_WIFI=1`, WiFi credentials are stored in NVS:

```cpp
// WiFi credentials (NVS or compile-time defaults)
#define WIFI_SSID_DEFAULT ""
#define WIFI_PASS_DEFAULT ""

// Gateway server for telemetry forwarding
#define GATEWAY_HOST_DEFAULT "192.168.0.188"
#define GATEWAY_PORT_DEFAULT 8001

// UDP port for telemetry
constexpr uint16_t WIFI_UDP_PORT = 5555;
```

### BLE Configuration

When `ENABLE_BLE=1`, BLE GATT service is created:

```cpp
#define BLE_DEVICE_NAME "MycoBrain"
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHAR_TX_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Notify
#define BLE_CHAR_RX_UUID        "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // Write
```

## MDP Protocol (MycoBrain Device Protocol)

### Overview

MDP is the binary protocol used for inter-ESP32 communication and gateway-to-device messaging.

**Features:**
- COBS framing (Consistent Overhead Byte Stuffing)
- CRC16 error detection
- ACK/retry reliability layer
- Support for telemetry, commands, events, and ACKs

### Frame Structure

```
┌─────────┬─────────┬──────────────────┬───────┬─────────┐
│ COBS    │ Header  │     Payload      │ CRC16 │  0x00   │
│ Encode  │ (12B)   │   (0-888 B)      │ (2B)  │ Delim   │
└─────────┴─────────┴──────────────────┴───────┴─────────┘
```

### Header Format (12 bytes)

```cpp
struct mdp_hdr_v1_t {
  uint16_t magic;     // 0x4D44 ("MD")
  uint8_t  version;   // 0x01
  uint8_t  msg_type;  // MDP_TELEMETRY, MDP_COMMAND, MDP_ACK, etc.
  uint32_t seq;       // Sequence number
  uint32_t ack;       // Last ACK'd sequence
  uint8_t  flags;     // IS_ACK, ACK_REQUESTED, etc.
  uint8_t  src;       // Source endpoint (EP_SIDE_A, EP_SIDE_B, EP_GATEWAY)
  uint8_t  dst;       // Destination endpoint
  uint8_t  rsv;       // Reserved
};
```

### Message Types

| Type | Value | Description |
|------|-------|-------------|
| MDP_TELEMETRY | 0x01 | Sensor data from Side A |
| MDP_COMMAND | 0x02 | Control command to device |
| MDP_ACK | 0x03 | Acknowledgment |
| MDP_EVENT | 0x04 | Async event notification |
| MDP_HEARTBEAT | 0x05 | Keep-alive |

## Telemetry Format

Side A emits telemetry as JSON in the MDP payload:

```json
{
  "type": "telemetry",
  "device_id": "MYCOBRAIN-ABC123",
  "device_role": "mushroom1",
  "device_display_name": "Mushroom 1 - Station Alpha",
  "ts": 1739145600000,
  "sensors": {
    "bme688_0": {
      "temperature": 22.5,
      "humidity": 65.2,
      "pressure": 1013.25,
      "gas_resistance": 50000,
      "iaq": 45,
      "co2_eq": 520
    }
  },
  "power": {
    "battery_mv": 3850,
    "solar_mv": 4200,
    "charging": true
  }
}
```

## Building and Flashing

### Side A (PlatformIO)

```bash
cd firmware/side_a
pio run -e mycobrain-side-a
pio run -t upload -e mycobrain-side-a
```

### Side B (PlatformIO)

```bash
cd firmware/side_b

# Default (LoRa only)
pio run -e mycobrain-side-b

# WiFi variant
pio run -e mycobrain-side-b-wifi

# BLE variant  
pio run -e mycobrain-side-b-ble

# Full comms
pio run -e mycobrain-side-b-full
```

### Single-ESP32 Arduino Variant

For development with a single ESP32, use the Arduino IDE with:

```
firmware/MycoBrain_SideA/MycoBrain_SideA_Production.ino
```

## Device Configuration Workflow

### 1. Factory Default

Devices ship with `device_role: "standalone"` and empty `device_display_name`.

### 2. Configuration via MycoBrain Service

The PC-connected MycoBrain service can configure devices:

```bash
# Set environment variables
export MYCOBRAIN_DEVICE_ROLE="mushroom1"
export MYCOBRAIN_DEVICE_DISPLAY_NAME="Mushroom 1 - Field Station Alpha"

python services/mycobrain/mycobrain_service_standalone.py
```

### 3. Over-the-Air Configuration

Send MDP commands to configure identity:

```python
# Via MAS API
POST /api/devices/{device_id}/command
{
  "command": "set_device_role",
  "params": {"role": "mushroom1"}
}
```

### 4. Registration with MAS

The MycoBrain service sends heartbeats with device identity:

```json
POST /api/devices/register
{
  "device_id": "MYCOBRAIN-ABC123",
  "device_name": "Local MycoBrain",
  "device_role": "mushroom1",
  "device_display_name": "Mushroom 1 - Station Alpha",
  "host": "192.168.0.100",
  "port": 8003,
  "ingestion_source": "serial"
}
```

## Future Enhancements

1. **Acoustic Modem Support** - Underwater/through-substrate communication
2. **Optical Modem Support** - Line-of-sight high-bandwidth
3. **Mesh Networking** - Device-to-device relay
4. **OTA Firmware Updates** - Remote firmware upgrade via WiFi/LoRa
5. **Deep Sleep Modes** - Extended battery life for field deployments

## Related Documentation

- `docs/MDP_PROTOCOL_SPEC.md` - Full MDP protocol specification
- `docs/MYCOBRAIN_SERVICE_FEB10_2026.md` - PC service documentation
- `docs/DEVICE_REGISTRY_API_FEB10_2026.md` - MAS device registry API
