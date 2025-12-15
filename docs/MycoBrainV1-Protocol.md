# Mycosoft Device Protocol (MDP) v1

## Framing (authoritative)

`COBS_ENCODE(payload + CRC16_LE) + 0x00`

- CRC: **CRC16-CCITT-FALSE** (poly 0x1021, init 0xFFFF)

## Header

```c
typedef struct mdp_hdr_v1_t {
  uint16_t magic;   // 0xA15A
  uint8_t  version; // 1
  uint8_t  msg_type;
  uint32_t seq;
  uint32_t ack;
  uint8_t  flags;
  uint8_t  src;
  uint8_t  dst;
  uint8_t  rsv;
} mdp_hdr_v1_t;
```

## Types

- TELEMETRY=0x01
- COMMAND=0x02
- ACK=0x03
- EVENT=0x05

## Flags

- ACK_REQUESTED=0x01
- IS_ACK=0x02
- IS_NACK=0x04