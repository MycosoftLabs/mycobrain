> AWS topology correction 2026-11-27: local production servers, LAN addresses and VM/Proxmox/NAS deployment instructions are retired. Use [AWS_INFRA_REFERENCE.md](../../../AWS_INFRA_REFERENCE.md). Preserve historical evidence and optional device/external connectors.

# Deploy — `mycobrain-pi-a`

> **Network update (2026-11-27):** Device addresses below were on the garage LAN, which was decommissioned 2026-10-15 (Mycosoft has no local servers; MINDEX/MAS/website run on AWS). Confirm each device's current network address live before connecting; never assume the old 192.168.0.x address.


## Identify

Same probe block as `mycobrain-jet-a` — point at `<device IP — confirm live>`.

## Install

```bash
sudo bash /opt/mycobrain/scripts/agent-install-pi.sh
sudo cp /opt/mycobrain/agents/deploy/env/raspberry_pi.env.example /etc/mycobrain/agent.env
sudo nano /etc/mycobrain/agent.env
# MYCOBRAIN_DEVICE_NICKNAME=pi-a
# MYCOBRAIN_HOST_IP=<device-ip>
# MYCOBRAIN_SIDE_A_PORT=/dev/serial0
sudo systemctl enable --now mycobrain-agent
```

## Pi-specific gotchas

- UART grabbed by `serial-getty` by default. The installer disables it.
- No CUDA — TAC-O inference modules return `no_model`.
- GPIO available via `/command` `target: "gpio"`.
