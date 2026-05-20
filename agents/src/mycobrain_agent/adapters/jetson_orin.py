"""Jetson Orin family (Orin Nano, Orin NX, AGX Orin).

Production target for the on-device cortex Mushroom 1 and Hyphae 1 boxes.
Has CUDA, expects two serial ports (Side A on ttyTHS1, Side B on ttyTHS2 by
convention from the deploy docs).
"""

from __future__ import annotations

import platform
from pathlib import Path

from mycobrain_agent.adapters.base import Adapter, HostInfo, SerialEndpoints
from mycobrain_agent.config import Settings


class JetsonOrinAdapter:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings

    def host_info(self) -> HostInfo:
        model = "Jetson Orin (unknown)"
        try:
            release = Path("/etc/nv_tegra_release").read_text(errors="ignore")
            first_line = release.splitlines()[0] if release else ""
            if first_line:
                model = f"Jetson ({first_line.strip()})"
        except OSError:
            pass
        return HostInfo(
            kind="jetson_orin",
            model=model,
            arch=platform.machine(),
            has_cuda=True,
            has_gpio=False,  # Orin Nano GPIO is uncommon for our use; flip if needed
            capabilities=["cuda", "mdp_command", "openclaw_control", "tac_o_inference"],
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


def _first_existing(paths: list[str]) -> str | None:
    for p in paths:
        if Path(p).exists():
            return p
    return None
