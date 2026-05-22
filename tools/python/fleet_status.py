"""Poll every paired MycoBrain agent and print a fleet status table.

Reads the agent's :8787 /status and /info on each host (LAN). Used for the
Phase 5 verification pass and as a quick "is the fleet healthy" check.

Usage:
    python tools/python/fleet_status.py
    python tools/python/fleet_status.py --hosts 192.168.0.228 192.168.0.123
    python tools/python/fleet_status.py --json   # machine-readable

Default host list comes from docs/AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md plus
any extra hosts you pass on the CLI.

Dependencies: urllib (stdlib only). No pip install needed.
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from typing import Any

DEFAULT_HOSTS = [
    "192.168.0.228",  # mycobrain-jet-a (Jetson + OpenClaw)
    "192.168.0.123",  # mycobrain-pi-a (Pi + OpenClaw)
    # Add 127.0.0.1 here when the bench device's agent is up; add the
    # older Jetson's IP once it's bootstrapped.
]

PORT = 8787
TIMEOUT_S = 3.0


@dataclass
class HostReport:
    host: str
    reachable: bool
    status: dict[str, Any] | None
    info: dict[str, Any] | None
    error: str | None = None


def _get(url: str) -> dict[str, Any] | None:
    req = urllib.request.Request(url, headers={"Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=TIMEOUT_S) as resp:
        if resp.status >= 400:
            return None
        return json.loads(resp.read().decode("utf-8"))


def probe(host: str) -> HostReport:
    base = f"http://{host}:{PORT}"
    try:
        status = _get(f"{base}/status")
        info = _get(f"{base}/info")
        return HostReport(host=host, reachable=True, status=status, info=info)
    except urllib.error.URLError as exc:
        return HostReport(host=host, reachable=False, status=None, info=None, error=str(exc.reason))
    except Exception as exc:  # noqa: BLE001
        return HostReport(host=host, reachable=False, status=None, info=None, error=str(exc))


def render_table(reports: list[HostReport]) -> str:
    rows = [
        ("HOST", "DEVICE_ID", "HOST_KIND", "SIDE_A", "SIDE_B", "OPENCLAW", "MQTT", "AGENT_V"),
    ]
    for r in reports:
        if not r.reachable:
            rows.append((r.host, "-", "-", "-", "-", "-", "-", f"unreachable: {r.error}"))
            continue
        st = r.status or {}
        info = r.info or {}
        device_id = st.get("device_id", "?")
        host_kind = st.get("host_kind") or info.get("host_kind", "?")
        side_a = "✓" if (st.get("side_a", {}) or {}).get("linked") else "✗"
        side_b = "✓" if (st.get("side_b", {}) or {}).get("linked") else "-"
        oc = (st.get("openclaw", {}) or {})
        oc_str = "ready" if oc.get("ready") else ("available" if oc.get("available") else "-")
        mqtt = "✓" if (st.get("mqtt", {}) or {}).get("connected") else "✗"
        agent_v = st.get("agent_version", "?")
        rows.append((r.host, device_id, host_kind, side_a, side_b, oc_str, mqtt, agent_v))

    widths = [max(len(str(row[i])) for row in rows) for i in range(len(rows[0]))]
    lines = []
    for i, row in enumerate(rows):
        line = "  ".join(str(c).ljust(widths[j]) for j, c in enumerate(row))
        lines.append(line)
        if i == 0:
            lines.append("-" * len(line))
    return "\n".join(lines)


def main() -> None:
    ap = argparse.ArgumentParser(description="MycoBrain fleet status")
    ap.add_argument("--hosts", nargs="*", default=DEFAULT_HOSTS, help="IPs to probe")
    ap.add_argument("--json", action="store_true", help="Emit JSON for piping")
    ap.add_argument("--timeout", type=float, default=TIMEOUT_S)
    args = ap.parse_args()

    global TIMEOUT_S  # noqa: PLW0603
    TIMEOUT_S = args.timeout

    with ThreadPoolExecutor(max_workers=min(8, len(args.hosts) or 1)) as ex:
        reports = list(ex.map(probe, args.hosts))

    if args.json:
        out = [
            {
                "host": r.host,
                "reachable": r.reachable,
                "status": r.status,
                "info": r.info,
                "error": r.error,
            }
            for r in reports
        ]
        print(json.dumps(out, indent=2, default=str))
        return

    print(render_table(reports))
    # Exit non-zero if any host unreachable, for monitoring use
    if any(not r.reachable for r in reports):
        sys.exit(2)


if __name__ == "__main__":
    main()
