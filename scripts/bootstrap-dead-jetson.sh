#!/usr/bin/env bash
# Cold-boot the older Jetson (device #3 in AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md).
# Run on the Jetson itself.
set -euo pipefail

echo "=== MycoBrain — Dead Jetson Bootstrap ==="
echo "[1/5] Identify Jetson"
cat /etc/nv_tegra_release 2>/dev/null || echo "  (no /etc/nv_tegra_release — not a Jetson?)"
echo "Arch: $(uname -m)"

echo "[2/5] Install OS prereqs"
apt update
apt install -y python3 python3-venv python3-pip git curl jq

echo "[3/5] Flash hint"
cat <<EOF
  Side A and Side B firmware should be flashed from another machine that has
  PlatformIO and the proper USB drivers, then the boards reconnected to this
  Jetson. From the mycobrain repo:
    .\\scripts\\flash-mycobrain-production.ps1 -Board SideA -Role mushroom1 -Port COMx
    .\\scripts\\flash-mycobrain-production.ps1 -Board SideB -Port COMy
  Skip this step if you already flashed them.
EOF

echo "[4/5] Install agent"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
bash "$REPO_ROOT/scripts/agent-install-jetson.sh" --adapter jetson_legacy

echo "[5/5] Verify"
echo "  Edit /etc/mycobrain/agent.env, then:"
echo "    systemctl enable --now mycobrain-agent"
echo "    curl http://localhost:8787/status | jq"
echo "    curl http://localhost:8787/info  | jq"
