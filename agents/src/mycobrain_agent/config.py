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
    openclaw_enabled: bool = True
    openclaw_base_url: str = "http://127.0.0.1:8000"
    openclaw_api_key: str | None = None
    openclaw_timeout_ms: int = 5000
    openclaw_audit_path: str = "/var/log/mycobrain/openclaw_audit.jsonl"

    # --- Upstream ---
    mas_heartbeat_url: str = "https://mycosoft.com/api/devices/heartbeat"
    mindex_telemetry_url: str = "https://mindex.mycosoft.com/api/fci/telemetry"
    nlm_translate_url: str = "https://mycosoft.com/api/translate"

    # --- Observability ---
    log_level: str = "INFO"
    audit_path: str = "/var/log/mycobrain/agent_audit.jsonl"

    # --- Resolved values ---
    @property
    def resolved_mqtt_client_id(self) -> str:
        return self.mqtt_client_id or f"mycobrain-{self.adapter}-{self.device_nickname}"
