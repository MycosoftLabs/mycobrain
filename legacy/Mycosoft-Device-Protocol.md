# MycoBrain V1 — Mycosoft Device Protocol (MDP v1)

## 1. Purpose

MDP (Mycosoft Device Protocol) defines a robust, binary communication layer for MycoBrain V1 and related Mycosoft devices.

Goals:

* Survive noisy links (UART, LoRa)
* Support reliable command delivery
* Be extensible for future sensors and security

---

## 2. Transport Framing

### 2.1 Frame Format

```
COBS_ENCODE( PAYLOAD || CRC16_LE(PAYLOAD) ) 0x00
```

* COBS removes all zero bytes from payload
* `0x00` is used exclusively as a frame delimiter

### 2.2 CRC

* Algorithm: CRC16‑CCITT‑FALSE
* Polynomial: `0x1021`
* Init: `0xFFFF`

---

## 3. Common Header (MDP v1)

```c
#pragma pack(push,1)
typedef struct {
  uint16_t magic;      // 0xA15A
  uint8_t  version;    // 1
  uint8_t  msg_type;   // see below
  uint32_t seq;        // sender sequence
  uint32_t ack;        // cumulative ACK
  uint8_t  flags;      // bitfield
  uint8_t  src;        // endpoint id
  uint8_t  dst;        // endpoint id
  uint8_t  rsv;        // reserved
} mdp_hdr_v1_t;
#pragma pack(pop)
```

### 3.1 Flags

* `0x01` ACK_REQUESTED
* `0x02` IS_ACK
* `0x04` IS_NACK

### 3.2 Endpoints

| ID   | Meaning   |
| ---- | --------- |
| 0xA1 | Side‑A    |
| 0xB1 | Side‑B    |
| 0xG0 | Gateway   |
| 0xFF | Broadcast |

---

## 4. Message Types

| Type      | Value | Description             |
| --------- | ----- | ----------------------- |
| TELEMETRY | 0x01  | Periodic measurements   |
| COMMAND   | 0x02  | Control / configuration |
| ACK       | 0x03  | Acknowledgement         |
| EVENT     | 0x05  | Asynchronous event      |
| HELLO     | 0x06  | Capability discovery    |

---

## 5. Reliability Model

### 5.1 ACK Semantics

* ACKs are **cumulative**
* `ack = N` ⇒ all seq ≤ N received

### 5.2 Retransmission

* Commands and Events: **reliable**
* Telemetry: **best‑effort** by default

### 5.3 Duplicate Handling

Receivers must safely ignore duplicates while re‑ACKing.

---

## 6. Telemetry Message

Telemetry is encapsulated inside an MDP payload and may contain:

* Analog readings
* MOSFET states
* I2C scan summary
* Sensor metadata

Exact telemetry structure is implementation‑defined but versioned.

---

## 7. Command Channel

### 7.1 Command Payload

```c
typedef struct {
  mdp_hdr_v1_t hdr;
  uint16_t cmd_id;
  uint16_t cmd_len;
  uint8_t  cmd_data[];
} mdp_cmd_v1_t;
```

### 7.2 Standard Commands (v1)

| ID     | Command            |
| ------ | ------------------ |
| 0x0001 | SET_I2C            |
| 0x0002 | SCAN_I2C           |
| 0x0003 | SET_TELEMETRY_RATE |
| 0x0004 | SET_MOSFET         |
| 0x0007 | SAVE_NVS           |
| 0x0008 | LOAD_NVS           |
| 0x0009 | REBOOT             |

---

## 8. Command Results

Commands return results via EVENT messages:

```c
typedef struct {
  mdp_hdr_v1_t hdr;
  uint16_t evt_type;   // CMD_RESULT
  uint16_t evt_len;
  uint16_t cmd_id;
  int16_t  status;     // 0 = OK
} mdp_evt_cmd_result_v1_t;
```

---

## 9. LoRa Usage Rules

* Telemetry: uplink, best‑effort
* Commands: downlink, reliable
* ACKs may be piggybacked

---

## 10. Future Extensions

Planned MDP v2+ features:

* Fragmentation for large payloads
* Selective ACK bitmaps
* Authenticated encryption (ChaCha20‑Poly1305)
* Formal schema for biological waveforms

---

## 11. Design Philosophy

MDP is designed to be:

* Deterministic
* Debbugable in the field
* Friendly to constrained radios
* Stable across hardware generations

---

© Mycosoft, Inc. — MycoBrain V1 Protocol
