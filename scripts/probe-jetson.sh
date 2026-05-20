#!/usr/bin/env bash
# Run ON the Jetson (or Pi) to capture everything we need to align it with the unified agent.
# Usage:
#   ssh user@192.168.0.228 'bash -s' < scripts/probe-jetson.sh
# Output is a single JSON-ish report on stdout.
set -uo pipefail

echo "=========================== MYCOBRAIN HOST PROBE ==========================="
echo "host: $(hostname)"
echo "date: $(date -Iseconds)"
echo "ip:   $(hostname -I 2>/dev/null | awk '{print $1}')"
echo "user: $(whoami)"
echo

echo "--- OS / Arch ---"
uname -a
[[ -r /etc/os-release ]] && cat /etc/os-release | head -8
[[ -r /etc/nv_tegra_release ]] && echo "nv_tegra: $(cat /etc/nv_tegra_release)"
[[ -r /proc/device-tree/model ]] && echo "model: $(tr -d '\0' </proc/device-tree/model)"
echo

echo "--- Listener on :8787 ---"
ss -tlnp 2>/dev/null | grep ':8787' || sudo lsof -iTCP:8787 -sTCP:LISTEN 2>/dev/null || echo "(no privileges to inspect ports)"
echo

echo "--- All listening TCP services ---"
ss -tlnp 2>/dev/null | head -40
echo

echo "--- /status from local agent ---"
curl -fsS --max-time 3 http://127.0.0.1:8787/status 2>&1 | head -100 || echo "(no response)"
echo
echo "--- /info from local agent ---"
curl -fsS --max-time 3 http://127.0.0.1:8787/info 2>&1 | head -100 || echo "(no response)"
echo
echo "--- /healthz / /health (compat) ---"
curl -fsS --max-time 3 http://127.0.0.1:8787/healthz 2>&1 | head -20 || true
curl -fsS --max-time 3 http://127.0.0.1:8787/health  2>&1 | head -20 || true
echo

echo "--- systemd units touching mycobrain / openclaw / mqtt ---"
systemctl list-units --type=service --no-legend --all 2>/dev/null | grep -Ei 'mycobrain|openclaw|mosquitto|mycosoft' || echo "(none)"
echo
for u in mycobrain-agent mycobrain-ondevice-operator mycobrain-gateway-router openclaw mosquitto; do
  echo "----- systemctl status $u -----"
  systemctl status "$u" --no-pager 2>&1 | head -25 || true
done
echo

echo "--- OpenClaw on 127.0.0.1:8000 ---"
ss -tlnp 2>/dev/null | grep ':8000' || true
curl -fsS --max-time 3 http://127.0.0.1:8000/healthz 2>&1 | head -20 || curl -fsS --max-time 3 http://127.0.0.1:8000/ 2>&1 | head -20 || echo "(no openclaw response)"
echo

echo "--- Serial ports present ---"
ls -la /dev/ttyTHS* /dev/ttyUSB* /dev/ttyACM* /dev/serial0 2>/dev/null || echo "(no UART devices)"
echo

echo "--- MQTT-related processes ---"
pgrep -fa 'mosquitto|mqtt' || true
echo

echo "--- Env files of interest ---"
for f in /etc/mycobrain/agent.env /etc/mycobrain/ondevice-operator.env /etc/mycobrain/gateway-router.env /etc/mycosoft/*.env; do
  [[ -r "$f" ]] && { echo "===== $f ====="; sed -E 's/(PASSWORD|TOKEN|KEY|SECRET)=.*$/\1=<redacted>/i' "$f"; }
done
echo

echo "--- Recent agent / operator logs (last 40 lines) ---"
for u in mycobrain-agent mycobrain-ondevice-operator mycobrain-gateway-router; do
  if systemctl is-active --quiet "$u" 2>/dev/null; then
    echo "----- journalctl -u $u (40 lines) -----"
    journalctl -u "$u" -n 40 --no-pager 2>/dev/null || true
  fi
done
echo

echo "--- Process list (top mycobrain / python / openclaw) ---"
ps auxf | grep -Ei 'mycobrain|openclaw|python.*8787|python.*8110|python.*8120' | grep -v grep || echo "(none)"
echo

echo "--- Disk: /opt/mycobrain, /opt/mycosoft ---"
ls -la /opt/mycobrain 2>/dev/null
ls -la /opt/mycosoft 2>/dev/null
ls -la /opt/mycosoft/mas 2>/dev/null
echo

echo "============================== PROBE COMPLETE =============================="
