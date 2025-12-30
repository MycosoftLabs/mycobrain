# MycoBrain Side-B Production Firmware

Production-ready firmware for the MycoBrain Side-B (Router MCU) ESP32-S3 board.

## Features

- **UART Routing**: Bidirectional communication with Side-A
- **Command Forwarding**: Routes commands from PC to Side-A
- **Telemetry Forwarding**: Routes telemetry from Side-A to PC
- **Connection Monitoring**: Tracks Side-A connection status
- **Status LED**: Visual indication of connection state
- **Heartbeat**: Automatic connection health monitoring

## Hardware Configuration

- **UART to Side-A**: RX=GPIO16, TX=GPIO17
- **Status LED**: GPIO2
- **Serial (PC)**: USB CDC

## Building

### PlatformIO

```bash
cd firmware/MycoBrain_SideB
pio run
pio run -t upload
pio device monitor
```

### Arduino IDE

1. Open `MycoBrain_SideB.ino`
2. Select board: **ESP32S3 Dev Module**
3. Configure settings (see ARDUINO_IDE_SETTINGS.md)
4. Upload

## Commands

Side-B specific commands:

```json
{"cmd":"ping"}
{"cmd":"status"}
{"cmd":"get_mac"}
{"cmd":"get_version"}
{"cmd":"side_a_status"}
```

All other commands are forwarded to Side-A.

## Status LED

- **3 blinks on startup**: Initialization complete
- **Solid ON**: Side-A connected
- **OFF**: Side-A disconnected

## Integration

This firmware works with:
- `services/mycobrain/mycobrain_dual_service.py`
- JSON command/telemetry format
- Automatic command/telemetry routing

## Version

- **Firmware Version**: 1.0.0-production
- **Protocol**: JSON mode
- **Status**: Production Ready

## License

See main project license.

