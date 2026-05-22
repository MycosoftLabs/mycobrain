# Deploy — `mycobrain-jet-b` (cold boot)

This device is currently dead. Bring it up from scratch.

1. Power on, identify the Jetson model:
   ```bash
   cat /etc/nv_tegra_release
   uname -m
   ```
2. Flash Side A + Side B firmware from another machine (PlatformIO + ESP32 USB drivers):
   ```powershell
   .\scripts\flash-mycobrain-production.ps1 -Board SideA -Role mushroom1 -Port COM7
   .\scripts\flash-mycobrain-production.ps1 -Board SideB -Port COM8
   ```
3. Reconnect the flashed boards to this Jetson via USB-UART.
4. On this Jetson, run:
   ```bash
   sudo bash /opt/mycobrain/scripts/bootstrap-dead-jetson.sh
   ```
   This installs the agent with `--adapter jetson_legacy`, configures serial port permissions, and enables the systemd service.
5. Configure env and pair (same flow as `mycobrain-jet-a`).
