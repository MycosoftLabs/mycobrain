#!/usr/bin/env bash
# Install the mycobrain-agent systemd unit on a Linux host (Jetson or Pi).
# Run with sudo.
set -euo pipefail

ADAPTER="${1:-jetson_orin}"   # jetson_orin | jetson_legacy | raspberry_pi
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"

if [[ "$EUID" -ne 0 ]]; then
  echo "Run as root (sudo)." >&2
  exit 1
fi

echo "[1/6] Creating mycobrain user and dirs"
id -u mycobrain &>/dev/null || useradd --system --no-create-home --shell /usr/sbin/nologin mycobrain
install -d -o mycobrain -g mycobrain /opt/mycobrain
install -d -o mycobrain -g mycobrain /var/log/mycobrain
install -d -o mycobrain -g mycobrain /var/lib/mycobrain
install -d /etc/mycobrain

echo "[2/6] Setting up Python venv"
python3 -m venv /opt/mycobrain/agent-venv
/opt/mycobrain/agent-venv/bin/pip install --upgrade pip
/opt/mycobrain/agent-venv/bin/pip install -e "$REPO_ROOT/agents"

echo "[3/6] Copying docs alongside the agent"
install -d /opt/mycobrain/docs
cp -r "$REPO_ROOT/docs/." /opt/mycobrain/docs/

echo "[4/6] Installing env template (only if absent)"
if [[ ! -f /etc/mycobrain/agent.env ]]; then
  cp "$REPO_ROOT/agents/deploy/env/${ADAPTER}.env.example" /etc/mycobrain/agent.env
  chmod 600 /etc/mycobrain/agent.env
  echo "  -> /etc/mycobrain/agent.env (edit before starting)"
fi

echo "[5/6] Installing systemd unit"
install -m 644 "$REPO_ROOT/agents/deploy/systemd/mycobrain-agent.service" /etc/systemd/system/
systemctl daemon-reload

echo "[6/6] Done. Next steps:"
echo "  edit  /etc/mycobrain/agent.env"
echo "  start sudo systemctl enable --now mycobrain-agent"
echo "  tail  journalctl -u mycobrain-agent -f"
