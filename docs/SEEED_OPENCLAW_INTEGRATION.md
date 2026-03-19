# Seeed OpenClaw + SenseCraft Integration Guide

## Overview

This guide covers integrating MycoBrain with the Seeed Studio OpenClaw ecosystem:

1. **Nemo Claw** — Servo gripper controller on Side A (PCA9685 or direct GPIO)
2. **OpenClaw Gateway** — AI agent on Jetson for voice/chat/messaging control
3. **reSpeaker-claw** — Voice interface via ReSpeaker XVF3800
4. **SenseCraft** — MQTT telemetry to Seeed Data Platform
5. **SenseCAP M2** — LoRaWAN gateway for direct cloud ingest

> **Important**: All existing MycoBrain operations (sensor ingestion, Jetson on-device operator, MDP protocol, envelope creation, Side A ↔ Side B ↔ Jetson communication) remain unchanged. The OpenClaw integration is additive — it runs alongside the existing stack.

---

## Architecture

```
                        SenseCraft Cloud
                        (MQTT Dashboard)
                              │
        ┌─────────────────────┼─────────────────────┐
        │           OpenClaw Gateway (Jetson)         │
        │              ws://127.0.0.1:18789           │
        │                                             │
        │  mycobrain-control skill                    │
        │  sensecraft-publish skill                   │
        │  Channels: Telegram, Discord, WebChat, Voice│
        └──────────────┬─────────────────────────────┘
                       │ /dev/ttyTHS1 (115200 baud)
                       │
                       │ JSON: {"action":"claw_grip"}\n
                       │ MDP:  [COBS+CRC16] 0x00
                       │
        ┌──────────────▼─────────────────────────────┐
        │  Side B ESP32-S3 (Comms MCU)                │
        │  - JSON↔MDP bridge (new)                    │
        │  - MDP passthrough (existing)               │
        │  - LoRa/WiFi/BLE transport (existing)       │
        └──────────────┬─────────────────────────────┘
                       │ UART2 (GPIO 8/9)
        ┌──────────────▼─────────────────────────────┐
        │  Side A ESP32-S3 (Sensor MCU)               │
        │  - BME688 dual sensors (existing)           │
        │  - Nemo claw servo controller (new)         │
        │  - MOSFET outputs + PWM (existing)          │
        │  - Analog inputs (existing)                 │
        └────────────────────────────────────────────┘
```

---

## 1. Nemo Claw Hardware Setup

### Option A: PCA9685 I2C Servo Driver (Recommended)

Connect a PCA9685 board to the I2C bus:
- **SDA**: GPIO 5
- **SCL**: GPIO 4
- **Address**: 0x40 (default)
- **Channel 0**: Servo signal wire

The firmware auto-detects PCA9685 at boot via I2C scan.

### Option B: Direct GPIO Servo

Connect servo signal wire to **GPIO 13** (MOSFET3 pin, repurposed).
Force feedback sensor (optional) on **GPIO 11** (AI4).

### CLI Commands

```
claw grip          # Close gripper
claw release       # Open gripper
claw pos 90        # Set angle (0-180)
claw status        # Show position, mode, force
```

### MDP Commands

| Command | ID | Params |
|---------|-----|--------|
| `claw_grip` | 0x0030 | — |
| `claw_release` | 0x0031 | — |
| `claw_position` | 0x0032 | `{"angle": 0-180}` |
| `claw_status` | 0x0033 | — |
| `drone_latch_payload` | 0x0026 | Alias for `claw_grip` |
| `drone_release_payload` | 0x0027 | Alias for `claw_release` |

---

## 2. OpenClaw JSON Bridge (Side B)

Side B now accepts **newline-delimited JSON** on the Jetson UART alongside existing MDP binary frames.

### Protocol

**Send** (Jetson → Side B):
```json
{"tool":"mycobrain","action":"claw_grip","params":{}}\n
{"tool":"mycobrain","action":"read_sensors","params":{}}\n
{"cmd":"lora_send","params":{"payload":"hello"}}\n
```

**Receive** (Side B → Jetson):
```json
{"status":"forwarded","cmd":"claw_grip"}\n
```

MDP binary frames (telemetry, ACKs from Side A) continue to flow as before.

The bridge detects JSON by checking if the line starts with `{` and ends with `}`. MDP binary frames use `0x00` as delimiter and are unaffected.

### Backwards Compatibility

- Existing MDP binary communication is **completely unchanged**
- The on-device operator, MAS heartbeat, Mycorrhizae publish, MINDEX persist all work as before
- JSON bridge is additive — only activates when a `\n`-terminated JSON line arrives

---

## 3. OpenClaw Gateway (Jetson)

### Installation

```bash
cd /path/to/mycobrain
sudo bash deploy/jetson/openclaw/install_openclaw.sh
```

### Configuration

Edit `/etc/mycosoft/openclaw.env`:
```bash
ANTHROPIC_API_KEY=sk-ant-...
MYCOBRAIN_SERIAL_PORT=/dev/ttyTHS1

# Optional: SenseCraft
SENSECRAFT_ORG_ID=your-org-id
SENSECRAFT_API_KEY=your-api-key
SENSECRAFT_DEVICE_EUI=your-device-eui

# Optional: Messaging
TELEGRAM_BOT_TOKEN=...
DISCORD_BOT_TOKEN=...
```

### Service Management

```bash
sudo systemctl start mycobrain-openclaw
sudo systemctl status mycobrain-openclaw
journalctl -u mycobrain-openclaw -f
```

### Available Tools

| Tool | Description |
|------|-------------|
| `sensor_read` | Read BME688 data |
| `sensor_stream` | Set streaming rate |
| `claw_grip` | Close gripper |
| `claw_release` | Open gripper |
| `claw_position` | Set angle |
| `claw_status` | Get claw state |
| `output_control` | MOSFET outputs |
| `led_set` | NeoPixel color |
| `buzzer` | Play tone |
| `device_status` | Health check |
| `lora_send` | LoRa message |
| `drone_mission` | Drone deploy/retrieve/data_mule |

---

## 4. reSpeaker Voice Interface

### Hardware

- ReSpeaker XVF3800 USB 4-Mic Array with XIAO ESP32S3
- Connect to Jetson via USB

### Firmware Setup

1. Clone reSpeaker-claw: `git clone https://github.com/Seeed-Projects/reSpeaker-claw`
2. Copy template: `cp openclaw/respeaker/mimi_secrets.h.template main/mimi_secrets.h`
3. Edit `main/mimi_secrets.h` with your API keys
4. Build and flash:
   ```bash
   idf.py set-target esp32s3
   idf.py build
   idf.py -p /dev/ttyUSBx flash monitor
   ```

### Voice Commands (Examples)

- "Read the sensors" → calls `sensor_read`
- "What's the temperature?" → calls `sensor_read`, returns temperature
- "Close the claw" → calls `claw_grip`
- "Release the payload" → calls `claw_release`
- "Set the LED to red" → calls `led_set` with r=255
- "What's the device status?" → calls `device_status`

---

## 5. SenseCraft Data Platform

### Setup

1. Create account at [sensecap.seeed.cc](https://sensecap.seeed.cc/portal/)
2. Register your MycoBrain as a device
3. Get Organization ID and API Key
4. Set environment variables (see Gateway Configuration above)

### Dashboard

Once configured, telemetry appears at:
- **Web**: sensecap.seeed.cc → Dashboard
- **Mobile**: SenseCraft App (iOS/Android)

### Data Channels

| Channel | Measurement | Unit |
|---------|-------------|------|
| amb_temperature | Ambient temp | °C |
| amb_humidity | Ambient RH | % |
| amb_pressure | Barometric | hPa |
| amb_iaq | IAQ index | — |
| amb_co2 | CO2 equivalent | ppm |
| amb_voc | VOC equivalent | ppm |
| env_temperature | Environment temp | °C |
| claw_position | Servo angle | deg |
| claw_closed | Gripper state | 0/1 |

---

## 6. SenseCAP M2 LoRaWAN Gateway (Optional)

MycoBrain's SX1262 LoRa at 915 MHz can send packets receivable by a SenseCAP M2 Multi-Platform Gateway.

### Setup

1. Configure SenseCAP M2 as Local Network Server (ChirpStack)
2. Set MQTT broker to point to your ChirpStack instance
3. MycoBrain Side B LoRa packets arrive via standard LoRaWAN uplink
4. No Jetson required for this path — direct LoRa → cloud

See: [SenseCAP M2 LNS Configuration](https://wiki.seeedstudio.com/Network/SenseCAP_Network/SenseCAP_M2_Multi_Platform/SenseCAP_M2_MP_Gateway_LNS_Configuration/)

---

## 7. Seeed AI Skills for Development

Install Seeed's Claude Code skills for hardware development:

```bash
claude skills install git+https://github.com/Seeed-Studio/ai-skills#subdirectory=ee-datasheet-master
claude skills install git+https://github.com/Seeed-Studio/ai-skills#subdirectory=schematic-analyzer
```

These provide:
- **ee-datasheet-master**: Extract specs from component datasheets with citations
- **schematic-analyzer**: Analyze KiCad schematics with structured JSON output

---

## Non-Interference Guarantee

This integration is designed to be **fully additive** and **non-breaking**:

| Existing Function | Impact |
|-------------------|--------|
| Side A sensor ingestion (BME688 BSEC2) | **No change** — claw init runs after sensor init |
| Side A → Side B MDP telemetry | **No change** — telemetry frames unchanged except optional `claw` field |
| Side B → Jetson MDP forwarding | **No change** — binary 0x00-delimited frames forwarded identically |
| Jetson on-device operator | **No change** — OpenClaw runs as separate systemd service |
| MAS heartbeat | **No change** |
| Mycorrhizae envelope publish | **No change** |
| MINDEX FCI persistence | **No change** |
| LoRa uplink | **No change** |
| E-stop | **Enhanced** — claw stops on e-stop; OpenClaw safety config can trigger e-stop on disconnect |

The JSON bridge on Side B uses `\n` as delimiter (newline), while MDP uses `0x00`. These are completely orthogonal — no collision possible.

---

## References

- [claw.seeed.cc](https://claw.seeed.cc/) — Seeed OpenClaw Hardware
- [reSpeaker-claw](https://github.com/Seeed-Projects/reSpeaker-claw) — Voice AI agent firmware
- [OpenClaw](https://github.com/openclaw/openclaw) — AI agent framework
- [SO-Arm + Jetson Thor](https://wiki.seeedstudio.com/ai_robotics_control_soarm_by_openclaw_on_jetson_thor/) — Robotic arm control
- [DuckyClaw](https://github.com/tuya/DuckyClaw) — Hardware-oriented edge agent
- [SenseCraft API](https://wiki.seeedstudio.com/sensecraft-data-platform/sensecraft-data-platform-api/sensecraft-data-platform-api/) — IoT platform API
- [Seeed AI Skills](https://github.com/Seeed-Studio/ai-skills) — Claude/Codex skills
