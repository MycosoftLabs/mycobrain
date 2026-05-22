"""HTTP client for OpenClaw at 127.0.0.1:8000.

The agent is the only thing that talks to OpenClaw. Operator → website
→ agent → OpenClaw. Full sequence in
``docs/OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md``.
"""

from __future__ import annotations

import asyncio
import json
import os
import time
from pathlib import Path
from typing import TYPE_CHECKING, Any

import httpx
import structlog

if TYPE_CHECKING:
    from mycobrain_agent.config import Settings

log = structlog.get_logger("openclaw")


_ACTION_MAP: dict[str, tuple[str, str]] = {
    # action → (method, path)
    "open":        ("POST", "/gripper/open"),
    "close":       ("POST", "/gripper/close"),
    "home":        ("POST", "/motion/home"),
    "move_to":     ("POST", "/motion/move"),
    "grasp":       ("POST", "/tasks/grasp"),
    "release":     ("POST", "/tasks/release"),
    "calibrate":   ("POST", "/maintenance/calibrate"),
    "estop":       ("POST", "/safety/estop"),
    "clear_estop": ("POST", "/safety/clear_estop"),
}


class OpenClawClient:
    def __init__(self, settings: "Settings") -> None:
        self.settings = settings
        self._estop_latched = False
        self._action_lock = asyncio.Lock()
        self._audit_id = int(time.time())
        self._audit_path = Path(settings.openclaw_audit_path)
        try:
            self._audit_path.parent.mkdir(parents=True, exist_ok=True)
        except OSError:
            pass

    @property
    def available(self) -> bool:
        return self.settings.openclaw_enabled

    async def status(self) -> dict[str, Any]:
        if not self.available:
            return {"available": False}
        async with httpx.AsyncClient(
            timeout=self.settings.openclaw_timeout_ms / 1000,
            headers=self._headers(),
        ) as client:
            try:
                resp = await client.get(f"{self.settings.openclaw_base_url}/status")
                if resp.status_code >= 400:
                    return {"available": True, "ready": False, "error": resp.text[:200]}
                return {"available": True, "ready": True, **resp.json()}
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
        if action not in _ACTION_MAP:
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
            method, path = _ACTION_MAP[action]
            url = self.settings.openclaw_base_url + path
            started = time.time()
            self._audit({"id": audit_id, "phase": "started", "ts": _now_iso()})
            try:
                async with httpx.AsyncClient(
                    timeout=self.settings.openclaw_timeout_ms / 1000,
                    headers=self._headers(),
                ) as client:
                    resp = await client.request(method, url, json=params)
                    completed_at = _now_iso()
                    if resp.status_code >= 400:
                        self._audit(
                            {
                                "id": audit_id,
                                "phase": "failed",
                                "ts": completed_at,
                                "status": resp.status_code,
                                "body": resp.text[:200],
                            }
                        )
                        return {
                            "ok": False,
                            "request_id": request_id,
                            "audit_id": audit_id,
                            "status": resp.status_code,
                            "error": resp.text[:200],
                        }
                    result = _safe_json(resp)
                    self._audit(
                        {
                            "id": audit_id,
                            "phase": "completed",
                            "ts": completed_at,
                            "result": result,
                        }
                    )
                    if action == "estop":
                        self._estop_latched = True
                    if action == "clear_estop":
                        self._estop_latched = False
                    return {
                        "ok": True,
                        "request_id": request_id,
                        "audit_id": audit_id,
                        "started_at": _to_iso(started),
                        "completed_at": completed_at,
                        "result": result,
                    }
            except httpx.HTTPError as exc:
                self._audit(
                    {
                        "id": audit_id,
                        "phase": "failed",
                        "ts": _now_iso(),
                        "error": str(exc),
                    }
                )
                raise OpenClawUnreachable(str(exc)) from exc

    def _headers(self) -> dict[str, str]:
        headers = {"Content-Type": "application/json"}
        if self.settings.openclaw_api_key:
            headers["X-API-Key"] = self.settings.openclaw_api_key
        return headers

    def _audit(self, record: dict[str, Any]) -> None:
        try:
            with self._audit_path.open("a", encoding="utf-8") as f:
                f.write(json.dumps(record) + "\n")
        except OSError as exc:
            log.warning("audit_write_failed", error=str(exc))

    def _next_audit_id(self) -> int:
        self._audit_id += 1
        return self._audit_id


class OpenClawUnavailable(RuntimeError):
    """OpenClaw is not enabled or not reachable on startup."""


class OpenClawUnreachable(RuntimeError):
    """OpenClaw was enabled but the HTTP call failed."""


class OpenClawLocked(RuntimeError):
    """An estop is latched; only clear_estop is allowed."""


def _safe_json(resp: httpx.Response) -> Any:
    try:
        return resp.json()
    except json.JSONDecodeError:
        return {"raw": resp.text[:500]}


def _now_iso() -> str:
    import datetime as _dt

    return _dt.datetime.utcnow().replace(microsecond=0).isoformat() + "Z"


def _to_iso(epoch: float) -> str:
    import datetime as _dt

    return _dt.datetime.utcfromtimestamp(epoch).replace(microsecond=0).isoformat() + "Z"
