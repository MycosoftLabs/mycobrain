#!/usr/bin/env bash
# Bootstrap the mycobrain-agent on a Jetson (Orin or legacy).
# Usage:
#   sudo bash scripts/agent-install-jetson.sh --adapter jetson_orin
#   sudo bash scripts/agent-install-jetson.sh --adapter jetson_legacy
set -euo pipefail

ADAPTER="jetson_orin"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --adapter) ADAPTER="$2"; shift 2 ;;
    *) echo "Unknown arg: $1" >&2; exit 1 ;;
  esac
done

if [[ "$ADAPTER" != "jetson_orin" && "$ADAPTER" != "jetson_legacy" ]]; then
  echo "Invalid --adapter (must be jetson_orin or jetson_legacy)" >&2
  exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec bash "$REPO_ROOT/agents/deploy/systemd/install.sh" "$ADAPTER"
