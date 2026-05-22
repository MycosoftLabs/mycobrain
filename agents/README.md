# mycobrain-agent

Unified Python host agent for every MycoBrain compute companion — Jetson (Orin or legacy), Raspberry Pi, or standalone PC over USB.

One codebase. One HTTP API on **port 8787**. Selects the right hardware adapter at startup.

```
mycobrain-agent
├── core/        ← MDP codec, serial bridge, MQTT client, registry, heartbeat
├── adapters/    ← jetson_orin · jetson_legacy · raspberry_pi · standalone
├── openclaw/    ← Proxy + audit for OpenClaw at 127.0.0.1:8000
├── http/        ← FastAPI server on :8787 (see docs/PORT_8787_HTTP_API_SPEC_*)
└── upstream/    ← MAS heartbeat, MINDEX FCI, NLM translate, Mycorrhizae MMP
```

## Why this exists

The mycobrain repo owned the ESP32-S3 firmware (Side A / Side B / MDP). The MAS repo owned the Jetson Python (`ondevice_operator.py`, `gateway_router.py`). The Raspberry Pi variant had no canonical code. The standalone PC variant had legacy JSON code that pre-dated MDP. **This package unifies all four host runtimes** so the website can manage every device through one API.

See [`../docs/PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md`](../docs/PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md) for the master plan.

## Install

### Jetson (Orin or legacy)

```bash
sudo bash /path/to/mycobrain/scripts/agent-install-jetson.sh \
    --adapter jetson_orin \
    --side-a-port /dev/ttyUSB0 \
    --side-b-port /dev/ttyUSB1
```

For the older Jetson use `--adapter jetson_legacy`.

### Raspberry Pi

```bash
sudo bash /path/to/mycobrain/scripts/agent-install-pi.sh \
    --side-a-port /dev/serial0
```

### Standalone PC (Windows)

```powershell
.\scripts\agent-install-standalone.ps1 -Port COM7
```

All installers drop a config under `/etc/mycobrain/agent.env` (Linux) or `%PROGRAMDATA%\mycobrain\agent.env` (Windows). Edit, then `systemctl restart mycobrain-agent` (Linux) or restart the Windows service.

## Run from source (dev)

```bash
cd agents
python -m venv .venv && source .venv/bin/activate
pip install -e .[dev]
export MYCOBRAIN_ADAPTER=jetson_orin
export MYCOBRAIN_SIDE_A_PORT=/dev/ttyUSB0
export MYCOBRAIN_HTTP_PORT=8787
python -m mycobrain_agent
```

## Verify

```bash
curl -s http://localhost:8787/status | jq
curl -s http://localhost:8787/info | jq
```

See [`../docs/PORT_8787_HTTP_API_SPEC_MAY19_2026.md`](../docs/PORT_8787_HTTP_API_SPEC_MAY19_2026.md) for the full API.

## Tests

```bash
pytest -q
```

The MDP codec has property tests against the C reference. Adapter tests use `pyserial` loopback fixtures.
