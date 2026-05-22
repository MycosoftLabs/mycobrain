# Deploy — `mycobrain-pi-a`

## Identify

Same probe block as `mycobrain-jet-a` — point at `192.168.0.123`.

## Install

```bash
sudo bash /opt/mycobrain/scripts/agent-install-pi.sh
sudo cp /opt/mycobrain/agents/deploy/env/raspberry_pi.env.example /etc/mycobrain/agent.env
sudo nano /etc/mycobrain/agent.env
# MYCOBRAIN_DEVICE_NICKNAME=pi-a
# MYCOBRAIN_HOST_IP=192.168.0.123
# MYCOBRAIN_SIDE_A_PORT=/dev/serial0
sudo systemctl enable --now mycobrain-agent
```

## Pi-specific gotchas

- UART grabbed by `serial-getty` by default. The installer disables it.
- No CUDA — TAC-O inference modules return `no_model`.
- GPIO available via `/command` `target: "gpio"`.
