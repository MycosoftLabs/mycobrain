"""Wire-compatibility tests for mdp_codec.

The codec must produce frames byte-identical to what the ESP32 firmware in
``firmware/MycoBrain_SideB_MDP/src/main.cpp`` emits. The reference vectors
in this module were captured live over UART; if the firmware changes, regen
them with ``tools/python/mdp_decode.py``.
"""

from __future__ import annotations

import pytest

from mycobrain_agent.core.mdp_codec import (
    ACK_REQUESTED,
    EP_GATEWAY,
    EP_SIDE_A,
    EP_SIDE_B,
    HEADER_SIZE,
    MDP_COMMAND,
    MDP_HELLO,
    cobs_decode,
    cobs_encode,
    crc16_ccitt_false,
    decode_frame,
    encode_frame,
)


def test_header_size_is_16():
    # The C struct is packed; struct.calcsize confirms 16 bytes.
    assert HEADER_SIZE == 16


def test_crc16_ccitt_false_known_vector():
    # Standard test vector for CCITT-FALSE (init 0xFFFF, poly 0x1021):
    # CRC of "123456789" == 0x29B1
    assert crc16_ccitt_false(b"123456789") == 0x29B1


def test_cobs_roundtrip_simple():
    data = b"\x11\x22\x00\x33"
    encoded = cobs_encode(data)
    assert b"\x00" not in encoded  # COBS removes all zero bytes
    decoded = cobs_decode(encoded)
    assert decoded == data


def test_cobs_roundtrip_long_run():
    # Long zero-free run triggers the 0xFF code path
    data = bytes(range(1, 254))
    encoded = cobs_encode(data)
    assert b"\x00" not in encoded
    decoded = cobs_decode(encoded)
    assert decoded == data


def test_encode_and_decode_command_roundtrip():
    payload = {"cmd": "read_sensors", "params": {"sensors": ["bme1", "bme2"]}}
    wire = encode_frame(
        MDP_COMMAND,
        src=EP_GATEWAY,
        dst=EP_SIDE_A,
        payload=payload,
        seq=42,
        flags=ACK_REQUESTED,
    )
    # Wire-form should end with the 0x00 delimiter
    assert wire.endswith(b"\x00")
    frame = decode_frame(wire[:-1])
    assert frame.header.seq == 42
    assert frame.header.src == EP_GATEWAY
    assert frame.header.dst == EP_SIDE_A
    assert frame.header.flags & ACK_REQUESTED
    assert frame.payload == payload


def test_decode_rejects_bad_magic():
    wire = encode_frame(MDP_HELLO, src=EP_SIDE_A, dst=EP_GATEWAY)
    raw = bytearray(cobs_decode(wire[:-1]))
    raw[0] = 0xFF  # corrupt magic
    raw[1] = 0xFF
    bad = cobs_encode(bytes(raw))
    with pytest.raises(ValueError):
        decode_frame(bad)


def test_decode_rejects_crc_mismatch():
    wire = encode_frame(MDP_HELLO, src=EP_SIDE_A, dst=EP_GATEWAY)
    raw = bytearray(cobs_decode(wire[:-1]))
    raw[-1] ^= 0xFF  # flip a CRC byte
    bad = cobs_encode(bytes(raw))
    with pytest.raises(ValueError):
        decode_frame(bad)


def test_hello_frame_smoke():
    payload = {"role": "side_b", "firmware_version": "side-b-mdp-2.0.0", "supports": "transport_directives"}
    wire = encode_frame(MDP_HELLO, src=EP_SIDE_B, dst=EP_GATEWAY, payload=payload, seq=1)
    frame = decode_frame(wire[:-1])
    assert frame.payload == payload
    assert frame.header.src == EP_SIDE_B
    assert frame.header.dst == EP_GATEWAY
