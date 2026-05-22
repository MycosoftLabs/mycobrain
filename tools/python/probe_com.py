"""Probe a MycoBrain over a serial port.

Auto-detects whether the firmware is MDP (binary, COBS-framed) or legacy
(newline-delimited JSON). Captures the first HELLO + a handful of frames /
lines and prints a clean summary.

Usage:
    python tools/python/probe_com.py COM4
    python tools/python/probe_com.py /dev/ttyACM0
    python tools/python/probe_com.py COM4 --seconds 8 --baud 115200

Requires pyserial:
    pip install pyserial
"""

from __future__ import annotations

import argparse
import json
import os
import struct
import sys
import time
from collections import Counter
from pathlib import Path

try:
    import serial  # type: ignore
except ImportError:
    print("FATAL: pyserial not installed. Run:  pip install pyserial", file=sys.stderr)
    sys.exit(1)

# --- MDP codec (mirrors agents/src/mycobrain_agent/core/mdp_codec.py) ----------

MDP_MAGIC = 0xA15A
MDP_VERSION = 0x01
HEADER_FMT = "<HBBIIBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FMT)
MSG_TYPE_NAMES = {0x01: "TELEMETRY", 0x02: "COMMAND", 0x03: "ACK", 0x05: "EVENT", 0x06: "HELLO"}
ENDPOINT_NAMES = {0xA1: "SIDE_A", 0xB1: "SIDE_B", 0xC0: "GATEWAY", 0xFF: "BCAST"}


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def cobs_decode(data: bytes) -> bytes:
    if not data:
        return b""
    out = bytearray()
    i = 0
    while i < len(data):
        code = data[i]
        if code == 0:
            raise ValueError("zero byte in COBS frame")
        i += 1
        for _ in range(1, code):
            if i >= len(data):
                raise ValueError("truncated COBS")
            out.append(data[i])
            i += 1
        if code != 0xFF and i < len(data):
            out.append(0)
    return bytes(out)


def decode_mdp(cobs_payload: bytes) -> dict | None:
    """Return decoded MDP frame as a dict, or None if it doesn't look like MDP."""
    try:
        raw = cobs_decode(cobs_payload)
    except ValueError:
        return None
    if len(raw) < HEADER_SIZE + 2:
        return None
    body = raw[:-2]
    crc_lo, crc_hi = raw[-2], raw[-1]
    got_crc = crc_lo | (crc_hi << 8)
    if got_crc != crc16_ccitt_false(body):
        return None
    fields = struct.unpack(HEADER_FMT, body[:HEADER_SIZE])
    magic, version, msg_type, seq, ack, flags, src, dst, _rsv = fields
    if magic != MDP_MAGIC or version != MDP_VERSION:
        return None
    payload_bytes = body[HEADER_SIZE:]
    try:
        payload = json.loads(payload_bytes) if payload_bytes else {}
    except json.JSONDecodeError:
        payload = {"<unparseable_json>": payload_bytes.hex()}
    return {
        "type": MSG_TYPE_NAMES.get(msg_type, f"0x{msg_type:02x}"),
        "src": ENDPOINT_NAMES.get(src, f"0x{src:02x}"),
        "dst": ENDPOINT_NAMES.get(dst, f"0x{dst:02x}"),
        "seq": seq,
        "ack": ack,
        "flags": flags,
        "payload": payload,
    }


# --- Main probe ----------------------------------------------------------------


def probe(port: str, baud: int, seconds: float) -> dict:
    summary: dict = {
        "port": port,
        "baud": baud,
        "duration_s": seconds,
        "bytes_total": 0,
        "delimiter_counts": Counter(),
        "looks_like": "unknown",
        "mdp_frames": [],
        "legacy_lines": [],
        "raw_first_64_hex": None,
        "errors": [],
    }

    try:
        ser = serial.Serial(port, baudrate=baud, timeout=0.05)
    except Exception as exc:
        summary["errors"].append(f"open_failed: {exc}")
        return summary

    deadline = time.time() + seconds
    buf = bytearray()
    first_chunk_logged = False

    while time.time() < deadline:
        chunk = ser.read(4096)
        if not chunk:
            time.sleep(0.01)
            continue
        if not first_chunk_logged:
            summary["raw_first_64_hex"] = chunk[:64].hex()
            first_chunk_logged = True
        summary["bytes_total"] += len(chunk)
        buf.extend(chunk)

        while True:
            mdp_term = buf.find(0x00)
            nl_term = buf.find(0x0A)
            terms = [t for t in (mdp_term, nl_term) if t >= 0]
            if not terms:
                break
            term = min(terms)
            piece = bytes(buf[:term])
            term_byte = buf[term]
            del buf[: term + 1]
            if term_byte == 0x00:
                summary["delimiter_counts"]["0x00"] += 1
            else:
                summary["delimiter_counts"]["0x0a"] += 1
            if not piece:
                continue
            # Try MDP first
            decoded = decode_mdp(piece)
            if decoded is not None:
                summary["mdp_frames"].append(decoded)
            else:
                # Try legacy JSON line
                try:
                    text = piece.decode("utf-8", errors="replace").strip()
                    if not text:
                        continue
                    if text.startswith("{"):
                        try:
                            obj = json.loads(text)
                            summary["legacy_lines"].append({"json": obj})
                        except json.JSONDecodeError:
                            summary["legacy_lines"].append({"raw": text})
                    else:
                        summary["legacy_lines"].append({"raw": text})
                except Exception as exc:
                    summary["errors"].append(f"line_decode: {exc}")

    try:
        ser.close()
    except Exception:
        pass

    # Verdict
    mdp = len(summary["mdp_frames"])
    legacy = len(summary["legacy_lines"])
    if mdp and mdp >= legacy:
        summary["looks_like"] = "mdp"
    elif legacy and legacy > mdp:
        summary["looks_like"] = "legacy_json"
    elif summary["bytes_total"] == 0:
        summary["looks_like"] = "silent_or_misconfigured"
    else:
        summary["looks_like"] = "unrecognized"

    return summary


def render(summary: dict) -> None:
    print("=" * 78)
    print(f"MycoBrain probe — {summary['port']} @ {summary['baud']} for {summary['duration_s']}s")
    print("=" * 78)
    print(f"Bytes received: {summary['bytes_total']}")
    print(f"Delimiter counts: {dict(summary['delimiter_counts'])}")
    print(f"First 64 bytes (hex): {summary['raw_first_64_hex']}")
    print(f"Verdict: {summary['looks_like'].upper()}")
    if summary["errors"]:
        print(f"Errors: {summary['errors']}")
    print()

    if summary["mdp_frames"]:
        print(f"MDP frames: {len(summary['mdp_frames'])}")
        type_counts = Counter(f["type"] for f in summary["mdp_frames"])
        print(f"  Frame types: {dict(type_counts)}")
        # Show the first HELLO if any
        hello = next((f for f in summary["mdp_frames"] if f["type"] == "HELLO"), None)
        if hello:
            print("  HELLO:")
            print(json.dumps(hello, indent=4))
        # Show first 3 frames
        print("  First 3 frames:")
        for f in summary["mdp_frames"][:3]:
            print(json.dumps(f, indent=4))

    if summary["legacy_lines"]:
        print(f"Legacy JSON lines: {len(summary['legacy_lines'])}")
        print("  First 3:")
        for line in summary["legacy_lines"][:3]:
            print(f"    {json.dumps(line)[:240]}")

    print()
    # Recommendation
    if summary["looks_like"] == "mdp":
        print("→ Firmware speaks MDP. Use adapter=standalone in the unified agent")
        print("  (it will pass MDP through unchanged).")
    elif summary["looks_like"] == "legacy_json":
        print("→ Firmware speaks legacy JSON. Use adapter=standalone — it auto-bridges")
        print("  to synthetic MDP frames for the rest of the agent pipeline.")
    elif summary["looks_like"] == "silent_or_misconfigured":
        print("→ No bytes seen. Check: device powered, correct COM port,")
        print("  no other app holding the port, try baud rates 9600/57600/921600.")
    else:
        print("→ Bytes seen but neither MDP nor JSON. Check baud rate first.")


def main() -> None:
    ap = argparse.ArgumentParser(description="Probe a MycoBrain over serial.")
    ap.add_argument("port", help="Serial port (COM4, /dev/ttyACM0, etc.)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=8.0)
    ap.add_argument("--json", action="store_true", help="Emit full JSON dump instead of rendered summary")
    args = ap.parse_args()
    summary = probe(args.port, args.baud, args.seconds)
    if args.json:
        # Convert Counter to dict for JSON
        summary["delimiter_counts"] = dict(summary["delimiter_counts"])
        print(json.dumps(summary, indent=2, default=str))
    else:
        render(summary)


if __name__ == "__main__":
    main()
