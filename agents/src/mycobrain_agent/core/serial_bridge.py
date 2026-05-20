"""Serial bridge — talks to Side A (and optionally Side B) over UART.

Owns:
  * Async read loop per port, framed by COBS terminator 0x00
  * MDP frame decode and dispatch into the registry
  * Send queue with single-flight per leg
  * Legacy JSON-line fallback when the adapter advertises it
  * Rolling in-memory MDP frame tail for debug
"""

from __future__ import annotations

import asyncio
import json
import time
from collections import deque
from dataclasses import dataclass
from typing import TYPE_CHECKING, Any

import structlog
from serial import Serial
from serial.serialutil import SerialException

from mycobrain_agent.core.mdp_codec import (
    EP_GATEWAY,
    MDP_COMMAND,
    MDP_HELLO,
    MdpFrame,
    decode_frame,
    encode_frame,
    MSG_TYPE_NAMES,
)

if TYPE_CHECKING:
    from mycobrain_agent.adapters.base import Adapter
    from mycobrain_agent.config import Settings
    from mycobrain_agent.core.registry import DeviceRegistry


log = structlog.get_logger("serial_bridge")


@dataclass
class FrameRecord:
    direction: str  # "rx" | "tx"
    type: str
    src: int
    dst: int
    seq: int
    payload: dict[str, Any]
    ts: float


class SerialBridge:
    def __init__(self, adapter: "Adapter", registry: "DeviceRegistry", settings: "Settings") -> None:
        self.adapter = adapter
        self.registry = registry
        self.settings = settings
        self.endpoints = adapter.discover_serial_ports()
        self._side_a: Serial | None = None
        self._side_b: Serial | None = None
        self._stop = asyncio.Event()
        self._tx_seq = 1
        self._pending_acks: dict[int, asyncio.Future[MdpFrame]] = {}
        self._tx_lock = asyncio.Lock()
        self.frame_tail: deque[FrameRecord] = deque(maxlen=2000)

    async def run(self) -> None:
        await asyncio.gather(
            self._reader(port=self.endpoints.side_a, leg="side_a"),
            self._reader(port=self.endpoints.side_b, leg="side_b"),
        )

    async def stop(self) -> None:
        self._stop.set()
        if self._side_a:
            try:
                self._side_a.close()
            except Exception:
                pass
        if self._side_b:
            try:
                self._side_b.close()
            except Exception:
                pass

    async def _reader(self, port: str | None, leg: str) -> None:
        if not port:
            log.info("serial_skip", leg=leg, reason="no_port_configured")
            return
        backoff = 1.0
        while not self._stop.is_set():
            try:
                ser = Serial(port, baudrate=self.endpoints.baud, timeout=0)
                if leg == "side_a":
                    self._side_a = ser
                else:
                    self._side_b = ser
                log.info("serial_open", leg=leg, port=port)
                backoff = 1.0
                await self._consume(ser, leg)
            except SerialException as exc:
                log.warning("serial_error", leg=leg, port=port, error=str(exc))
                await asyncio.sleep(backoff)
                backoff = min(backoff * 2, 30.0)

    async def _consume(self, ser: Serial, leg: str) -> None:
        buf = bytearray()
        legacy_mode = self.adapter.supports_legacy_json() and leg == "side_a"
        loop = asyncio.get_running_loop()
        while not self._stop.is_set():
            chunk = await loop.run_in_executor(None, ser.read, 4096)
            if not chunk:
                await asyncio.sleep(0.005)
                continue
            buf.extend(chunk)
            # Split on framing terminator 0x00 (MDP) or 0x0a (legacy JSON)
            while True:
                term = -1
                for i, byte in enumerate(buf):
                    if byte == 0x00:
                        term = i
                        break
                    if legacy_mode and byte == 0x0A:
                        term = i
                        break
                if term < 0:
                    break
                raw = bytes(buf[:term])
                del buf[: term + 1]
                if not raw:
                    continue
                # Detect by terminator: 0x00 → MDP, 0x0a → legacy JSON line
                if term < len(buf) + len(raw) and (legacy_mode and raw[:1] == b"{"):
                    self._handle_legacy_json(raw, leg)
                else:
                    try:
                        frame = decode_frame(raw)
                        self._record(direction="rx", frame=frame)
                        await self.registry.on_mdp_frame(frame, leg=leg)
                        self._maybe_resolve_ack(frame)
                    except ValueError as exc:
                        log.warning("frame_decode_error", leg=leg, error=str(exc))

    def _handle_legacy_json(self, raw: bytes, leg: str) -> None:
        try:
            doc = json.loads(raw.decode("utf-8", errors="replace"))
        except json.JSONDecodeError:
            return
        log.info("legacy_json_rx", leg=leg, doc=doc)
        # Translate into a synthetic TELEMETRY MdpFrame and feed to registry
        # so downstream paths are uniform.
        synthetic_payload = {"legacy": True, "data": doc}
        # We can't use a real MdpHeader here without a seq from the device;
        # use a monotonic counter.
        from mycobrain_agent.core.mdp_codec import MDP_TELEMETRY, EP_SIDE_A, MdpHeader

        hdr = MdpHeader(
            magic=0xA15A,
            version=0x01,
            msg_type=MDP_TELEMETRY,
            seq=int(time.time() * 1000) & 0xFFFFFFFF,
            ack=0,
            flags=0,
            src=EP_SIDE_A,
            dst=EP_GATEWAY,
        )
        frame = MdpFrame(header=hdr, payload=synthetic_payload)
        asyncio.create_task(self.registry.on_mdp_frame(frame, leg="side_a"))
        self._record(direction="rx", frame=frame)

    def _record(self, direction: str, frame: MdpFrame) -> None:
        self.frame_tail.append(
            FrameRecord(
                direction=direction,
                type=MSG_TYPE_NAMES.get(frame.header.msg_type, f"0x{frame.header.msg_type:02x}"),
                src=frame.header.src,
                dst=frame.header.dst,
                seq=frame.header.seq,
                payload=frame.payload,
                ts=time.time(),
            )
        )

    def _maybe_resolve_ack(self, frame: MdpFrame) -> None:
        if frame.header.ack and frame.header.ack in self._pending_acks:
            fut = self._pending_acks.pop(frame.header.ack)
            if not fut.done():
                fut.set_result(frame)

    async def send_command(
        self,
        target: str,
        cmd: str,
        params: dict[str, Any] | None = None,
        ack_requested: bool = True,
        timeout_ms: int = 2000,
    ) -> MdpFrame | None:
        from mycobrain_agent.core.mdp_codec import (
            ACK_REQUESTED,
            EP_SIDE_A,
            EP_SIDE_B,
        )

        if target == "side_a":
            ser = self._side_a
            dst = EP_SIDE_A
        elif target == "side_b":
            ser = self._side_b or self._side_a  # Side B may be reached via Side A's UART2
            dst = EP_SIDE_B
        else:
            raise ValueError(f"unknown target: {target}")
        if ser is None:
            raise RuntimeError(f"no serial port open for {target}")
        async with self._tx_lock:
            seq = self._tx_seq
            self._tx_seq = (self._tx_seq + 1) & 0xFFFFFFFF
            payload = {"cmd": cmd, "params": params or {}}
            flags = ACK_REQUESTED if ack_requested else 0
            frame_bytes = encode_frame(
                MDP_COMMAND, src=EP_GATEWAY, dst=dst, payload=payload, seq=seq, flags=flags
            )
            loop = asyncio.get_running_loop()
            fut: asyncio.Future[MdpFrame] | None = None
            if ack_requested:
                fut = loop.create_future()
                self._pending_acks[seq] = fut
            await loop.run_in_executor(None, ser.write, frame_bytes)
            self._record(
                direction="tx",
                frame=MdpFrame(
                    header=_synthetic_header(seq, MDP_COMMAND, dst, flags),
                    payload=payload,
                ),
            )
            if fut is None:
                return None
            try:
                return await asyncio.wait_for(fut, timeout=timeout_ms / 1000)
            except asyncio.TimeoutError:
                self._pending_acks.pop(seq, None)
                raise


def _synthetic_header(seq: int, msg_type: int, dst: int, flags: int):
    from mycobrain_agent.core.mdp_codec import EP_GATEWAY, MdpHeader

    return MdpHeader(
        magic=0xA15A,
        version=0x01,
        msg_type=msg_type,
        seq=seq,
        ack=0,
        flags=flags,
        src=EP_GATEWAY,
        dst=dst,
    )
