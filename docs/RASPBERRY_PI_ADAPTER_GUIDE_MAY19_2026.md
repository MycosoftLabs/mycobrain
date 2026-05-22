# Raspberry Pi Adapter — Bring-up guide

**Date:** 2026-05-19
**Companion:** [`PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md`](PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md), [`AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md`](AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md) (device #2)

The Raspberry Pi runs the same `mycobrain-agent` as the Jetsons — only the adapter differs. This guide is the short version of "turn a fresh Pi into device #2 in your fleet."

## Tested on

- Pi 4 Model B (4GB and 8GB)
- Pi 5 (8GB)
- Raspberry Pi OS Bookworm 64-bit

## Hardware notes

- **Side A**: connect via USB-UART bridge to `/dev/ttyUSB0`, OR straight onto the Pi's UART pins (GPIO14 TXD, GPIO15 RXD → `/dev/serial0`)
- **Side B**: optional. Most Pi-hosted MycoBrains are single-MCU. The agent's `Side B` features are simply unavailable; the rest of the agent works fine.
- **OpenClaw**: runs as a separate service (containerized or systemd) on the same Pi, binding `127.0.0.1:8000`. Same convention as Jetson.

## Bring-up

```bash
# 1. Update + python
sudo apt update
sudo apt install -y python3 python3-venv python3-pip git curl jq

# 2. Clone the mycobrain repo
sudo mkdir -p /opt && cd /opt
sudo git clone <your-fork-or-mirror>/mycobrain.git mycobrain
cd mycobrain

# 3. One-shot installer (enables UART, drops env, installs systemd unit)
sudo bash scripts/agent-install-pi.sh

# 4. Edit env
sudo nano /etc/mycobrain/agent.env
# Set:
#   MYCOBRAIN_DEVICE_NICKNAME=pi-a
#   MYCOBRAIN_HOST_IP=192.168.0.123    (or whatever DHCP gives this Pi)
#   MYCOBRAIN_SIDE_A_PORT=/dev/serial0  (or /dev/ttyUSB0)
#   MYCOBRAIN_MQTT_PASSWORD=...
#   MYCOBRAIN_OPENCLAW_API_KEY=...

# 5. Start
sudo systemctl enable --now mycobrain-agent

# 6. Verify
curl -s http://localhost:8787/status | jq
curl -s http://localhost:8787/info   | jq
journalctl -u mycobrain-agent -f --no-pager
```

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `serial.SerialException: device reports readiness to read but returned no data` | UART grabbed by another process (probably `serial-getty`). The installer disables it; run `sudo systemctl disable --now serial-getty@ttyAMA0.service`. |
| Agent reports `side_a.linked: false` | Wrong port. Set `MYCOBRAIN_SIDE_A_PORT` to the actual node (`ls /dev/serial* /dev/ttyUSB* /dev/ttyACM*`). |
| `OpenClaw unreachable` in `/openclaw/status` | OpenClaw isn't running locally. `curl http://127.0.0.1:8000/healthz`. |
| Slow MQTT publish on Pi 4 | The Cloudflare WSS path tunnels through Pi → broker; for a same-LAN broker set `MYCOBRAIN_MQTT_URL=mqtt://<broker>:1883`. |
| Agent dies on first telemetry | Check `pip` finished installing `pyserial` and `paho-mqtt`. Re-run the venv installer. |

## Differences from Jetson

| Aspect | Jetson Orin | Raspberry Pi |
|--------|-------------|---------------|
| CUDA inference | yes | no (TAC-O stubs return `no_model`) |
| GPIO | not exposed by adapter | available; `output_control` with `target:"gpio"` works |
| Side B common topology | dual-MCU | single-MCU typical |
| Power budget | high (10–25W) | low (3–5W) |
| Suitable role | on-device cortex | gateway router OR small-site operator |

## How NatureOS sees it

`mycosoft.com/natureos/devices` shows the Pi exactly like the Jetson — same card, same panels — but the host badge says `raspberry_pi` and the OpenClaw panel only renders if `info.openclaw.available === true`.
