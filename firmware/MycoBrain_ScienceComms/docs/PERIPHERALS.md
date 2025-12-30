# MycoBrain Peripheral Discovery System

## Overview

The MycoBrain firmware includes a peripheral discovery system that:

1. Scans the I2C bus for connected devices
2. Identifies known device types
3. Reports device descriptors in JSON format
4. Enables dashboard widgets to auto-configure

## Supported Devices

| Address | Device | Type | Capabilities |
|---------|--------|------|--------------|
| 0x76, 0x77 | BME688 | bme688 | Environmental sensor |
| 0x44, 0x45 | SHT40 | sht40 | Temp/Humidity |
| 0x23 | BH1750 | bh1750 | Light sensor |
| 0x59 | SGP40 | sgp40 | VOC sensor |
| 0x3C, 0x3D | SSD1306 | ssd1306 | OLED display |
| 0x48, 0x49 | ADS1115 | ads1115 | ADC |
| 0x20, 0x21 | MCP23017 | mcp23017 | GPIO expander |
| 0x40 | PCA9685 | pca9685 | PWM driver |
| 0x50, 0x51 | EEPROM | eeprom | ID EEPROM |

## Commands

### Scan I2C Bus

```bash
periph scan
```

Returns:
```json
{"type": "periph_scan", "found": 2}
```

### List All Peripherals

```bash
periph list
```

Returns:
```json
{
  "type": "periph_list",
  "count": 2,
  "devices": [
    {
      "type": "periph",
      "board_id": "AABBCCDDEEFF",
      "bus": "i2c0",
      "address": "0x76",
      "peripheral_uid": "i2c0-76",
      "peripheral_type": "bme688",
      "vendor": "Bosch",
      "product": "BME688",
      "revision": "1.0",
      "present": true,
      "last_seen": 12345
    },
    ...
  ]
}
```

### Describe Specific Peripheral

```bash
periph describe 0x76
```

Returns full descriptor JSON for the device.

### Enable Hotplug Monitoring

```bash
periph hotplug on
```

When enabled, the firmware periodically re-scans the I2C bus to detect newly connected or disconnected devices.

## Descriptor Schema

Each peripheral has a JSON descriptor:

```json
{
  "type": "periph",
  "board_id": "<esp32_mac>",
  "bus": "i2c0",
  "address": "0x76",
  "peripheral_uid": "i2c0-76",
  "peripheral_type": "bme688",
  "vendor": "Bosch",
  "product": "BME688",
  "revision": "1.0",
  "capabilities": ["telemetry", "control"],
  "data_plane": {
    "control": "i2c",
    "stream": "none"
  },
  "telemetry_schema": { ... },
  "command_schema": { ... },
  "ui_widget": "environment_sensor",
  "present": true,
  "last_seen": 12345
}
```

### Capability Flags

| Capability | Description |
|------------|-------------|
| telemetry | Device provides sensor readings |
| control | Device accepts commands |
| acoustic_rx | Device can receive audio |
| optical_rx | Device can receive light |
| optical_tx | Device can emit light |
| haptic | Device has vibration/motor |

## Declared Peripherals

For non-I2C devices (like NeoPixel arrays on GPIO pins), use declared peripherals:

```bash
# This is a planned feature
# periph declare pixel_array pin=18 count=60
```

This allows the dashboard to create widgets for GPIO-connected devices.

## Dashboard Integration

The peripheral discovery system is designed to work with the Mycosoft dashboard:

1. Dashboard calls `/api/mycobrain/peripherals`
2. Service queries board: `periph list`
3. Board returns peripheral descriptors
4. Dashboard auto-creates widgets based on `ui_widget` hint

### UI Widget Hints

| Widget | Description |
|--------|-------------|
| environment_sensor | BME688, SHT40 - Temp/Humidity/Pressure |
| light_sensor | BH1750 - Lux readings |
| voc_sensor | SGP40 - Air quality |
| pixel_array | NeoPixel strip controls |
| gpio_expander | Digital I/O controls |
| pwm_driver | Servo/LED dimmer controls |

## Adding New Device Types

To add support for a new I2C device:

1. Add to `knownDevices[]` in `peripherals.cpp`:
```cpp
{0xNN, PERIPH_NEW_TYPE, "Vendor", "Product"},
```

2. Add type enum in `peripherals.h`:
```cpp
PERIPH_NEW_TYPE,
```

3. Add type name in `peripheralTypeName()`:
```cpp
case PERIPH_NEW_TYPE: return "new_type";
```

4. Implement any device-specific driver in a new module.

