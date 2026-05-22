"""OpenClaw client — sends MDP claw commands to Side A via the serial bridge.

The earlier draft of this module assumed OpenClaw was a separate REST service
at ``http://127.0.0.1:8000``. The actual implementation in the
``claude/integrate-seeed-claw-SPwlV`` branch puts the claw control in Side A
firmware as MDP command IDs ``0x0030``–``0x003F`` (see
``firmware/common/mdp_claw.h``). This module now speaks that.

The Node.js ``openclaw`` daemon is a parallel UX layer for voice/chat — it is
not on the agent's critical path. The agent and the daemon both end up
funneling claw actions through the MDP rail, just from different front-doors.
Coordination is at the serial-port level: only one process owns
``/dev/ttyTHS1`` at a time. The recommended setup is agent-as-owner; see
``docs/OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md`` for the arbitration plan.

Full sequence and the MDP claw command table are in
``docs/OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md``.
"""

from __future__ import annotations

import asyncio
import json
import time
from pathlib import Path
from typing import TYPE_CHECKING, Any

import structlog

if TYPE_CHECKING:
    from mycobrain_agent.config import Settings
    from mycobrain_agent.core.serial_bridge import SerialBridge

log = structlog.get_logger("openclaw")


# Action vocabulary the HTTP API exposes → MDP command names the firmware understands.
# The firmware-level IDs (0x0030+) are handled inside the serial bridge / Side A.
_ACTION_TO_CMD: dict[str, str] = {
    "grip":         "claw_grip",
    "release":      "claw_release",
    "position":     "claw_position",
    "status":       "claw_status",
    "calibrate":    "claw_calibrate",
    "estop":        "estop",          # cross-cutting Side A
    "clear_estop":  "clear_estop",
}

# Old action names from the May 19 first draft, retained as a soft alias so
# NatureOS UIs that hadn't updated yet get a clear 405 with a hint.
_RETIRED_ACTIONS: dict[str, str] = {
    "open":   "release",
    "close":  "grip",
    "home":   "position",            # call position with the release angle
    "move_to": "position",
    "grasp":  "grip",
}


class OpenClawClient:
    """Owns the agent-side OpenClaw state.

    Talks to Side A via the shared ``SerialBridge`` — does NOT open the serial
    port itself. The bridge handles single-flight, MDP framing, ACKs, and
    rolling-tail recording.
    """

    def __init__(self, settings: "Settings", serial_bridge: "SerialBridge | None" = None) -> None:
        self.settings = settings
        self._bridge = serial_bridge
        self._estop_latched = False
        self._action_lock = asyncio.Lock()
        self._audit_id = int(time.time())
        self._audit_path = Path(settings.openclaw_audit_path)
        try:
            self._audit_path.parent.mkdir(parents=True, exist_ok=True)
        except OSError:
            pass
        self._last_status: dict[str, Any] | None = None
        self._last_status_ts: float = 0.0

    def attach_bridge(self, bridge: "SerialBridge") -> None:
        """Wire the serial bridge after both objects exist."""
        self._bridge = bridge

    @property
    def available(self) -> bool:
        """True if the agent CAN issue claw commands.

        We treat OpenClaw as "available" whenever the serial bridge has a Side A
        link. There is no separate "OpenClaw enabled" knob — the firmware either
        has the claw wired up or it doesn't, and `claw_status` tells us.
        """
        if not self.settings.openclaw_enabled:
            return False
        if self._bridge is None:
            return False
        # The registry's side_a.linked flag is the source of truth.
        return True

    async def status(self) -> dict[str, Any]:
        if not self.available:
            return {"available": False}
        # Cheap caching: re-poll claw_status at most every 2s.
        now = time.time()
        if self._last_status and (now - self._last_status_ts) < 2.0:
            return self._last_status
        try:
            frame = await self._bridge.send_command(  # type: ignore[union-attr]
                target="side_a",
                cmd="claw_status",
                params={},
                ack_requested=True,
                timeout_ms=2000,
            )
            payload = (frame.payload if frame else {}) or {}
            status = {
                "available": True,
                "ready": payload.get("calibrated", False),
                "position": payload.get("position"),
                "is_closed": payload.get("is_closed"),
                "force_adc": payload.get("force_adc"),
                "mode": payload.get("mode"),
                "calibrated": payload.get("calibrated"),
                "estop_latched": self._estop_latched,
            }
            self._last_status = status
            self._last_status_ts = now
            return status
        except Exception as exc:  # noqa: BLE001
            return {"available": True, "ready": False, "error": str(exc)}

    async def action(
        self,
        action: str,
        params: dict[str, Any],
        request_id: str,
        user_subject: str,
    ) -> dict[str, Any]:
        if not self.available:
            raise OpenClawUnavailable
        if action in _RETIRED_ACTIONS:
            new_action = _RETIRED_ACTIONS[action]
            raise OpenClawRetired(f"action {action!r} retired; use {new_action!r}")
        if action not in _ACTION_TO_CMD:
            raise ValueError(f"unknown openclaw action: {action!r}")
        if self._estop_latched and action != "clear_estop":
            raise OpenClawLocked

        async with self._action_lock:
            audit_id = self._next_audit_id()
            self._audit(
                {
                    "id": audit_id,
                    "phase": "received",
                    "ts": _now_iso(),
                    "request_id": request_id,
                    "user_subject": user_subject,
                    "action": action,
                    "params": params,
                }
            )
            started = time.time()
            self._audit({"id": audit_id, "phase": "started", "ts": _now_iso()})
            cmd = _ACTION_TO_CMD[action]
            try:
                frame = await self._bridge.send_command(  # type: ignore[union-attr]
                    target="side_a",
                    cmd=cmd,
                    params=params,
                    ack_requested=True,
                    timeout_ms=3000,
                )
                completed_at = _now_iso()
                result = (frame.payload if frame else {}) or {}
                ok = bool(result.get("success", True))
                phase = "completed" if ok else "failed"
                self._audit(
                    {
                        "id": audit_id,
                        "phase": phase,
                        "ts": completed_at,
                        "result": result,
                    }
          