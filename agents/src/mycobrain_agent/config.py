"""Env-driven configuration for mycobrain-agent.

All knobs live in env vars (or `/etc/mycobrain/agent.env`). The installer
scripts populate sensible defaults per host. See `deploy/env/*.example`.
"""

from __future__ import annotations

from typing import Literal

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict

AdapterKind = Literal["jetson_orin", "jetson_legacy", "raspberry_pi", "standalone"]
AuthMode = Literal["none", "pair_token", "jwt"]


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_prefix="MYCOBRAIN_",
        env_file=("/etc/mycobrain/agent.env", ".env"),
        env_file_encoding="utf-8",
        extra="ignore",
    )

    # --- identity / adapter selection ---
    adapter: AdapterKind = "standalone"
    device_nickname: str = "mycobrain"
    host_ip: str | None = None  # auto-detected if unset

    # --- HTTP API ---
    http_host: str = "0.0.0.0"
    http_port: int = 8787
    public_reads: bool = True
    auth_mode: AuthMode = "jwt"
    natureos_jwks_url: str | None = None
    pair_token: str | None = None

    # --- Serial ports ---
    side_a_port: str | None = None
    side_b_port: str | None = None
    serial_baud: int = 115200

    # --- MQTT ---
    mqtt_url: str = "wss://mqtt.mycosoft.com"
    mqtt_username: str = "mycobrain"
    mqtt_password: str | None = None
    mqtt_client_id: str | None = None  # default: f"mycobrain-{adapter}-{nickname}"

    # --- OpenClaw ---
    # The OpenClaw daemon (Node.js, ws://127.0.0.1:18789) is a parallel UX layer
    # — the agent does NOT proxy through it. Claw control is via MDP commands
    # sent through the serial bridge to Side A. See
    # docs/OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md for the full picture.
    openclaw_enabled: bool = True
    openclaw_daemon_ws: str = "ws://127.0.0.1:18789"  # presence probe only
    openclaw_timeout_ms: int = 5000
    openclaw_audit_path: str = "/var/log/mycobrain/openclaw_audit.jsonl"
    # Retired May 21 (kept as comment for any deployment that still reads them):
    # openclaw_base_url: str = "http://127.0.0.1:8000"  -- there is no HTTP service there
    # openclaw_api_key: str | None = None                -- no key needed; serial is local

    # ---