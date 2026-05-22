# Deploy — `mycobrain-bench` (Windows + USB serial)

## Install

From an elevated PowerShell on Morgan's machine:

```powershell
cd D:\Users\admin2\Desktop\MYCOSOFT\CODE\mycobrain
powershell -ExecutionPolicy Bypass -File .\scripts\agent-install-standalone.ps1 -Port COM4 -Nickname bench
```

(Requires `python` and `nssm` on PATH. `winget install nssm` installs nssm.)

## Configure

```powershell
notepad "$env:PROGRAMDATA\mycobrain\agent.env"
# MYCOBRAIN_DEVICE_NICKNAME=bench
# MYCOBRAIN_SIDE_A_PORT=COM4
# MYCOBRAIN_AUTH_MODE=pair_token
# MYCOBRAIN_PAIR_TOKEN=<random>
# MYCOBRAIN_OPENCLAW_ENABLED=false
```

## Start + verify

```powershell
Start-Service mycobrain-agent
curl http://localhost:8787/status
```

## Pair with NatureOS

Browser → `https://mycosoft.com/natureos/devices?pair=1` → enter `http://<this-pc-LAN-ip>:8787` and the pair token → confirm.
