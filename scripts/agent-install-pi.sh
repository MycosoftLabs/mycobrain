#!/usr/bin/env bash
# Bootstrap the mycobrain-agent on a Raspberry Pi.
# Usage: sudo bash scripts/agent-install-pi.sh
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "Enabling UART for /dev/serial0 (you may need to reboot)"
# Only modify if user hasn't already configured
if [[ -f /boot/config.txt ]] && ! grep -q "^enable_uart=1" /boot/config.txt; then
  echo "enable_uart=1" | sudo tee -a /boot/config.txt
fi

# Disable getty on serial so we own the port
systemctl disable --now serial-getty@ttyS0.service 2>/dev/null || true
systemctl disable --now serial-getty@ttyAMA0.service 2>/dev/null || true

exec bash "$REPO_ROOT/agents/deploy/systemd/install.sh" raspberry_pi
