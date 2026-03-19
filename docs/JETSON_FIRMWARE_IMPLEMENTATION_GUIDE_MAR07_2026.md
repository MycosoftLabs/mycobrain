# Jetson and Firmware Implementation Guide

**Date:** March 7, 2026  
**Status:** Active  
**Related:** `MYCOBRAIN_FIRMWARE_ROADMAP_MAR07_2026.md` (MAS), `DEVICE_JETSON16_CORTEX_ARCHITECTURE_MAR07_2026.md`, `GATEWAY_JETSON4_LILYGO_ARCHITECTURE_MAR07_2026.md`

---

## What Was Implemented

### Firmware (new MDP-first targets) — mycobrain repo

- `firmware/MycoBrain_SideA_MDP/`
  - MDP framing (COBS + CRC16)
  - Side A command family (`read_sensors`, `stream_sensors`, `output_control`, `estop`, `clear_estop`, `health`)
  - HELLO + capability/sensor advertisement
- `firmware/MycoBrain_SideB_MDP/`
  - MDP dual-UART relay topology (Jetson16 ↔ SideB ↔ SideA)
  - Transport directives (`lora_send`, `ble_advertise`, `wifi_connect`, `sim_send`, `transport_status`)
  - Link-status events and ACK/NACK handling

### On-device Jetson (16GB) — MAS repo

- `mycosoft_mas/edge/ondevice_operator.py`
  - Side A and Side B command arbitration
  - Audit logging (`data/edge/ondevice_audit.jsonl`)
  - Proposal/approval/apply flow for firmware/code/config mutation
  - OpenClaw task integration endpoint
- `mycosoft_mas/edge/mdp_protocol.py` and `mycosoft_mas/edge/mdp_serial_bridge.py`
  - Shared MDP Python implementation and serial bridge
- Launcher: `scripts/run_ondevice_operator.py` (MAS repo)

### Gateway Jetson (4GB + LilyGO) — MAS repo

- `mycosoft_mas/edge/gateway_router.py`
  - Ingest + store-and-forward queue
  - Upstream publish to MAS heartbeat, Mycorrhizae, MINDEX, website ingest
  - Gateway self-registration
  - OpenClaw task integration endpoint (gateway profile)
  - Audit log (`data/edge/gateway_audit.jsonl`)
- Launcher: `scripts/run_gateway_router.py` (MAS repo)

### Deployment and Execution Tooling

- **Canonical location:** MAS repo `deploy/jetson/`
- **Reference copy:** mycobrain repo `deploy/jetson/` (see `deploy/jetson/README.md`)
- Systemd units:
  - `mycobrain-ondevice-operator.service`
  - `mycobrain-gateway-router.service`
- Env templates:
  - `ondevice-operator.env.example`
  - `gateway-router.env.example`
- Service installer: `install_jetson_services.sh`
- Firmware flash helper (mycobrain): `scripts/flash-mycobrain-production.ps1` or MAS `scripts/flash_mycobrain_mdp.ps1`

---

## Side A Firmware Build and Flash

1. Connect Side A ESP32-S3 to USB.
2. Build:

```powershell
cd "C:\Users\admin2\Desktop\MYCOSOFT\CODE\mycobrain\firmware\MycoBrain_SideA_MDP"
pio run
```

3. Upload (replace COM port):

```powershell
pio run -t upload --upload-port COM7
```

4. Verify serial output:

```powershell
pio device monitor -b 115200 --port COM7
```

Expected: HELLO frame emission and periodic telemetry.

---

## Side B Firmware Build and Flash

1. Connect Side B ESP32-S3 to USB.
2. Build:

```powershell
cd "C:\Users\admin2\Desktop\MYCOSOFT\CODE\mycobrain\firmware\MycoBrain_SideB_MDP"
pio run
```

3. Upload:

```powershell
pio run -t upload --upload-port COM8
```

4. Verify:

```powershell
pio device monitor -b 115200 --port COM8
```

Expected: HELLO frame and status LED tied to Jetson heartbeat activity.

---

## On-device Jetson (16GB) Setup

### Environment

Set on the 16GB Jetson:

```bash
export ONDEVICE_SIDE_A_PORT=/dev/ttyTHS1
export ONDEVICE_SIDE_B_PORT=/dev/ttyTHS2
export ONDEVICE_MDP_BAUD=115200
export ONDEVICE_AUDIT_LOG=/opt/mycosoft/logs/ondevice_audit.jsonl
export OPENCLAW_BASE_URL=http://127.0.0.1:8000
export OPENCLAW_API_KEY=<your_openclaw_key_if_required>
```

### Run service

```bash
cd /opt/mycosoft/mas
python scripts/run_ondevice_operator.py --host 0.0.0.0 --port 8110
```

### Health check

```bash
curl -s http://127.0.0.1:8110/health
```

### Install as systemd service

```bash
cd /opt/mycosoft/mas
sudo bash deploy/jetson/install_jetson_services.sh ondevice
sudo cp deploy/jetson/ondevice-operator.env.example /etc/mycosoft/ondevice-operator.env
sudo nano /etc/mycosoft/ondevice-operator.env
sudo systemctl restart mycobrain-ondevice-operator
sudo systemctl status mycobrain-ondevice-operator --no-pager
```

---

## Gateway Jetson (4GB + LilyGO) Setup

### Environment

Set on the 4GB gateway Jetson:

```bash
export GATEWAY_ID=site-gateway-01
export GATEWAY_HOST=192.168.0.123
export GATEWAY_PORT=8120
export GATEWAY_LOCATION=server-room
export MAS_API_URL=http://192.168.0.188:8001
export MYCORRHIZAE_API_URL=http://192.168.0.187:8002
export MINDEX_API_URL=http://192.168.0.189:8000
export TELEMETRY_INGEST_URL=http://192.168.0.187:3000
export GATEWAY_OPENCLAW_BASE_URL=http://127.0.0.1:8000
export GATEWAY_OPENCLAW_API_KEY=<your_openclaw_key_if_required>
```

### Run service

```bash
cd /opt/mycosoft/mas
python scripts/run_gateway_router.py --host 0.0.0.0 --port 8120
```

### Install as systemd service

```bash
cd /opt/mycosoft/mas
sudo bash deploy/jetson/install_jetson_services.sh gateway
sudo cp deploy/jetson/gateway-router.env.example /etc/mycosoft/gateway-router.env
sudo nano /etc/mycosoft/gateway-router.env
sudo systemctl restart mycobrain-gateway-router
sudo systemctl status mycobrain-gateway-router --no-pager
```

---

## End-to-End Command Checks

### Side A via on-device operator

```bash
curl -s -X POST http://127.0.0.1:8110/side-a/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"read_sensors","params":{},"ack_requested":true}'
```

### Side B transport status via on-device operator

```bash
curl -s -X POST http://127.0.0.1:8110/side-b/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"transport_status","params":{},"ack_requested":true}'
```

---

## Notes

- **Firmware:** mycobrain repo owns ESP32 firmware (`MycoBrain_SideA_MDP`, `MycoBrain_SideB_MDP`).
- **Edge Python:** MAS repo owns `ondevice_operator`, `gateway_router`, `mdp_protocol`, deploy scripts.
- Side B transport directives are implemented as control surfaces; integrate hardware-specific LoRa/SIM drivers for physical radio transmission.
- Identity invariants remain enforced: canonical device identity remains Side A/MycoBrain identity.
