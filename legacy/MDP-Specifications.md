## Mycosoft Device Protocol Specification

**Document ID:** MYCO-PROTO-DEV-001
**Title:** Mycosoft Device Protocol (MDP) — UART + LoRa Transport
**Version:** 1.0
**Status:** Draft for implementation
**Applies to:** MycoBrain V1 (ESP32AB Side-A + Side-B), Mushroom1, SporeBase, Petraeus, MycoNode family

---

### 1. Scope and Goals

MDP defines a compact, robust, binary protocol for:

* Telemetry transmission from device subsystems (e.g., Side-A → Side-B)
* Forwarding over constrained links (LoRa)
* Bi-directional command/control with acknowledgements, retransmission, and ordering

MDP is transport-agnostic and may be used over:

* UART (A↔B)
* LoRa (device↔gateway)
* Future transports (BLE, Wi-Fi, Ethernet) without changing payload semantics

---

### 2. Terminology

* **Frame:** A delimited unit transmitted over a transport.
* **Payload:** The unencoded protocol message bytes inside a frame.
* **COBS:** Consistent Overhead Byte Stuffing (used for framing reliability).
* **CRC16:** CCITT-FALSE checksum verifying payload integrity.
* **Endpoint:** A protocol participant (Side-A, Side-B, gateway, MAS agent).
* **Seq:** Sequence number for reliability and ordering.

---

### 3. Transport Framing

MDP uses **COBS framing** with a **0x00 delimiter** and **CRC16** for integrity.

#### 3.1 Frame Format (On Wire)

```
COBS_ENCODE( PAYLOAD || CRC16_LE(PAYLOAD) ) || 0x00
```

* `0x00` is reserved solely as a frame delimiter (never appears within the COBS frame body).
* `CRC16_LE` is CRC16 CCITT-FALSE appended little-endian.

#### 3.2 CRC Details

* Polynomial: `0x1021`
* Init: `0xFFFF`
* No reflection, no XOR-out (CCITT-FALSE)

#### 3.3 Maximum Sizes

Implementations SHALL define and enforce:

* `MAX_PAYLOAD` (recommend: 512 bytes UART; 220 bytes LoRa typical if unfragmented)
* `MAX_FRAME` = `MAX_PAYLOAD + CRC(2) + COBS overhead`

---

### 4. Message Layer

All payloads start with a common header.

#### 4.1 Common Header

```c
#pragma pack(push,1)
typedef struct {
  uint16_t magic;      // 0xA15A
  uint8_t  version;    // 1
  uint8_t  msg_type;   // see 4.2
  uint32_t seq;        // sender sequence
  uint32_t ack;        // cumulative ack of received seq (optional; see 6)
  uint8_t  flags;      // bitfield
  uint8_t  src;        // endpoint id
  uint8_t  dst;        // endpoint id (0xFF=broadcast)
  uint8_t  rsv;        // reserved
} mdp_hdr_v1_t;
#pragma pack(pop)
```

**Flags (v1):**

* `0x01` = ACK_REQUESTED (receiver must respond with ACK)
* `0x02` = IS_ACK (this message is an ACK packet)
* `0x04` = IS_NACK (this message is a NACK packet)
* `0x08` = HAS_COMMAND (payload includes a command block)
* `0x10` = FRAGMENTED (see 8; optional v1.1)

**Endpoint IDs (v1 recommended):**

* `0xA1` = Side-A
* `0xB1` = Side-B
* `0xG0` = Gateway
* `0xM0` = MAS Ingest Agent
* `0xFF` = Broadcast

> Note: This header replaces the current “TelemetryV1 magic/proto/msg_type” header. If you need strict backward compatibility, keep TelemetryV1 as msg body and wrap it with `mdp_hdr_v1_t`.

---

### 4.2 Message Types

* `0x01` TELEMETRY: periodic measurements / status
* `0x02` COMMAND: request to perform an action or set configuration
* `0x03` ACK: acknowledgment (can be standalone or piggyback via header fields)
* `0x04` NACK: negative acknowledgment (optional; use for explicit errors)
* `0x05` EVENT: asynchronous notable event (fault, boot, sensor attach)
* `0x06` HELLO: capability discovery / link bring-up
* `0x07` HEARTBEAT: liveness ping (lightweight)
* `0x08` LOG: structured log line / debug record

---

### 5. Telemetry Message

#### 5.1 Telemetry Body (v1)

Two acceptable options:

**Option A (wrap existing struct):**

```c
typedef struct {
  mdp_hdr_v1_t hdr;
  TelemetryV1  t;     // your existing packed struct
} mdp_telemetry_wrap_v1_t;
```

**Option B (MDP-native compact telemetry):**

* Prefer counts + scaled integers (mV, centi-degrees, etc.)
* Avoid floats for cross-platform parsing stability

**Required fields for v1 operation:**

* `seq` increments per sender message
* `uptime_ms` inside telemetry (or in a standardized telemetry field)

---

### 6. Reliability: ACK / Retransmit

MDP v1 supports reliable delivery via **Selective Repeat (windowed) or Stop-and-Wait**. For initial implementation, use **Stop-and-Wait on UART** and **Window=4 on LoRa**.

#### 6.1 ACK semantics

* Receiver maintains `rx_last_inorder_seq`
* Receiver SHALL transmit ACK:

  * immediately for `ACK_REQUESTED` messages, OR
  * periodically via piggyback (`ack` field in outgoing messages)
* `ack` is **cumulative**: “I have received all seq ≤ ack”.

#### 6.2 Optional Selective ACK bitmap (recommended for LoRa)

To handle out-of-order or drops efficiently, add an ACK extension when `IS_ACK`:

```c
typedef struct {
  mdp_hdr_v1_t hdr;     // hdr.msg_type = ACK, hdr.flags includes IS_ACK
  uint32_t base_seq;    // ack base
  uint32_t bitmap;      // bits for base_seq+1 ... base_seq+32 (1=received)
} mdp_ack_ext_v1_t;
```

* This allows selective retransmission without high overhead.

#### 6.3 Retransmission behavior

Sender maintains a TX queue of unacknowledged messages:

* Each queued entry has: `seq`, `payload`, `len`, `sent_time`, `retries`
* Retransmit when `now - sent_time > RTO_MS`

**Recommended defaults:**

* UART RTO: 50–150 ms (local link)
* LoRa RTO: 800–3000 ms (depends on SF/BW/airtime)
* Max retries: 5 (UART), 3 (LoRa)

#### 6.4 Duplicate handling

Receiver MUST accept duplicates and drop them safely:

* If `seq ≤ rx_last_inorder_seq`, treat as duplicate; re-ACK (idempotent).

---

### 7. Command Channel

Commands are encoded as a **typed TLV** command block for extensibility.

#### 7.1 Command Message Body

```c
typedef struct {
  mdp_hdr_v1_t hdr;      // msg_type=COMMAND
  uint16_t cmd_id;       // command identifier
  uint16_t cmd_len;      // bytes following
  uint8_t  cmd_data[];   // TLVs or fixed format per cmd_id
} mdp_cmd_v1_t;
```

#### 7.2 Command Response

Responses SHALL be sent as:

* ACK with status code (simple), or
* EVENT message with `event_type=CMD_RESULT`, including `cmd_id`, `status`, optional data

#### 7.3 Standard Command IDs (initial set)

* `0x0001` SET_I2C_CONFIG (sda, scl, hz)
* `0x0002` SCAN_I2C_NOW
* `0x0003` SET_TELEMETRY_RATE (ms)
* `0x0004` SET_OUTPUTS (mos mask/state)
* `0x0005` LED_SET_MODE / LED_SET_RGB
* `0x0006` BSEC_SET_RATE (sensor slot, lp/ulp)
* `0x0007` SAVE_NVS / LOAD_NVS
* `0x0008` REBOOT
* `0x0009` FACTORY_RESET

Each command MUST be idempotent or safely repeatable because retransmits occur.

---

### 8. Fragmentation (for LoRa) — optional v1.1

If telemetry exceeds the LoRa maximum payload, fragmentation is required.

Fragment header extension when `FRAGMENTED` flag is set:

```c
typedef struct {
  uint16_t frag_id;     // message group id
  uint8_t  frag_idx;    // 0..N-1
  uint8_t  frag_cnt;    // total fragments
} mdp_frag_v1_t;
```

Receiver reassembles based on `(src, frag_id)` with timeout.

---

### 9. Security (future v2)

MDP v1 does not encrypt. v2 SHALL add authenticated encryption:

* AEAD: ChaCha20-Poly1305
* Per-device key provisioning
* Nonce strategy tied to seq

---

## Implementation Work: ACK/Retry + Bidirectional Commands (A & B)

### A. Side-A (Sender) changes

1. Wrap existing telemetry with MDP header
2. Use COBS framing (already agreed)
3. Add TX queue for reliable messages:

* Telemetry can be “best effort” on UART if you want, but commands must be reliable.
* Recommended: telemetry reliable on UART (cheap), best-effort on LoRa (unless critical).

**Side-A responsibilities**

* Maintain `tx_seq++`
* Maintain `rx_ack_from_b` and clear queued frames
* Process incoming COMMAND messages from Side-B (if Side-B forwards gateway commands)

### B. Side-B (Router + LoRa) changes

Side-B becomes a router:

* UART <-> LoRa
* Telemetry uplink (A→B→LoRa)
* Command downlink (LoRa→B→UART→A)
* ACK coordination on both hops

**Side-B responsibilities**

* Maintain per-link RX tracking: `rx_last_inorder_seq_uart`, `rx_last_inorder_seq_lora`
* Generate ACKs quickly
* Forward COMMAND messages to Side-A and forward CMD_RESULT back up

---

## Concrete next step: Code skeletons (ready to implement)

### 1) Side-B: Add LoRa RX + forward COMMAND to Side-A

Core loop tasks:

* `uart_rx_poll_cobs()` -> parse MDP, if TELEMETRY: forward to LoRa
* `lora_rx_poll()` -> if COMMAND: forward to UART (Side-A) reliably
* `ack_manager()` -> send ACKs on both links

### 2) Side-A: Add UART RX handling for commands + ACKs

* `uart_rx_poll_cobs()` -> parse MDP

  * if COMMAND: execute, send EVENT(CMD_RESULT) + ACK
  * if ACK: mark tx queue entries acknowledged

---

## ACK / Retransmit policy that will work immediately

### UART (A↔B)

* Stop-and-wait for COMMAND traffic
* Telemetry: piggyback ack; do not retransmit telemetry unless requested

### LoRa (B↔Gateway)

* Window=4 (selective ack bitmap recommended)
* Retransmit COMMANDs and CMD_RESULTs
* Telemetry best-effort by default; “critical telemetry” can set `ACK_REQUESTED`

---

## What I will produce next if you confirm one detail

To write the exact Side-B firmware, I need one board fact:

**Which direction is LoRa used?**

1. Side-B transmits only (uplink telemetry), and receives commands rarely
2. Full duplex: Side-B both TX/RX regularly (telemetry uplink + command downlink)

If you don’t answer, I’ll assume **full duplex** and implement RX/TX with a small reliable command channel.

---

## Deliverables (immediate, in the next message if you want)

1. **MDP v1 reference C++ module** (`mdp_framing.h/.cpp`, `mdp_router.h/.cpp`)
2. **Side-A firmware patch**: wrap telemetry in `mdp_hdr_v1_t`, parse commands, send cmd results
3. **Side-B full firmware**: UART+LoRa router, ACK/Retry, command channel, NDJSON logs to USB
4. **Gateway/MAS decoder**: Python parser for COBS frames + CRC + JSON emit (for testing)

State whether LoRa is **915 MHz** and whether Side-B uses **SX1262 with RadioLib** (as your board indicates), and I’ll generate the full integrated Side-A + Side-B code package accordingly.
