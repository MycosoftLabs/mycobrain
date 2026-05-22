# Deploy — `mycobrain-jet-a`

## Identify what's running today

```bash
ssh jetson@192.168.0.228
sudo lsof -iTCP:8787 -sTCP:LISTEN
systemctl list-units --type=service --no-legend | grep -Ei 'mycobrain|openclaw|mosquitto'
journalctl -u "$(systemctl list-units --type=service --no-legend | grep mycobrain | head -1 | awk '{print $1}')" -n 50
```

(Paste output back; Claude will diff against `PORT_8787_HTTP_API_SPEC_MAY19_2026.md`.)

## Cutover to unified agent

1. Stop the existing service (whatever it is):
   ```bash
   sudo systemctl stop <existing-service>
   sudo systemctl disable <existing-service>
   ```
2. Install the unified agent:
   ```bash
   sudo bash /opt/mycobrain/scripts/agent-install-jetson.sh --adapter jetson_orin
   ```
3. Configure env:
   ```bash
   sudo cp /opt/mycobrain/agents/deploy/env/jetson_orin.env.example /etc/mycobrain/agent.env
   sudo nano /etc/mycobrain/agent.env
   # MYCOBRAIN_DEVICE_NICKNAME=jet-a
   # MYCOBRAIN_HOST_IP=192.168.0.228
   # MYCOBRAIN_SIDE_A_PORT=/dev/ttyTHS1
   # MYCOBRAIN_SIDE_B_PORT=/dev/ttyTHS2
   # MYCOBRAIN_OPENCLAW_ENABLED=true
   ```
4. Start:
   ```bash
   sudo systemctl enable --now mycobrain-agent
   curl -s http://localhost:8787/status | jq
   ```
5. Verify on NatureOS: visit `https://mycosoft.com/natureos/devices` (as `morgan@mycosoft.org`) — card should appear green within 30s.

## Roll back

If the unified agent misbehaves, the previous service can be re-enabled. Note where its env was stored before installing the new agent.
