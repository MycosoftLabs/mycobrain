# MycoBrain Side-A Production Firmware

Production-ready firmware for the MycoBrain Side-A (Sensor MCU) ESP32-S3 board.

## Features

- **Sensors**: BME688 environmental sensors (I2C)
- **I2C Scanning**: Automatic detection of I2C devices
- **Analog Inputs**: 4 analog inputs (AI1-AI4)
- **MOSFET Control**: 3 digital MOSFET outputs (GPIO12/13/14)
- **NeoPixel LED**: Onboard SK6805 NeoPixel on GPIO15
- **Buzzer**: Piezo buzzer control on GPIO16
- **Telemetry**: Automatic telemetry transmission (JSON format)
- **Commands**: Full command interface via Serial
- **Brownout Protection**: Disabled for stable operation

## Hardware Configuration

- **I2C**: SDA=GPIO5, SCL=GPIO4
- **Analog Inputs**: AI1=GPIO6, AI2=GPIO7, AI3=GPIO10, AI4=GPIO11
- **MOSFETs**: GPIO12, GPIO13, GPIO14
- **NeoPixel**: GPIO15 (SK6805)
- **Buzzer**: GPIO16

**⚠️ CRITICAL**: Previous documentation incorrectly listed analog pins as GPIO34/35/36/39 (classic ESP32 pins). These are **WRONG** for ESP32-S3 and will cause firmware to read incorrect pins or fail.

## Building

### PlatformIO

```bash
cd firmware/MycoBrain_SideA
pio run
pio run -t upload
pio device monitor
```

### Arduino IDE

1. Open `MycoBrain_SideA_Production.ino`
2. Select board: **ESP32S3 Dev Module**
3. Configure settings (see ARDUINO_IDE_SETTINGS.md)
4. Upload

## Commands

All commands are JSON format sent via Serial:

```json
{"cmd":"ping"}
{"cmd":"status"}
{"cmd":"get_mac"}
{"cmd":"get_version"}
{"cmd":"i2c_scan"}
{"cmd":"set_telemetry_interval","interval_seconds":5}
{"cmd":"set_mosfet","mosfet_index":0,"state":true}
{"cmd":"read_sensor","sensor_id":0}
{"cmd":"buzzer","frequency":1000,"duration":500}
{"cmd":"reset"}
```

## Telemetry Format

Telemetry is automatically sent at the configured interval:

```json
{
  "type": "telemetry",
  "data": {
    "ai1_voltage": 3.3,
    "ai2_voltage": 2.5,
    "ai3_voltage": 1.8,
    "ai4_voltage": 0.0,
    "temperature": 25.5,
    "humidity": 60.0,
    "pressure": 1013.25,
    "gas_resistance": 50000,
    "mosfet_states": [true, false, false],
    "i2c_addresses": [118, 119],
    "firmware_version": "1.0.0",
    "uptime_seconds": 3600
  }
}
```

## Integration

This firmware is compatible with:
- `services/mycobrain/mycobrain_dual_service.py`
- JSON command/telemetry format
- Future MDP v1 protocol support (planned)

## Version

- **Firmware Version**: 1.0.0
- **Protocol**: JSON mode
- **Status**: Production Ready

## License

See main project license.

