"""In-memory registry of the device's identity, latest telemetry, and side link state.

Hydrated from Side A HELLO frames; updated on each TELEMETRY/EVENT. Used by
the HTTP routes and the MQTT publisher to assemble the canonical device
record.
"""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import TYPE_CHECKING, Any

import structlog

if TYPE_CHECKING:
    from mycobrain_agent.adapters.base import Adapter
    from mycobrain_agent.config import Settings
    from mycobrain_agent.core.mdp_codec import MdpFrame

log = structlog.get_logger("registry")


@dataclass
class SideLink:
    linked: bool = False
    fw_version: str | None = None
    role: str | None = None
    last_seen_ts: float = 0.0
    capabilities: dict[str, Any] = field(default_factory=dict)


@dataclass
class DeviceRecord:
    device_id: str | None = None
    side_a: SideLink = field(default_factory=SideLink)
    side_b: SideLink = field(default_factory=SideLink)
    latest_telemetry: dict[str, Any] | None = None
    latest_telemetry_ts: float = 0.0
    transports: dict[str, bool] = field(default_factory=dict)
    estop_latched: bool = False
    audit_tail_id: int = 0


class DeviceRegistry:
    def __init__(self, adapter: "Adapter", settings: "Settings") -> None:
        self.adapter = adapter
        self.settings = settings
        self.record = DeviceRecord()
        self.started_at = time.time()

    @property
    def device_id(self) -> str:
        """Canonical id: from Side A HELLO if known, else falls back to nickname."""
        return self.record.device_id or f"mycobrain-{self.settings.device_nickname}"

    async def on_mdp_frame(self, frame: "MdpFrame", leg: str) -> None:
        from mycobrain_agent.core.mdp_codec import (
            EP_SIDE_A,
            EP_SIDE_B,
            MDP_EVENT,
            MDP_HELLO,
            MDP_TELEMETRY,
        )

        link = self.record.side_a if frame.header.src == EP_SIDE_A else self.record.side_b
        link.linked = True
        link.last_seen_ts = time.time()

        if frame.header.msg_type == MDP_HELLO:
            link.fw_version = frame.payload.get("firmware_version") or frame.payload.get("fw_version")
            link.role = frame.payload.get("role")
            link.capabilities = {k: v for k, v in frame.payload.items() if k not in {"role", "firmware_version", "fw_version"}}
            if frame.header.src == EP_SIDE_A and not self.record.device_id:
                # First HELLO from Side A → its identity wins
                self.record.device_id = frame.payload.get("device_id") or self._derive_device_id(link.role)
                log.info("device_id_assigned", device_id=self.record.device_id, role=link.role)
        elif frame.header.msg_type == MDP_TELEMETRY:
            self.record.latest_telemetry = frame.payload
            self.record.latest_telemetry_ts = link.last_seen_ts
        elif frame.header.msg_type == MDP_EVENT:
            kind = frame.payload.get("event")
            if kind == "estop":
                self.record.estop_latched = True
            elif kind == "clear_estop":
                self.record.estop_latched = False
            elif kind == "transport_status":
                self.record.transports = {
                    k.replace("_ready", ""): v
                    for k, v in frame.payload.items()
                    if k.endswith("_ready")
                }

    def _derive_device_id(self, role: str | None) -> str:
        # Default scheme: ``mycobrain-{role}-{nickname}``. Replace with proper
        # provisioning when the pairing flow lands.
        role_part = (role or "unknown").replace(" ", "_")
        return f"mycobrain-{role_part}-{self.settings.device_nickname}"

    def status(self) -> dict[str, Any]:
        now = time.time()
        return {
            "device_id": self.device_id,
            "host_kind": self.adapter.host_info().kind,
            "agent_version": __import__("mycobrain_agent").__version__,
            "uptime_s": int(now - self.started_at),
            "side_a": _link_summary(self.record.side_a, now),
            "side_b": _link_summary(self.record.side_b, now),
            "estop_latched": self.record.estop_latched,
            "transports": self.record.transports,
        }

    def info(self) -> dict[str, Any]:
        host = self.adapter.host_info()
        return {
            "device_id": self.device_id,
            "nickname": self.settings.device_nickname,
            "host_kind": host.kind,
            "host_model": host.model,
            "host_arch": host.arch,
            "agent_version": __import__("mycobrain_agent").__version__,
            "side_a": {
                "fw_version": self.record.side_a.fw_version,
                "role": self.record.side_a.role,
                "capabilities": self.record.side_a.capabilities,
            },
            "side_b": {
                "fw_version": self.record.side_b.fw_version,
                "capabilities": self.record.side_b.capabilities,
            },
            "capabilities": host.capabilities + ["mdp_command", "telemetry_stream", "frame_tail"],
        }


def _link_summary(link: SideLink, now: float) -> dict[str, Any]:
    return {
        "linked": link.linked,
        "last_seen_ms_ago": int((now - link.last_seen_ts) * 1000) if link.last_seen_ts else None,
        "fw_version": link.fw_version,
        "role": link.role,
    }
