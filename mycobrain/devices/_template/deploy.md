# Deploy runbook — `<nickname>`

## Prerequisites

- This PC has SSH access to the host (`ssh <user>@<host>`)
- Latest firmware flashed on Side A + Side B (or single-MCU per host kind)
- Optional: OpenClaw wired and powered

## Install the agent

Linux (Jetson, Pi):

```bash
sudo bash /opt/mycobrain/scripts/agent-install-jetson.sh --adapter <kind>
sudo nano /etc/mycobrain/agent.env
sudo systemctl enable --now mycobrain-agent
```

Windows (standalone bench):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\agent-install-standalone.ps1 -Port COM4 -Nickname <nickname>
```

## Pair to NatureOS

1. Open `https://mycosoft.com/natureos/devices?pair=1`
2. Sign in with Google (`@mycosoft.org` only)
3. Enter the device IP + adapter pair-token
4. Confirm — the device appears on the fleet grid

## Verify

```bash
curl http://<device-ip>:8787/status
curl http://<device-ip>:8787/info
```

`status.linked` should be `true` for `side_a` (and `side_b` if dual-MCU).

## Roll back

```bash
sudo systemctl disable --now mycobrain-agent
sudo rm /etc/mycobrain/agent.env
# device disappears from fleet grid after ~60s of missed presence
```
