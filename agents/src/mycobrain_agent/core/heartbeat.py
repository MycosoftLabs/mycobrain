"""Heartbeat to MAS at /api/devices/heartbeat every 30s, plus presence publish."""

from __future__ import annotations

import asyncio
from typing import TYPE_CHECKING

import httpx
import structlog

if TYPE_CHECKING:
    from mycobrain_agent.config import Settings
    from mycobrain_agent.core.registry import DeviceRegistry

log = structlog.get_logger("heartbeat")


class HeartbeatService:
    def __init__(self, registry: "DeviceRegistry", settings: "Settings") -> None:
        self.registry = registry
        self.settings = settings
        self._interval_s = 30.0

    async def run(self) -> None:
        async with httpx.AsyncClient(timeout=10.0) as client:
            while True:
                try:
                    body = {
                        **self.registry.status(),
                        "ts": _now_iso(),
                        "agent_url": f"http://{self.settings.host_ip}:{self.settings.http_port}"
                        if self.settings.host_ip
                        else None,
                    }
                    resp = await client.post(self.settings.mas_heartbeat_url, json=body)
                    if resp.status_code >= 400:
                        log.warning(
                            "heartbeat_rejected",
                            status=resp.status_code,
                            body=resp.text[:200],
                        )
                except Exception as exc:  # noqa: BLE001
                    log.warning("heartbeat_failed", error=str(exc))
                await asyncio.sleep(self._interval_s)


def _now_iso() -> str:
    import datetime as _dt

    return _dt.datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
