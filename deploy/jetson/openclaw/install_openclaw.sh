#!/usr/bin/env bash
# install_openclaw.sh — Install OpenClaw gateway for MycoBrain on Jetson
#
# Prerequisites:
#   - NVIDIA Jetson (AGX Orin, Orin NX Super, Orin Nano Super)
#   - MycoBrain Side B connected via UART (/dev/ttyTHS1)
#   - Internet connectivity for npm packages
#
# Usage:
#   sudo bash install_openclaw.sh
#
# This script:
#   1. Installs Node.js 24 via nvm (if not present)
#   2. Installs OpenClaw globally
#   3. Copies MycoBrain skills to OpenClaw workspace
#   4. Installs the systemd service
#   5. Configures serial port permissions

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
OPENCLAW_WORKSPACE="$HOME/.openclaw/workspace"
SKILLS_SRC="$REPO_ROOT/openclaw/skills"

echo "=== MycoBrain OpenClaw Gateway Installer ==="
echo "Repository: $REPO_ROOT"
echo "OpenClaw workspace: $OPENCLAW_WORKSPACE"

# ---- Node.js ----
if ! command -v node &>/dev/null || [[ $(node -v | cut -d. -f1 | tr -d v) -lt 22 ]]; then
  echo "Installing Node.js 24 via nvm..."
  if ! command -v nvm &>/dev/null; then
    curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.1/install.sh | bash
    export NVM_DIR="$HOME/.nvm"
    [ -s "$NVM_DIR/nvm.sh" ] && . "$NVM_DIR/nvm.sh"
  fi
  nvm install 24
  nvm use 24
else
  echo "Node.js $(node -v) already installed"
fi

# ---- OpenClaw ----
if ! command -v openclaw &>/dev/null; then
  echo "Installing OpenClaw..."
  npm install -g openclaw@latest
else
  echo "OpenClaw $(openclaw --version 2>/dev/null || echo 'installed') already present"
fi

# ---- Skills ----
echo "Installing MycoBrain skills..."
mkdir -p "$OPENCLAW_WORKSPACE/skills"

# Copy mycobrain-control skill
cp -r "$SKILLS_SRC/mycobrain-control" "$OPENCLAW_WORKSPACE/skills/"
echo "  Installed: mycobrain-control"

# Copy sensecraft-publish skill
cp -r "$SKILLS_SRC/sensecraft-publish" "$OPENCLAW_WORKSPACE/skills/"
echo "  Installed: sensecraft-publish"

# Install skill dependencies
cd "$OPENCLAW_WORKSPACE/skills/mycobrain-control" && npm install --production 2>/dev/null || true
cd "$OPENCLAW_WORKSPACE/skills/sensecraft-publish" && npm install --production 2>/dev/null || true

# ---- Configuration ----
echo "Installing OpenClaw configuration..."
mkdir -p "$HOME/.openclaw"
cp "$SCRIPT_DIR/openclaw.json" "$HOME/.openclaw/openclaw.json"

# Copy voice persona if reSpeaker integration is desired
if [ -f "$REPO_ROOT/openclaw/respeaker/SOUL.md" ]; then
  mkdir -p "$HOME/.openclaw/agents/main/agent"
  cp "$REPO_ROOT/openclaw/respeaker/SOUL.md" "$HOME/.openclaw/agents/main/agent/SOUL.md"
  echo "  Installed: Voice persona (SOUL.md)"
fi

# ---- Serial permissions ----
echo "Configuring serial port permissions..."
if [ -e /dev/ttyTHS1 ]; then
  sudo usermod -aG dialout "$(whoami)" 2>/dev/null || true
  echo "  Added $(whoami) to dialout group for /dev/ttyTHS1"
fi

# ---- Systemd service ----
echo "Installing systemd service..."
sudo cp "$SCRIPT_DIR/mycobrain-openclaw.service" /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable mycobrain-openclaw.service
echo "  Service installed and enabled"

echo ""
echo "=== Installation Complete ==="
echo ""
echo "Next steps:"
echo "  1. Set environment variables in /etc/mycosoft/openclaw.env:"
echo "     ANTHROPIC_API_KEY=sk-ant-..."
echo "     MYCOBRAIN_SERIAL_PORT=/dev/ttyTHS1"
echo "     # Optional: SENSECRAFT_ORG_ID, SENSECRAFT_API_KEY, SENSECRAFT_DEVICE_EUI"
echo "     # Optional: TELEGRAM_BOT_TOKEN, DISCORD_BOT_TOKEN"
echo ""
echo "  2. Start the service:"
echo "     sudo systemctl start mycobrain-openclaw"
echo ""
echo "  3. Check status:"
echo "     sudo systemctl status mycobrain-openclaw"
echo "     openclaw dashboard  # Open web UI"
echo ""
echo "  4. For voice interface, flash reSpeaker-claw firmware:"
echo "     See: openclaw/respeaker/mimi_secrets.h.template"
