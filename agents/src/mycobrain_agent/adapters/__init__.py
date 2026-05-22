"""Adapter selection.

The adapter encapsulates everything host-specific: serial port discovery,
optional second MCU (Side B) presence, GPIO availability, hardware capabilities
(CUDA on Jetson, none on Pi), and friendly identity metadata.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from mycobrain_agent.adapters.base import Adapter

if TYPE_CHECKING:
    from mycobrain_agent.config import Settings


def load_adapter(settings: "Settings") -> Adapter:
    kind = settings.adapter
    if kind == "jetson_orin":
        from mycobrain_agent.adapters.jetson_orin import JetsonOrinAdapter

        return JetsonOrinAdapter(settings)
    if kind == "jetson_legacy":
        from mycobrain_agent.adapters.jetson_legacy import JetsonLegacyAdapter

        return JetsonLegacyAdapter(settings)
    if kind == "raspberry_pi":
        from mycobrain_agent.adapters.raspberry_pi import RaspberryPiAdapter

        return RaspberryPiAdapter(settings)
    if kind == "standalone":
        from mycobrain_agent.adapters.standalone import StandaloneAdapter

        return StandaloneAdapter(settings)
    raise ValueError(f"unknown adapter kind: {kind!r}")


__all__ = ["Adapter", "load_adapter"]
