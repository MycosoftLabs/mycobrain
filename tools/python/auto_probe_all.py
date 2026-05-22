"""Self-running probe of all three reachable MycoBrains.

This script is what Claude double-clicks via File Explorer when it needs to
gather the hardware audit data. It runs:

  1. probe_com.py against COM4 (or whatever MYCOBRAIN_COM_PORT is set to)
  2. probe-jetson.sh on jetson@192.168.0.228 via paramiko
  3. probe-jetson.sh on jetson@192.168.0.123 via paramiko

Outputs land in the mycobrain repo root as:
  - probe_com4.txt
  - probe_jet228.txt
  - probe_jet123.txt
  - probe_done.flag  (touched after success, deleted on start)

Dependencies (installed by auto-probe-all.bat before this runs):
  pip install paramiko pyserial

The SSH password is read from %MYCOBRAIN_JETSON_PASSWORD% env var
(set by the launching .bat). If unset, that probe is skipped.

NOTE: This script is intended to be ephemeral. The .bat that launches it
should be deleted after the probe completes. The password is never written
to any output file.
"""

from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
COM_PORT = os.environ.get("MYCOBRAIN_COM_PORT", "COM4")
JETSONS = [
    ("228", os.environ.get("MYCOBRAIN_JET228", "192.168.0.228")),
    ("123", os.environ.get("MYCOBRAIN_JET123", "192.168.0.123")),
]
JETSON_USER = os.environ.get("MYCOBRAIN_JETSON_USER", "jetson")
JETSON_PASSWORD = os.environ.get("MYCOBRAIN_JETSON_PASSWORD")

DONE_FLAG = REPO_ROOT / "probe_done.flag"
LOG = REPO_ROOT / "probe_run.log"


def log(msg: str) -> None:
    line = f"[{time.strftime('%H:%M:%S')}] {msg}"
    print(line, flush=True)
    with LOG.open("a", encoding="utf-8") as f:
        f.write(line + "\n")


def reset_outputs() -> None:
    for name in ["probe_com4.txt", "probe_jet228.txt", "probe_jet123.txt", "probe_done.flag", "probe_run.log"]:
        p = REPO_ROOT / name
        try:
            p.unlink()
        except FileNotFoundError:
            pass


def probe_com() -> None:
    out = REPO_ROOT / f"probe_{COM_PORT.lower()}.txt"
    if COM_PORT.upper().startswith("COM"):
        out = REPO_ROOT / "probe_com4.txt"
    script = REPO_ROOT / "tools" / "python" / "probe_com.py"
    log(f"Probing {COM_PORT} via {script.name}")
    try:
        result = subprocess.run(
            [sys.executable, str(script), COM_PORT, "--seconds", "10"],
            capture_output=True,
            text=True,
            timeout=30,
        )
        out.write_text(
            f"=== probe_com.py {COM_PORT} ===\n"
            f"--- stdout ---\n{result.stdout}\n"
            f"--- stderr ---\n{result.stderr}\n"
            f"--- exit code: {result.returncode} ---\n",
            encoding="utf-8",
        )
        log(f"  -> {out.name} ({len(result.stdout)} bytes stdout)")
    except Exception as exc:  # noqa: BLE001
        out.write_text(f"COM4 probe failed: {exc}\n", encoding="utf-8")
        log(f"  -> {out.name} ERROR: {exc}")


def probe_jetsons() -> None:
    if not JETSON_PASSWORD:
        log("Skipping Jetson probes — MYCOBRAIN_JETSON_PASSWORD not set")
        return
    try:
        import paramiko  # noqa: F401
    except ImportError:
        log("paramiko not installed — pip install paramiko then re-run")
        return

    probe_script = (REPO_ROOT / "scripts" / "probe-jetson.sh").read_text(encoding="utf-8", errors="replace")
    for tag, host in JETSONS:
        out = REPO_ROOT / f"probe_jet{tag}.txt"
        log(f"Probing jetson@{host}")
        try:
            _run_remote_probe(host, probe_script, out)
            log(f"  -> {out.name} ({out.stat().st_size} bytes)")
        except Exception as exc:  # noqa: BLE001
            out.write_text(f"Jetson {host} probe failed: {exc}\n", encoding="utf-8")
            log(f"  -> {out.name} ERROR: {exc}")


def _run_remote_probe(host: str, script: str, out_path: Path) -> None:
    import paramiko

    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(
        hostname=host,
        port=22,
        username=JETSON_USER,
        password=JETSON_PASSWORD,
        timeout=10,
        banner_timeout=10,
        auth_timeout=10,
        look_for_keys=False,
        allow_agent=False,
    )
    try:
        # Pipe the script over stdin to bash -s
        stdin, stdout, stderr = client.exec_command("bash -s", timeout=60)
        stdin.write(script)
        stdin.channel.shutdown_write()
        out = stdout.read().decode("utf-8", errors="replace")
        err = stderr.read().decode("utf-8", errors="replace")
        exit_code = stdout.channel.recv_exit_status()
        out_path.write_text(
            f"=== probe-jetson.sh on {JETSON_USER}@{host} ===\n"
            f"--- stdout ---\n{out}\n"
            f"--- stderr ---\n{err}\n"
            f"--- exit code: {exit_code} ---\n",
            encoding="utf-8",
        )
    finally:
        client.close()


def main() -> None:
    log(f"auto_probe_all start — repo at {REPO_ROOT}")
    reset_outputs()
    probe_com()
    probe_jetsons()
    DONE_FLAG.write_text(time.strftime("%Y-%m-%dT%H:%M:%S\n"), encoding="utf-8")
    log("done")


if __name__ == "__main__":
    main()
