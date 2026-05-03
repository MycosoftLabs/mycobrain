# MycoBrain MQTT Broker — Production Deploy Guide

**For:** CEO / production server operator  
**From:** Berto  
**Date:** 2026-04-07  
**Updated:** 2026-04-08 — WebSockets (9001) + Cloudflare WSS; LAN stays on 1883.  
**What this is:** Everything you (or your agent) need to get the MycoBrain MQTT broker running on the production server. Once it's up, I'll point the Jetson at it and we're live on prod.

---

## What you're deploying

A Mosquitto MQTT broker in Docker. It receives sensor telemetry from the Jetson gateway over MQTT (port 1883). The entire deployment is one folder with 3 files that matter.

## What you'll receive

A zip file: `mycobrain-mqtt-prod.zip`. Unzip it and you get:

```
mqtt-broker/
├── docker-compose.yml    ← defines the container
├── config/
│   └── mosquitto.conf    ← broker config (ready to go, don't edit)
├── data/                 ← broker persistence (starts empty)
└── log/                  ← broker logs (starts empty)
```

That's it. No build step, no dependencies beyond Docker.

---

## Step-by-step deployment

### 1. Prerequisites

- Docker Engine + Docker Compose plugin installed on the server
- If not installed: `sudo apt update && sudo apt install -y docker.io docker-compose-v2`
- Make sure your user can run docker: `sudo usermod -aG docker $USER` (log out/in after)

### 2. Unzip and place the folder

```bash
unzip mycobrain-mqtt-prod.zip -d /opt/mycobrain/
cd /opt/mycobrain/mqtt-broker
```

Put it wherever makes sense — `/opt/mycobrain/mqtt-broker` is just a suggestion.

### 3. Create the password file

This must exist before starting the container or it will crash-loop.

```bash
touch config/passwd
chmod 644 config/passwd
```

### 4. Start the broker

```bash
docker compose up -d
```

Verify it's running:

```bash
docker logs mycobrain-mqtt
```

You should see:

```
mosquitto version 2.1.2 running
```

If you see `Error: Unable to open pwfile` — go back to step 3.

### 5. Create the MQTT user

```bash
docker exec mycobrain-mqtt mosquitto_passwd -b /mosquitto/config/passwd mycobrain <PICK_A_PASSWORD>
docker restart mycobrain-mqtt
```

Replace `<PICK_A_PASSWORD>` with a real password. **Send me the password** so I can configure the Jetson.

### 6. Verify it works

```bash
# Terminal 1 — subscribe:
docker exec mycobrain-mqtt mosquitto_sub -u mycobrain -P <YOUR_PASSWORD> -t "mycobrain/#" -C 1 -W 10

# Terminal 2 — publish a test message:
docker exec mycobrain-mqtt mosquitto_pub -u mycobrain -P <YOUR_PASSWORD> -t "mycobrain/test" -m '{"status":"ok"}'
```

Terminal 1 should print `{"status":"ok"}`.

### 7. Network access (LAN fast path + public WSS)

The container exposes:

| Port | Protocol | Use |
|------|----------|-----|
| **1883** | MQTT (TCP) | **LAN / VPN** — lowest latency; Jetson on same site uses `mqtt://<broker-lan-ip>:1883`. |
| **9001** | MQTT over WebSockets | **Cloudflare Tunnel only** — not for raw public Internet clients; lock to tunnel host IP. |

**LAN — open MQTT to the site:**

```bash
sudo ufw allow 1883/tcp comment 'MQTT LAN'
```

**WebSockets for Cloudflare (Sandbox tunnel hits broker LAN IP):**

On the **MQTT VM**, allow **only** the machine running `cloudflared` (Sandbox **192.168.0.187**) to reach 9001:

```bash
sudo ufw allow from 192.168.0.187 to any port 9001 proto tcp comment 'MQTT WS tunnel'
sudo ufw enable
```

**Public Internet — do not use `mqtt://mqtt.mycosoft.com:1883`.** Cloudflare’s edge is not raw MQTT TCP on that hostname. Remote devices use **WSS**:

- URL: `wss://mqtt.mycosoft.com` (port **443**, TLS terminated at Cloudflare)
- WebSocket path: **`/`** (Mosquitto default)
- Same username/password as LAN (`mycobrain` + password from step 5)

**Apply tunnel routing** (from a machine with Cloudflare API token + account id in env, e.g. MAS repo `.credentials.local`):

```bash
cd /path/to/mycosoft-mas
python scripts/mqtt_tunnel_cloudflare_apply.py
```

That sets `mqtt.mycosoft.com` → `http://<MQTT_BROKER_LAN_IP>:9001` on your existing **mycosoft-tunnel** without wiping other hostnames.

**DNS:** `mqtt.mycosoft.com` must be a **proxied** hostname pointing at the tunnel (CNAME to `*.cfargotunnel.com` as for other tunnel apps).

---

## What I need back from you

Once the broker is running, send me:

1. **The MQTT password** you chose in step 5
2. **The hostname or IP** the Jetson should connect to:
   - If using Cloudflare Tunnel: the tunnel hostname (e.g., `mqtt.yourdomain.com`)
   - If direct: the server's IP address
3. **Confirmation** that the smoke test in step 6 passed

I'll update the Jetson's environment variables and restart the publisher. Examples:

**Same LAN as broker:**

```bash
MYCOBRAIN_MQTT_URL=mqtt://<BROKER_LAN_IP>:1883
MYCOBRAIN_MQTT_USERNAME=mycobrain
MYCOBRAIN_MQTT_PASSWORD=<YOUR_PASSWORD>
```

**Remote (use your MQTT client’s WebSocket mode):**

```text
wss://mqtt.mycosoft.com:443  path /  user mycobrain  password <YOUR_PASSWORD>
```

(Python Paho: `transport="websockets"`, `path="/"`.)

---

## Reference

| Setting | Value |
|---------|-------|
| Container name | `mycobrain-mqtt` |
| Image | `eclipse-mosquitto:2` (pulled from Docker Hub automatically) |
| Ports | `1883` MQTT (LAN); `9001` WebSockets (tunnel to Cloudflare only) |
| MQTT user | `mycobrain` |
| Auth | Password file, no anonymous connections |
| TLS | **Remote:** TLS at Cloudflare (`wss://`). **LAN:** plain TCP on 1883 (trusted network). |
| Restart policy | `unless-stopped` (survives server reboots) |
| Data | Persisted in `data/` directory |
| Logs | Written to `log/mosquitto.log` and stdout |

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Container crash-loops | Check `docker logs mycobrain-mqtt`. Usually means `config/passwd` doesn't exist — run step 3 |
| "Connection refused" from Jetson | Broker not running, or port 1883 not open on LAN. Remote: use **WSS**, not `tcp://mqtt.mycosoft.com:1883`. |
| Remote Jetson `Network unreachable` to public hostname | Often IPv6 or wrong transport; use **wss://** on 443 or force IPv4 for LAN `mqtt://` |
| "Not authorized" from Jetson | Wrong username or password. Re-run step 5 |
| Image won't pull | Server needs internet access to pull `eclipse-mosquitto:2` from Docker Hub on first run |
