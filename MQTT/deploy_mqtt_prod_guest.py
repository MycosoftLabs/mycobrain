#!/usr/bin/env python3
"""
Deploy mycobrain-mqtt-prod to the MQTT VM per CEO_DEPLOY_GUIDE.md.

Uploads docker-compose + config, installs Docker if needed, starts compose,
creates mycobrain MQTT user, optional smoke test.

Env:
  MQTT_VM_HOST / MQTT_VM_GUEST_IP  default 192.168.0.196
  MQTT_VM_USER / MQTT_VM_SSH_USER  default mycosoft
  VM_PASSWORD / VM_SSH_PASSWORD
  MQTT_BROKER_PASSWORD             optional; defaults to VM password
  MQTT_SKIP_PULL=1                 skip docker compose pull for faster redeploy
  MQTT_SKIP_SMOKE=1                skip sub/pub smoke test for faster redeploy

Date: 2026-04-08
"""
from __future__ import annotations

import base64
import os
import sys
from pathlib import Path


def _mas_root() -> Path | None:
    code = Path(__file__).resolve().parent.parent.parent
    p = code / "MAS" / "mycosoft-mas"
    return p if p.is_dir() else None


def main() -> int:
    try:
        import paramiko
    except ImportError:
        print("ERROR: pip install paramiko", file=sys.stderr)
        return 1

    mr = _mas_root()
    if mr:
        sys.path.insert(0, str(mr))
    try:
        from infra.csuite.paramiko_ssh_dual import ssh_client_try_keys_password_kbd
    except ImportError:
        print("ERROR: Could not import infra.csuite.paramiko_ssh_dual (run from repo tree).", file=sys.stderr)
        return 1

    host = (os.environ.get("MQTT_VM_HOST") or os.environ.get("MQTT_VM_GUEST_IP") or "192.168.0.196").strip()
    user = (os.environ.get("MQTT_VM_USER") or os.environ.get("MQTT_VM_SSH_USER") or "mycosoft").strip()
    ssh_pw = (
        (os.environ.get("MQTT_VM_SSH_PASSWORD") or os.environ.get("VM_PASSWORD") or os.environ.get("VM_SSH_PASSWORD") or "")
        .strip()
    )
    mqtt_pw = (os.environ.get("MQTT_BROKER_PASSWORD") or ssh_pw).strip()

    if not ssh_pw:
        print("ERROR: Set VM_PASSWORD or VM_SSH_PASSWORD", file=sys.stderr)
        return 1
    if not mqtt_pw:
        print("ERROR: Set MQTT_BROKER_PASSWORD or VM_PASSWORD", file=sys.stderr)
        return 1

    bundle = Path(__file__).resolve().parent / "mycobrain-mqtt-prod"
    if not bundle.is_dir():
        print(f"ERROR: Missing {bundle}", file=sys.stderr)
        return 1

    passwd = bundle / "config" / "passwd"
    passwd.parent.mkdir(parents=True, exist_ok=True)
    if not passwd.exists():
        passwd.write_bytes(b"")

    remote_dir = "/opt/mycobrain/mqtt-broker"

    client = None
    ssh_err: Exception | None = None
    try:
        client = ssh_client_try_keys_password_kbd(host, user, ssh_pw, timeout=30.0)
    except Exception as e:
        ssh_err = e

    if client is None:
        import subprocess

        qga = mr / "scripts" / "deploy_mqtt_docker_via_pve_qga.py" if mr else None
        if qga and qga.is_file():
            print(
                f"SSH failed ({ssh_err}); falling back to Proxmox guest-agent deploy...",
                file=sys.stderr,
            )
            r = subprocess.run(
                [sys.executable, str(qga)],
                cwd=str(mr),
                env={**os.environ},
            )
            return r.returncode
        print(f"ERROR: SSH connect failed: {ssh_err}", file=sys.stderr)
        return 1

    def run_sudo_bash(script: str, timeout: int = 900) -> tuple[int, str, str]:
        _ = timeout  # kept for compatibility with callsites; command uses no inactivity timeout
        stdin, stdout, stderr = client.exec_command("sudo -S bash -s", get_pty=True, timeout=None)
        stdin.write(ssh_pw + "\n")
        stdin.write(script)
        if not script.endswith("\n"):
            stdin.write("\n")
        stdin.channel.shutdown_write()
        out = stdout.read().decode(errors="replace")
        err = stderr.read().decode(errors="replace")
        code = stdout.channel.recv_exit_status()
        return code, out, err

    init_dirs = f"""set -euo pipefail
mkdir -p {remote_dir}/config {remote_dir}/data {remote_dir}/log
chown -R {user}:{user} /opt/mycobrain
"""
    code, out, err = run_sudo_bash(init_dirs, timeout=600)
    if code != 0:
        sys.stdout.write(out)
        if err.strip():
            sys.stderr.write(err)
        client.close()
        return code

    sftp = client.open_sftp()
    try:
        sftp.put(bundle / "docker-compose.yml", f"{remote_dir}/docker-compose.yml")
        sftp.put(bundle / "config" / "mosquitto.conf", f"{remote_dir}/config/mosquitto.conf")
        sftp.put(bundle / "config" / "passwd", f"{remote_dir}/config/passwd")
    finally:
        sftp.close()

    skip_pull = (os.environ.get("MQTT_SKIP_PULL") or "").strip().lower() in {"1", "true", "yes", "on"}
    skip_smoke = (os.environ.get("MQTT_SKIP_SMOKE") or "").strip().lower() in {"1", "true", "yes", "on"}
    pw_b64 = base64.b64encode(mqtt_pw.encode("utf-8")).decode("ascii")
    pull_block = "echo 'MQTT_SKIP_PULL=1: skipping docker compose pull'" if skip_pull else "docker compose pull"
    smoke_block = (
        "echo 'MQTT_SKIP_SMOKE=1: skipping smoke test'"
        if skip_smoke
        else """docker exec mycobrain-mqtt mosquitto_sub -u mycobrain -P "$MQPW" -t 'mycobrain/#' -C 1 -W 15 &
sleep 2
docker exec mycobrain-mqtt mosquitto_pub -u mycobrain -P "$MQPW" -t mycobrain/test -m '{"status":"ok"}'
wait || true"""
    )
    setup = f"""set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
if ! command -v docker >/dev/null 2>&1; then
  apt-get update -y
  apt-get install -y docker.io docker-compose-v2
  systemctl enable --now docker
fi
usermod -aG docker {user} || true
chown -R {user}:{user} /opt/mycobrain
cd {remote_dir}
touch config/passwd
chown root:root config/passwd 2>/dev/null || true
chmod 644 config/passwd
{pull_block}
docker compose up -d
sleep 3
if command -v ufw >/dev/null 2>&1; then
  ufw allow OpenSSH 2>/dev/null || ufw allow 22/tcp 2>/dev/null || true
  ufw allow 1883/tcp comment 'MQTT LAN' 2>/dev/null || true
  ufw allow from 192.168.0.187 to any port 9001 proto tcp comment 'MQTT WS tunnel' 2>/dev/null || true
  ufw --force enable 2>/dev/null || true
fi
MQPW=$(echo {pw_b64} | base64 -d)
docker exec mycobrain-mqtt mosquitto_passwd -b /mosquitto/config/passwd mycobrain "$MQPW"
docker restart mycobrain-mqtt
sleep 3
docker logs mycobrain-mqtt 2>&1 | tail -n 30
{smoke_block}
echo '--- smoke done ---'
"""
    code, out, err = run_sudo_bash(setup, timeout=3600)
    sys.stdout.write(out)
    if err.strip():
        sys.stderr.write(err)

    client.close()

    print(
        f"\nJetson (same LAN - fastest):\n"
        f"  MYCOBRAIN_MQTT_URL=mqtt://{host}:1883\n"
        f"  MYCOBRAIN_MQTT_USERNAME=mycobrain\n"
        f"  MYCOBRAIN_MQTT_PASSWORD=<from secure store>\n\n"
        f"Jetson (Internet - WebSocket + TLS via Cloudflare):\n"
        f"  Use broker URL wss://mqtt.mycosoft.com:443 with WebSocket transport and path /.\n"
        f"  Same username/password; do not use raw tcp :1883 on the public hostname.\n\n"
        f"After broker up: on dev PC from MAS repo run:\n"
        f"  python scripts/mqtt_tunnel_cloudflare_apply.py\n"
        f"(sets tunnel hostname -> http://{host}:9001). UFW on broker: allow 9001/tcp from Sandbox 192.168.0.187 only.\n"
    )
    return 0 if code == 0 else code


if __name__ == "__main__":
    raise SystemExit(main())
