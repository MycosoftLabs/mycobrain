# Standalone Serial Agent — Bench MycoBrain on a PC

**Date:** 2026-05-19
**Companion:** [`PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md`](PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md), [`AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md`](AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md) (device #4)

For the MycoBrain plugged directly into a developer's PC over USB — no Jetson, no Pi, no Side B. Today this device runs the **legacy `firmware/MycoBrain_SideA/`** build (JSON-over-serial, pre-MDP) and talks to the website via WebSocket. We're going to put the unified agent in front of it so NatureOS sees it like every other MycoBrain.

## What the agent does for this device

- Opens the USB-CDC serial port
- Auto-detects whether the firmware speaks MDP (binary COBS frames) or legacy JSON lines
- For legacy JSON: translates each line into a synthetic MDP TELEMETRY frame and runs it through the normal pipeline → NatureOS sees the same telemetry shape as everywhere else
- Exposes `:8787` on this PC so NatureOS can poll/stream
- Publishes to MQTT (`mycosoft/devices/{id}/...`) so MQTT-status shows the bench device
- Reports `openclaw.available: false` (no claw on the bench)
- **Does not break the existing site WebSocket** — both paths run side-by-side

## Install (Windows, the user's case)

```powershell
# From the mycobrain repo root, in an elevated PowerShell
# Find the COM port first
Get-PnpDevice -PresentOnly | Where-Object { $_.Class -eq 'Ports' } | Format-Table -AutoSize

# Then install
.\scripts\agent-install-standalone.ps1 -Port COM7 -Nickname bench
```

Prereqs:
- Python 3.10+ on PATH (`python --version`)
- `nssm` on PATH (for the Windows service wrapper). Install: `winget install nssm` or `choco install nssm`

The installer writes `%PROGRAMDATA%\mycobrain\agent.env`. Edit before/after as needed:

```bash
MYCOBRAIN_ADAPTER=standalone
MYCOBRAIN_DEVICE_NICKNAME=bench
MYCOBRAIN_SIDE_A_PORT=COM7
MYCOBRAIN_HTTP_PORT=8787
MYCOBRAIN_AUTH_MODE=pair_token       # use a token for the bench; jwt is overkill
MYCOBRAIN_PAIR_TOKEN=<random>
MYCOBRAIN_MQTT_URL=wss://mqtt.mycosoft.com
MYCOBRAIN_MQTT_USERNAME=mycobrain
MYCOBRAIN_MQTT_PASSWORD=<password>
MYCOBRAIN_OPENCLAW_ENABLED=false
```

Start (auto-start is already configured):

```powershell
Start-Service mycobrain-agent
Get-Service mycobrain-agent
curl http://localhost:8787/status
```

## Install (Linux / macOS, equivalent)

```bash
pip install -e agents/
export MYCOBRAIN_ADAPTER=standalone
export MYCOBRAIN_SIDE_A_PORT=/dev/ttyACM0   # or /dev/tty.usbmodem...
export MYCOBRAIN_AUTH_MODE=pair_token
export MYCOBRAIN_PAIR_TOKEN=$(openssl rand -hex 16)
python -m mycobrain_agent
```

For unattended Linux, drop the `mycobrain-agent.service` file and the env file like the Jetson install does.

## What you see in NatureOS

A new card on `mycosoft.com/natureos/devices`:

```
● mycobrain-bench    standalone    (USB / COM7)    legacy mode
   side-a-v1-json (legacy)         OpenClaw: —
```

Detail page:
- Live tab: legacy JSON readings appear under "synthetic telemetry" with the original doc preserved under `sensors.legacy = true`
- Console: Side A `output_control` is the only useful command on legacy firmware (LED, buzzer) — Side B tab is hidden
- Frames tab: shows the synthetic MDP frames the agent emits
- OpenClaw tab: hidden

## Legacy WebSocket coexistence

The agent **does not intercept** the existing `ws://mycosoft.com:8765/ws/{device_id}` connection that the legacy site buttons rely on. The MycoBrain firmware happily writes JSON to its single USB-CDC stream; the OS lets one process read those bytes. There are two strategies depending on whether the site's WS-bridge tool runs on this same PC:

1. **Site WS-bridge runs on this PC** — pick one to own the COM port. Either keep WS-bridge (legacy buttons keep working as-is; agent off) OR run the agent (NatureOS works; legacy buttons stop until the website is updated to call `:8787` instead).
2. **Site WS-bridge runs elsewhere** — no conflict. Run the agent.

**Recommended for the bench during transition:** run the agent. The legacy buttons will be re-pointed to the agent in Phase 4 of the master plan.

## Verifying end-to-end

```bash
# Agent up?
curl -s http://localhost:8787/status | jq

# See telemetry coming in?
curl -s -N http://localhost:8787/mdp/frames?limit=10 | jq

# Send an LED command (LED 1 ON)
curl -s -X POST http://localhost:8787/command \
  -H "X-Pair-Token: $MYCOBRAIN_PAIR_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"target":"side_a","cmd":"output_control","params":{"id":"led1","value":1}}'

# Watch MQTT
# (from any machine with mosquitto_sub)
mosquitto_sub -h mqtt.mycosoft.com -p 443 --ws --tls -u mycobrain -P <pw> -t 'mycosoft/devices/+/presence' -v
```

## Future: upgrade the bench to MDP

When you're ready, reflash with `firmware/MycoBrain_SideA_MDP/` and the standalone adapter automatically switches paths — no agent config change. The synthetic-frame branch becomes inert; real MDP frames flow through.
