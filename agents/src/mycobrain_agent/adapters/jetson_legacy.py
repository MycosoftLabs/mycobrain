"""Older Jetsons (Nano 4GB, TX2, pre-Orin).

Reused for device #3 (the dead one). Same UART conventions as Orin where
present, but capabilities advertise no CUDA inference (model coverage is
limited; the gateway path is preferred for these).
"""

from __future__ import annotations

import platform
from pathlib import Path

from mycobrain_agent.adapters.base import HostInfo, SerialEndpoints
from mycobrain_agent.config import Settings


class JetsonLegacyAdapter:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings

    def host_info(self) -> HostInfo:
        return HostInfo(
            kind="jetson_legacy",
            model=_read_model(),
            arch=platform.machine(),
            has_cuda=False,  # Conservative — flip on per box if you bench inference
            has_gpio=False,
            capabilities=["mdp_command", "openclaw_control", "gateway_router"],
        )

    def discover_serial_ports(self) -> SerialEndpoints:
        s = self.settings
        side_a = s.side_a_port or _first_existing(["/dev/ttyTHS1", "/dev/ttyUSB0"])
        side_b = s.side_b_port or _first_existing(["/dev/ttyTHS2", "/dev/ttyUSB1"])
        return SerialEndpoints(side_a=side_a, side_b=side_b, baud=s.serial_baud)

    def supports_legacy_json(self) -> bool:
        return False

    async def on_estop(self) -> None:
        return None

    async def shutdown(self) -> None:
        return None


def _read_model() -> str:
    try:
        release = Path("/etc/nv_tegra_release").read_text(errors="ignore")
        return f"Jetson Legacy ({release.splitlines()[0].strip()})" if release else "Jetson Legacy"
    except OSError:
        return "Jetson Legacy"


def _first_existing(paths: list[str]) -> str | None:
    for p in paths:
        if Path(p).exists():
            return p
    return None
