#!/usr/bin/env python3
import argparse, struct
import serial

MDP_MAGIC = 0xA15A
MDP_VER = 1

ACK_REQUESTED = 0x01

EP_GATEWAY = 0xC0

def crc16_ccitt_false(data: bytes) -> int:
  crc = 0xFFFF
  for b in data:
    crc ^= (b << 8)
    for _ in range(8):
      crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
      crc &= 0xFFFF
  return crc

def cobs_encode(data: bytes) -> bytes:
  out = bytearray()
  out.append(0)
  code_i = 0
  code = 1
  for b in data:
    if b == 0:
      out[code_i] = code
      code_i = len(out)
      out.append(0)
      code = 1
    else:
      out.append(b)
      code += 1
      if code == 0xFF:
        out[code_i] = code
        code_i = len(out)
        out.append(0)
        code = 1
  out[code_i] = code
  out.append(0)
  return bytes(out)

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument('--port', required=True)
  ap.add_argument('--baud', type=int, default=115200)
  ap.add_argument('--dst', type=lambda x:int(x,0), required=True)
  ap.add_argument('--cmd', type=lambda x:int(x,0), required=True)
  ap.add_argument('--data', default='')
  ap.add_argument('--seq', type=int, default=1)
  ap.add_argument('--ack', type=int, default=0)
  args = ap.parse_args()

  cmd_data = bytes.fromhex(args.data) if args.data else b''

  hdr = struct.pack('<HBBIIBBBB', MDP_MAGIC, MDP_VER, 0x02, args.seq, args.ack, ACK_REQUESTED, EP_GATEWAY, args.dst, 0)
  body = struct.pack('<HH', args.cmd, len(cmd_data)) + cmd_data
  payload = hdr + body
  crc = crc16_ccitt_false(payload)
  frame = cobs_encode(payload + struct.pack('<H', crc))

  ser = serial.Serial(args.port, args.baud, timeout=1)
  ser.write(frame)
  ser.flush()

if __name__ == '__main__':
  main()