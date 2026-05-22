"""Raspberry Pi 4 / 5.

The Pi runs the exact same agent as the Jetsons. Differences:

* Default serial port is the on-board UART (``/dev/serial0`` → ``/dev/ttyAMA0`` or
  ``/dev/ttyS0`` depending on overlay) for Side A. Side B optional.
* No CUDA. The TAC-O / NLM inference modules degrade to no-op stubs.
* GPIO available — surfaced through ``output_control`` with ``target: gpio``.
"""

from __future__ import annotations

import platform
from pathlib import Path

from mycobrain_agent.adapters.base import HostInfo, SerialEndpoints
from mycobrain_agent.config import Settings


class RaspberryPiAdapter:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self._gpio_initialized = False

    def host_info(self) -> HostInfo:
        return HostInfo(
            kind="raspberry_pi",
            model=_pi_model(),
            arch=platform.machine(),
            has_cuda=False,
            has_gpio=True,
            capabilities=["mdp_command", "openclaw_control", "gpio_control"],
        )

    def discover_serial_ports(self) -> SerialEndpoints:
        s = self.settings
        side_a = s.side_a_port or _first_existing(
            ["/dev/serial0", "/dev/ttyAMA0", "/dev/ttyUSB0", "/dev/ttyACM0"]
        )
        side_b = s.side_b_port  # Often single-MCU on Pi; if absent, Side B features disabled.
        return SerialEndpoints(side_a=side_a, side_b=side_b, baud=s.serial_baud)

    def supports_legacy_json(self) -> bool:
        # The Pi variant came from the MDP era; legacy JSON not expected.
        return False

    async def on_estop(self) -> None:
        # If GPIO is wired to a relay, cut it here. The base implementation is
        # a no-op so a misconfigured Pi never bricks an unrelated GPIO.
        return None

    async def shutdown(self) -> None:
        if self._gpio_initialized:
            try:
                import RPi.GPIO as GPIO  # type: ignore

                GPIO.cleanup()
            except Exception:
                pass


def _pi_model() -> str:
    try:
        model = Path("/proc/device-tree/model").read_text(errors="ignore").strip("\x00 \n")
        return model or "Raspberry Pi"
    except OSError:
        return "Raspberry Pi"


def _first_existing(paths: list[str]) -> str | None:
    for p in paths:
        if Path(p).exists():
            return p
    return None
