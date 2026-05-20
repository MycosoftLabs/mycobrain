"""FastAPI app on port 8787.

Implements the canonical Agent HTTP API from
``docs/PORT_8787_HTTP_API_SPEC_MAY19_2026.md``. Routes are grouped by concern
into ``routes/`` modules but kept inlined here for the v1 scaffold to keep
the surface area easy to read.
"""

from __future__ import annotations

import asyncio
import json
import time
from typing import TYPE_CHECKING, Any

from fastapi import Depends, FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse, RedirectResponse

from mycobrain_agent.http.auth_middleware import require_auth

if TYPE_CHECKING:
    from mycobrain_agent.config import Settings
    from mycobrain_agent.core.mqtt_client import MqttClient
    from mycobrain_agent.core.registry import DeviceRegistry
    from mycobrain_agent.core.serial_bridge import SerialBridge
    from mycobrain_agent.openclaw.client import OpenClawClient


def build_app(
    *,
    settings: "Settings",
    registry: "DeviceRegistry",
    serial_bridge: "SerialBridge",
    mqtt: "MqttClient",
    openclaw: "OpenClawClient",
) -> FastAPI:
    app = FastAPI(
        title="mycobrain-agent",
        version="1.0.0",
        description="Unified host agent for MycoBrain devices. See docs/PORT_8787_HTTP_API_SPEC.",
    )

    # Stash deps on app.state so route handlers can reach them.
    app.state.settings = settings
    app.state.registry = registry
    app.state.serial_bridge = serial_bridge
    app.state.mqtt = mqtt
    app.state.openclaw = openclaw

    @app.get("/healthz")
    async def healthz() -> dict[str, Any]:
        return {"ok": True}

    @app.get("/readyz")
    async def readyz() -> dict[str, Any]:
        record = registry.record
        if not record.side_a.linked:
            return JSONResponse({"ready": False, "reason": "side_a_unlinked"}, status_code=503)
        return {"ready": True}

    @app.get("/status")
    async def get_status() -> dict[str, Any]:
        oc = await openclaw.status()
        status = registry.status()
        status["openclaw"] = oc
        status["mqtt"] = {"connected": True, "broker": settings.mqtt_url}  # paho keeps reconnecting
        return status

    @app.get("/info")
    async def get_info() -> dict[str, Any]:
        info = registry.info()
        info["openclaw"] = {
            "available": openclaw.available,
            "endpoint": settings.openclaw_base_url if openclaw.available else None,
        }
        return info

    @app.get("/telemetry/latest")
    async def telemetry_latest() -> dict[str, Any]:
        if registry.record.latest_telemetry is None:
            raise HTTPException(status_code=204, detail="no_telemetry_yet")
        return {
            "device_id": registry.device_id,
            "captured_at": _iso(registry.record.latest_telemetry_ts),
            "sensors": registry.record.latest_telemetry,
        }

    @app.post("/command", dependencies=[Depends(require_auth)])
    async def post_command(body: dict[str, Any]) -> dict[str, Any]:
        target = body.get("target")
        cmd = body.get("cmd")
        params = body.get("params") or {}
        ack = bool(body.get("ack_requested", True))
        timeout_ms = int(body.get("timeout_ms", 2000))
        if target not in {"side_a", "side_b"}:
            raise HTTPException(status_code=400, detail="bad_target")
        if not cmd:
            raise HTTPException(status_code=400, detail="missing_cmd")
        try:
            frame = await serial_bridge.send_command(target, cmd, params, ack, timeout_ms)
        except asyncio.TimeoutError:
            raise HTTPException(status_code=504, detail="ack_timeout")
        except RuntimeError as exc:
            raise HTTPException(status_code=503, detail=str(exc))
        response: dict[str, Any] = {"ok": True}
        if frame is not None:
            response["ack"] = {
                "received_at": _iso(time.time()),
                "success": bool(frame.payload.get("success", True)),
                "message": frame.payload.get("message"),
            }
            response["seq"] = frame.header.seq
        return response

    @app.get("/openclaw/status")
    async def openclaw_status() -> dict[str, Any]:
        return await openclaw.status()

    @app.post("/openclaw/action", dependencies=[Depends(require_auth)])
    async def openclaw_action(body: dict[str, Any]) -> dict[str, Any]:
        action = body.get("action")
        params = body.get("params") or {}
        request_id = body.get("request_id") or f"agent-{int(time.time() * 1000)}"
        user_subject = body.get("user_subject", "unknown")
        try:
            result = await openclaw.action(
                action=action,
                params=params,
                request_id=request_id,
                user_subject=user_subject,
            )
        except ValueError as exc:
            raise HTTPException(status_code=400, detail=str(exc))
        except RuntimeError as exc:
            kind = type(exc).__name__
            if kind == "OpenClawUnavailable":
                raise HTTPException(status_code=409, detail="openclaw_unavailable")
            if kind == "OpenClawLocked":
                raise HTTPException(status_code=423, detail="estop_latched")
            raise HTTPException(status_code=502, detail=str(exc))
        # fan-out to MQTT
        await mqtt.publish_openclaw_state(await openclaw.status())
        return result

    @app.get("/mdp/frames")
    async def mdp_frames(since: int | None = None, limit: int = 200) -> list[dict[str, Any]]:
        out = []
        for rec in list(serial_bridge.frame_tail)[-max(limit, 1):]:
            if since is not None and rec.seq <= since:
                continue
            out.append(
                {
                    "dir": rec.direction,
                    "type": rec.type,
                    "src": f"0x{rec.src:02x}",
                    "dst": f"0x{rec.dst:02x}",
                    "seq": rec.seq,
                    "payload": rec.payload,
                    "ts": _iso(rec.ts),
                }
            )
        return out

    @app.websocket("/ws/telemetry")
    async def ws_telemetry(websocket: WebSocket) -> None:
        await websocket.accept()
        try:
            last_seq = 0
            while True:
                # Push the newest frames since last_seq
                new_tail = [r for r in serial_bridge.frame_tail if r.seq > last_seq]
                for rec in new_tail:
                    last_seq = max(last_seq, rec.seq)
                    await websocket.send_text(
                        json.dumps(
                            {
                                "kind": "mdp_frame",
                                "direction": rec.direction,
                                "type": rec.type,
                                "src": f"0x{rec.src:02x}",
                                "dst": f"0x{rec.dst:02x}",
                                "seq": rec.seq,
                                "payload": rec.payload,
                            }
                        )
                    )
                if registry.record.latest_telemetry is not None:
                    await websocket.send_text(
                        json.dumps(
                            {
                                "kind": "telemetry",
                                "device_id": registry.device_id,
                                "captured_at": _iso(registry.record.latest_telemetry_ts),
                                "sensors": registry.record.latest_telemetry,
                            }
                        )
                    )
                await asyncio.sleep(0.5)
        except WebSocketDisconnect:
            return

    @app.post("/pair")
    async def pair(body: dict[str, Any]) -> dict[str, Any]:
        # Placeholder — real pairing wires up key exchange, see
        # docs/NATUREOS_DEVICES_INTEGRATION_MAY19_2026.md
        return {
            "device_id": registry.device_id,
            "host_kind": registry.info()["host_kind"],
            "agent_pubkey": "ed25519:STUB",
            "signed_nonce": "STUB",
            "jwt": "STUB.JWT.TOKEN",
            "note": "pairing not yet implemented in v1 scaffold",
        }

    # --- Backwards-compat redirects from old ports/paths ---
    @app.get("/health")
    async def health_legacy() -> RedirectResponse:
        return RedirectResponse(url="/healthz", status_code=308)

    @app.get("/side-a/command")
    @app.get("/side-b/command")
    async def legacy_command(request: Request) -> RedirectResponse:
        return RedirectResponse(url="/command", status_code=308)

    return app


def _iso(epoch: float) -> str:
    import datetime as _dt

    return _dt.datetime.utcfromtimestamp(epoch).replace(microsecond=0).isoformat() + "Z"
