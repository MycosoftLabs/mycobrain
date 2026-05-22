"""Adapter contract.

Every host-specific adapter implements this Protocol. The rest of the agent
(serial bridge, MQTT, HTTP server, OpenClaw client) is hardware-agnostic and
only talks to the adapter through these methods.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import TYPE_CHECKING, Literal, Protocol

if TYPE_CHECKING:
    from mycobrain_agent.config import Settings

HostKind = Literal["jetson_orin", "jetson_legacy", "raspberry_pi", "standalone"]


@dataclass
class HostInfo:
    kind: HostKind
    model: str
    arch: str
    has_cuda: bool = False
    has_gpio: bool = False
    capabilities: list[str] = field(default_factory=list)


@dataclass
class SerialEndpoints:
    side_a: str | None
    side_b: str | None  # None for single-MCU variants (e.g. some Pi or standalone)
    baud: int = 115200


class Adapter(Protocol):
    """Hardware abstraction for the host the agent runs on.

    Implementations must be thin — anything non-host-specific belongs in
    ``mycobrain_agent.core``.
    """

    settings: "Settings"

    def host_info(self) -> HostInfo:
        """Return identity + capability info, used in /info responses."""
        ...

    def discover_serial_ports(self) -> SerialEndpoints:
        """Return the side_a / side_b port paths, falling back to env hints."""
        ...

    def supports_legacy_json(self) -> bool:
        """True if this adapter should also accept the pre-MDP JSON-line protocol."""
        ...

    async def on_estop(self) -> None:
        """Optional host-side response to an estop event (e.g. cut MOSFET on Pi)."""
        ...

    async def shutdown(self) -> None:
        """Release host-specific resources (close GPIO, etc.)."""
        ...
