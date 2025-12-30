# MycoBrain Science Communications Firmware

Advanced ESP32-S3 firmware for the Mycosoft MycoBrain board with science communication capabilities.

## Features

### 🌈 NeoPixel LED Control (GPIO15)
- Onboard SK6805 addressable RGB LED
- RMT-based driver for ESP32-S3 compatibility (NeoPixelBus)
- Patterns: rainbow, pulse, sweep, beacon

### 🔊 Buzzer Control (GPIO16)
- MOSFET-driven buzzer with LEDC PWM
- Built-in sound patterns: coin, bump, power, 1up, morgio, alert, warning, success, error
- Configurable frequency and duration

### 📡 Optical Modem (LiFi TX)
- Transmit data via LED blinking for camera/light-sensor receivers
- Profiles: camera_ook, camera_manchester, beacon, morse
- CRC16 error detection
- Camera-friendly rates (5-20 Hz)

### 🎵 Acoustic Modem (FSK TX)
- Transmit data via buzzer tones for microphone receivers
- Simple FSK (2-tone) with preamble + CRC16
- Patterns: sweep, chirp, pulse_train
- ggwave-like "gibberlink" vibe

### 🧪 Stimulus Engine
- Separate from modem mode for repeatable experiments
- Light patterns: pulse, flash, ramp, strobe
- Sound patterns: tone, pulse, sweep, chirp
- Configurable cycles and timing

### 🔌 Peripheral Discovery
- I2C bus scanning and device identification
- Automatic peripheral type detection
- JSON descriptor reporting for dashboard widgets
- Hotplug monitoring

### 💬 JSON-CLI Protocol
- Human mode: banners, help text, readable output
- Machine mode: NDJSON only, no banners
- Supports both plaintext and JSON commands
- Real-time telemetry streaming

## Hardware Configuration

| Component | GPIO | Notes |
|-----------|------|-------|
| NeoPixel LED | 15 | SK6805 addressable RGB |
| Buzzer | 16 | MOSFET-driven |
| I2C SCL | 4 | With pullups |
| I2C SDA | 5 | With pullups |
| OUT_1 | 12 | MOSFET digital output |
| OUT_2 | 13 | MOSFET digital output |
| OUT_3 | 14 | MOSFET digital output |
| AIN_1 | 6 | Analog input |
| AIN_2 | 7 | Analog input |
| AIN_3 | 10 | Analog input |
| AIN_4 | 11 | Analog input |

## Building

### PlatformIO (Recommended)

```bash
cd firmware/MycoBrain_ScienceComms
pio run
pio run -t upload
```

### Arduino IDE

1. Install ESP32 board support (Espressif)
2. Install libraries:
   - NeoPixelBus by Makuna
   - ArduinoJson by Benoit Blanchon
3. Use these exact settings:
   - Board: ESP32-S3 Dev Module
   - CPU Frequency: 240 MHz
   - Flash Mode: QIO @ 80 MHz
   - Flash Size: 16 MB
   - Partition Scheme: 16MB Flash (3MB APP/9.9MB FATFS)
   - PSRAM: OPI PSRAM
   - USB CDC On Boot: Enabled
   - USB Mode: Hardware CDC and JTAG
   - Upload Speed: 921600

## Quick Start

```bash
# After flashing, connect via serial at 115200 baud

# In human mode (default)
help                          # Show all commands
led rgb 255 0 0               # Red LED
buzz pattern coin             # Play coin sound
status                        # Show system status

# Switch to machine mode (for dashboard)
mode machine                  # NDJSON output only
```

## License

(c) 2025 Mycosoft. All rights reserved.

