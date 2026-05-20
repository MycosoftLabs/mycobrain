"""Standalone PC over USB-CDC.

For the bench MycoBrain plugged into a developer's machine. Supports BOTH:

* MDP framing (COBS+CRC16) — modern firmware
* Newline-delimited JSON — legacy ``firmware/MycoBrain_SideA/`` build

The serial bridge sniffs the first bytes to decide which path. There is no
Side B in this topology (single-MCU). OpenClaw is typically unavailable.
"""

from __future__ import annotations

import platform

from mycobrain_agent.adapters.base import HostInfo, SerialEndpoints
from mycobrain_agent.config import Settings


class StandaloneAdapter:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings

    def host_info(self) -> HostInfo:
        sysname = platform.system()
        return HostInfo(
            kind="standalone",
            model=f"{sysname} {platform.release()}".strip(),
            arch=platform.machine(),
            has_cuda=False,
            has_gpio=False,
            capabilities=["mdp_command", "legacy_json_bridge"],
        )

    def discover_serial_ports(self) -> SerialEndpoints:
        s = self.settings
        # No defaults — the standalone install script must set MYCOBRAIN_SIDE_A_PORT.
        return SerialEndpoints(side_a=s.side_a_port, side_b=None, baud=s.serial_baud)

    def supports_legacy_json(self) -> bool:
        return True

    async def on_estop(self) -> None:
        return None

    async def shutdown(self) -> None:
        return None
