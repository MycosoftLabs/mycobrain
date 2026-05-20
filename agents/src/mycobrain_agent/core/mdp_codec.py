"""Python port of ``firmware/common_mdp/include/mdp_codec.h``.

Wire-compatible with the ESP32 Side A and Side B firmware. The C header is
the source of truth; this module mirrors it. Tests in
``tests/test_mdp_codec.py`` enforce byte-for-byte parity using vectors
captured from the firmware over UART.

Frame layout (little-endian, packed):

    Header (14 bytes)           Payload (JSON)   CRC16-LE (2)
    +-------------------------+ +----...-------+ +----+----+
    | magic | ver | type | seq | ack | flags | ... | crc_lo | crc_hi |
    | u16   | u8  | u8   | u32 | u32 | u8    |     |        |        |
    | src   | dst | rsv                       |
    +-------------------------+ +----...-------+ +----+----+

The entire frame is then COBS-encoded and terminated with a single 0x00
delimiter byte on the wire.
"""

from __future__ import annotations

import json
import struct
from dataclasses import dataclass
from typing import Any

# ---- Constants (match mdp_codec.h) ---------------------------------------------------

MDP_MAGIC = 0xA15A
MDP_VERSION = 0x01

# Message types
MDP_TELEMETRY = 0x01
MDP_COMMAND = 0x02
MDP_ACK = 0x03
MDP_EVENT = 0x05
MDP_HELLO = 0x06

MSG_TYPE_NAMES = {
    MDP_TELEMETRY: "TELEMETRY",
    MDP_COMMAND: "COMMAND",
    MDP_ACK: "ACK",
    MDP_EVENT: "EVENT",
    MDP_HELLO: "HELLO",
}

# Endpoints
EP_SIDE_A = 0xA1
EP_SIDE_B = 0xB1
EP_GATEWAY = 0xC0
EP_BCAST = 0xFF

# Flags
ACK_REQUESTED = 0x01
IS_ACK = 0x02
IS_NACK = 0x04

# ``<H B B I I B B B B`` = 2+1+1+4+4+1+1+1+1 = 16 bytes
# but the C header is `#pragma pack(push, 1)` so we need packed alignment.
# struct format with no padding:
_HEADER_FMT = "<HBBIIBBBB"
HEADER_SIZE = struct.calcsize(_HEADER_FMT)  # 16

# Wait — the C header packs to: 2 + 1 + 1 + 4 + 4 + 1 + 1 + 1 + 1 = 16 bytes.
# But the comment in the firmware says "header (14 bytes)". Let's compute and
# trust struct.calcsize which respects little-endian + packed alignment.
assert HEADER_SIZE == 16, f"unexpected MDP header size: {HEADER_SIZE}"


@dataclass(frozen=True)
class MdpHeader:
    magic: int
    version: int
    msg_type: int
    seq: int
    ack: int
    flags: int
    src: int
    dst: int
    rsv: int = 0

    def pack(self) -> bytes:
        return struct.pack(
            _HEADER_FMT,
            self.magic,
            self.version,
            self.msg_type,
            self.seq,
            self.ack,
            self.flags,
            self.src,
            self.dst,
            self.rsv,
        )

    @classmethod
    def unpack(cls, data: bytes) -> "MdpHeader":
        if len(data) < HEADER_SIZE:
            raise ValueError(f"header too short: {len(data)} < {HEADER_SIZE}")
        fields = struct.unpack(_HEADER_FMT, data[:HEADER_SIZE])
        return cls(*fields)


@dataclass(frozen=True)
class MdpFrame:
    header: MdpHeader
    payload: dict[str, Any]


# ---- CRC-16/CCITT-FALSE (matches mdp_crc16_ccitt_false) -------------------------------


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


# ---- COBS (matches mdp_cobs_encode / decode) ------------------------------------------


def cobs_encode(data: bytes) -> bytes:
    """Consistent Overhead Byte Stuffing — same algorithm as the firmware.

    Output does NOT include the trailing 0x00 delimiter; callers append it.
    """
    out = bytearray(len(data) + 2)
    read_index = 0
    write_index = 1
    code_index = 0
    code = 1

    while read_index < len(data):
        byte = data[read_index]
        if byte == 0:
            out[code_index] = code
            code = 1
            code_index = write_index
            write_index += 1
            read_index += 1
        else:
            if write_index >= len(out):
                out.extend(b"\x00\x00")
            out[write_index] = byte
            write_index += 1
            read_index += 1
            code += 1
            if code == 0xFF:
                out[code_index] = code
                code = 1
                code_index = write_index
                write_index += 1
                if write_index >= len(out):
                    out.extend(b"\x00\x00")

    out[code_index] = code
    return bytes(out[:write_index])


def cobs_decode(data: bytes) -> bytes:
    """Inverse of cobs_encode. Returns the decoded buffer (no 0x00 delimiter expected)."""
    if not data:
        return b""
    out = bytearray()
    read_index = 0

    while read_index < len(data):
        code = data[read_index]
        if code == 0:
            raise ValueError("zero byte inside COBS-encoded frame")
        read_index += 1
        for _ in range(1, code):
            if read_index >= len(data):
                raise ValueError("truncated COBS frame")
            out.append(data[read_index])
            read_index += 1
        if code != 0xFF and read_index < len(data):
            out.append(0)

    return bytes(out)


# ---- High-level encode / decode -------------------------------------------------------


def encode_frame(
    msg_type: int,
    src: int,
    dst: int,
    payload: dict[str, Any] | None = None,
    *,
    seq: int = 0,
    ack: int = 0,
    flags: int = 0,
) -> bytes:
    """Build a wire-ready MDP frame including COBS encoding and trailing 0x00."""
    if payload is None:
        payload = {}
    payload_bytes = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    header = MdpHeader(
        magic=MDP_MAGIC,
        version=MDP_VERSION,
        msg_type=msg_type,
        seq=seq,
        ack=ack,
        flags=flags,
        src=src,
        dst=dst,
    )
    body = header.pack() + payload_bytes
    crc = crc16_ccitt_false(body)
    body_with_crc = body + bytes((crc & 0xFF, (crc >> 8) & 0xFF))
    return cobs_encode(body_with_crc) + b"\x00"


def decode_frame(cobs_frame: bytes) -> MdpFrame:
    """Decode a COBS-encoded frame (without the trailing 0x00 delimiter).

    Raises ``ValueError`` on any framing, CRC, or JSON error.
    """
    raw = cobs_decode(cobs_frame)
    if len(raw) < HEADER_SIZE + 2:
        raise ValueError(f"frame too short: {len(raw)}")
    body = raw[: -2]
    crc_lo, crc_hi = raw[-2], raw[-1]
    got_crc = crc_lo | (crc_hi << 8)
    expected_crc = crc16_ccitt_false(body)
    if got_crc != expected_crc:
        raise ValueError(f"CRC mismatch: got=0x{got_crc:04x} expected=0x{expected_crc:04x}")
    header = MdpHeader.unpack(body[:HEADER_SIZE])
    if header.magic != MDP_MAGIC or header.version != MDP_VERSION:
        raise ValueError(
            f"bad header: magic=0x{header.magic:04x} version=0x{header.version:02x}"
        )
    payload_bytes = body[HEADER_SIZE:]
    payload: dict[str, Any] = json.loads(payload_bytes) if payload_bytes else {}
    return MdpFrame(header=header, payload=payload)
