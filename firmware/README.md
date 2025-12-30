# MycoBrain Firmware

Arduino sketches for MycoBrain V1 dual-ESP32-S3 board.

## Directory Structure

- `MycoBrain_SideA/` - Sensor MCU firmware (Side-A)
- `MycoBrain_SideB/` - Router MCU firmware (Side-B)

## Side-A (Sensor MCU)

**Features:**
- BME688 environmental sensors (2x)
- I2C sensor scanning
- Analog inputs (AI1-AI4)
- MOSFET control (4x)
- NeoPixel LED control
- Buzzer control
- Telemetry transmission
- Command handling

**Hardware:**
- ESP32-S3
- BME688 sensors on I2C (0x76, 0x77)
- Analog inputs on pins 6, 7, 10, 11 (ESP32-S3 MycoBrain - VERIFIED)
- MOSFETs on pins 12, 13, 14
- NeoPixel on pin 15 (SK6805, single pixel)
- Buzzer on pin 16

**⚠️ CRITICAL**: Previous documentation incorrectly listed analog pins as 34, 35, 36, 39 (classic ESP32 pins). These are **WRONG** for ESP32-S3 and will cause firmware to read incorrect pins or fail.

## Side-B (Router MCU)

**Features:**
- UART communication with Side-A
- Command routing
- Telemetry forwarding
- LoRa support (future)
- Status monitoring

**Hardware:**
- ESP32-S3
- UART to Side-A (RX: 16, TX: 17)
- Status LED on pin 2

## Installation

### Prerequisites

1. **Arduino IDE** (1.8.19 or later) or **PlatformIO**
2. **ESP32 Board Support**
   - In Arduino IDE: File → Preferences → Additional Board Manager URLs
   - Add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install

3. **Required Libraries:**
   - **ArduinoJson** (v6.21.0 or later)
   - **Adafruit BME680 Library**
   - **FastLED** (for NeoPixel)
   - **WiFi** (built-in for MAC address)

### Installing Libraries

**Arduino IDE:**
1. Sketch → Include Library → Manage Libraries
2. Search and install:
   - `ArduinoJson` by Benoit Blanchon
   - `Adafruit BME680 Library` by Adafruit
   - `FastLED` by Daniel Garcia

**PlatformIO:**
Add to `platformio.ini`:
```ini
lib_deps = 
    bblanchon/ArduinoJson@^6.21.0
    adafruit/Adafruit BME680 Library
    fastled/FastLED@^3.5.0
```

## Upload Instructions

### Side-A

1. Open `MycoBrain_SideA/MycoBrain_SideA.ino` in Arduino IDE
2. Select board: **Tools → Board → ESP32 Arduino → ESP32S3 Dev Module**
3. Select port: Your Side-A USB port (e.g., COM4)
4. Configure:
   - **CPU Frequency:** 240MHz
   - **Flash Size:** 4MB (or your board's size)
   - **Partition Scheme:** Default
   - **Upload Speed:** 921600
5. Click **Upload**

### Side-B

1. Open `MycoBrain_SideB/MycoBrain_SideB.ino` in Arduino IDE
2. Select board: **Tools → Board → ESP32 Arduino → ESP32S3 Dev Module**
3. Select port: Your Side-B USB port (e.g., COM5)
4. Configure same as Side-A
5. Click **Upload**

## Configuration

### Pin Configuration

If your board uses different pins, modify these in the sketch:

**Side-A (ESP32-S3 MycoBrain - VERIFIED):**
```cpp
#define I2C_SDA 5
#define I2C_SCL 4
#define AI1_PIN 6      // ⚠️ NOT GPIO34 (classic ESP32)!
#define AI2_PIN 7
#define AI3_PIN 10
#define AI4_PIN 11
#define MOSFET_1_PIN 12
#define MOSFET_2_PIN 13
#define MOSFET_3_PIN 14
#define NEOPIXEL_PIN 15
#define BUZZER_PIN 16
```

**⚠️ CRITICAL**: Do NOT use GPIO34/35/36/39 for analog inputs on ESP32-S3. These are classic ESP32 pins and will cause incorrect readings or failures. The correct pins for ESP32-S3 MycoBrain are GPIO6, 7, 10, 11.

**Side-B:**
```cpp
#define UART_RX 16
#define UART_TX 17
#define LED_PIN 2
```

### BME688 Addresses

If your sensors use different I2C addresses:
```cpp
#define BME688_ADDR_1 0x76
#define BME688_ADDR_2 0x77
```

## Testing

### Serial Monitor

1. Open Serial Monitor (Tools → Serial Monitor)
2. Set baudrate to **115200**
3. Send commands:
   ```json
   {"cmd":"ping"}
   {"cmd":"status"}
   {"cmd":"i2c_scan"}
   ```

### Expected Behavior

**Side-A:**
- Blue LED on startup
- Green LED when ready
- Telemetry every 10 seconds
- Responds to commands

**Side-B:**
- LED blinks 3 times on startup
- LED on when Side-A connected
- Forwards commands to Side-A
- Forwards telemetry to PC

## Commands

### Side-A Commands

```json
{"cmd":"ping"}
{"cmd":"status"}
{"cmd":"get_mac"}
{"cmd":"get_version"}
{"cmd":"i2c_scan"}
{"cmd":"read_sensor","sensor_id":0}
{"cmd":"set_telemetry_interval","interval_seconds":5}
{"cmd":"set_mosfet","mosfet_index":0,"state":true}
{"cmd":"neopixel","r":255,"g":0,"b":0,"brightness":128}
{"cmd":"buzzer","frequency":1000,"duration":500}
{"cmd":"reset"}
```

### Side-B Commands

```json
{"cmd":"ping"}
{"cmd":"status"}
{"cmd":"get_mac"}
{"cmd":"get_version"}
{"cmd":"side_a_status"}
```

## Troubleshooting

### Side-A Not Responding

1. Check serial connection
2. Verify baudrate (115200)
3. Check I2C connections
4. Verify power supply

### No Telemetry

1. Send: `{"cmd":"set_telemetry_interval","interval_seconds":5}`
2. Wait 10 seconds
3. Check serial output

### BME688 Not Detected

1. Run: `{"cmd":"i2c_scan"}`
2. Check I2C addresses
3. Verify wiring (SDA, SCL, VCC, GND)
4. Check sensor power

### Side-B Not Forwarding

1. Check UART wiring (RX, TX)
2. Verify baudrate matches
3. Check Side-A connection
4. Monitor Side-B serial output

## Protocol

### Telemetry Format

```json
{
  "ai1_voltage": 3.3,
  "ai2_voltage": 2.5,
  "temperature": 25.5,
  "humidity": 60.0,
  "pressure": 1013.25,
  "gas_resistance": 50000,
  "mosfet_states": [true, false, false, false],
  "i2c_addresses": [118, 119],
  "firmware_version": "1.0.0",
  "uptime_seconds": 3600
}
```

### Response Format

```json
{
  "type": "status",
  "data": {
    "status": "ready",
    "mac": "AA:BB:CC:DD:EE:FF",
    "firmware": "1.0.0"
  }
}
```

## Integration with Service

This firmware is designed to work with:
- `services/mycobrain/mycobrain_dual_service.py`
- JSON command format (not MDP yet)
- Newline-delimited JSON telemetry

The service will:
1. Send commands as JSON: `{"cmd":"command_name",...}`
2. Receive telemetry as JSON lines
3. Parse responses as JSON

## Future Enhancements

- [ ] MDP v1 protocol support (COBS framing, CRC16)
- [ ] LoRa module integration
- [ ] OTA updates
- [ ] WiFi configuration
- [ ] Web interface
- [ ] Data logging to SD card

## License

See main project license.

