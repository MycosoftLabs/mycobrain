#!/usr/bin/env python3
import argparse, json, struct, sys
import serial

MDP_MAGIC = 0xA15A
MDP_VER = 1

EP_NAMES = {
  0xA1: "SIDE_A",
  0xB1: "SIDE_B",
  0xC0: "GATEWAY",
  0xFF: "BCAST",
}

TYPE_NAMES = {
  0x01: "TELEMETRY",
  0x02: "COMMAND",
  0x03: "ACK",
  0x05: "EVENT",
  0x06: "HELLO",
}

def crc16_ccitt_false(data: bytes) -> int:
  crc = 0xFFFF
  for b in data:
    crc ^= (b << 8)
    for _ in range(8):
      crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
      crc &= 0xFFFF
  return crc

def cobs_decode(data: bytes):
  out = bytearray()
  i = 0
  while i < len(data):
    code = data[i]
    if code == 0:
      return None
    if i + code > len(data) + 1:
      return None
    i += 1
    for _ in range(code - 1):
      if i >= len(data):
        break
      out.append(data[i]); i += 1
    if code != 0xFF and i < len(data):
      out.append(0)
  return bytes(out)

def decode_frame(frame: bytes):
  if not frame or frame[-1] != 0:
    return None
  enc = frame[:-1]
  dec = cobs_decode(enc)
  if dec is None or len(dec) < 2:
    return None
  payload, recv_crc = dec[:-2], struct.unpack('<H', dec[-2:])[0]
  calc = crc16_ccitt_false(payload)
  if recv_crc != calc:
    return {"error":"CRC_MISMATCH","recv":hex(recv_crc),"calc":hex(calc)}
  if len(payload) < 16:
    return {"error":"SHORT_PAYLOAD","len":len(payload)}
  magic, ver, mtype, seq, ack, flags, src, dst, rsv = struct.unpack('<HBBIIBBBB', payload[:16])
  return {
    "magic": hex(magic),
    "ver": ver,
    "type": mtype,
    "type_name": TYPE_NAMES.get(mtype, str(mtype)),
    "seq": seq,
    "ack": ack,
    "flags": flags,
    "src": src,
    "src_name": EP_NAMES.get(src, str(src)),
    "dst": dst,
    "dst_name": EP_NAMES.get(dst, str(dst)),
    "len": len(payload)
  }

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument('--port', required=True)
  ap.add_argument('--baud', type=int, default=115200)
  args = ap.parse_args()
  ser = serial.Serial(args.port, args.baud, timeout=1)
  buf = bytearray()
  while True:
    b = ser.read(1)
    if not b:
      continue
    buf.append(b[0])
    if b[0] == 0:
      msg = decode_frame(bytes(buf))
      if msg:
        print(json.dumps(msg))
      buf.clear()

if __name__ == '__main__':
  try:
    main()
  except KeyboardInterrupt:
    pass