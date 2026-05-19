#!/usr/bin/env bash
set -euo pipefail
if lsmod | awk '{print $1}' | grep -qx algif_aead; then
    echo "[abort] algif_aead loaded -- runner unpatched (CVE-2026-31431)"; exit 1
fi
if ! grep -rqE '^install\s+algif_aead\s+/bin/false' /etc/modprobe.d/ 2>/dev/null; then
    echo "[abort] CVE-2026-31431 mitigation not present"; exit 1
fi
exit 0
