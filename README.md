# mycobrain
Core hardware motherboard for mycosoft hardware

# MycoBrain V1 — Bring‑Up, Firmware, and Operation (Mycosoft)

## Overview

MycoBrain V1 is a dual‑ESP32‑S3 controller board with an integrated SX1262 LoRa radio. It is designed as a modular biological‑sensor and actuator backbone for Mycosoft devices including Mushroom 1, SporeBase, Petraeus, and MycoNodes.

The board is intentionally split into two logical compute roles:

* **Side‑A (Sensor MCU)** — owns sensors, I2C, analog inputs, and MOSFET outputs.
* **Side‑B (Router MCU)** — routes data between Side‑A and the outside world (LoRa / gateway), manages acknowledgements, retries, and command delivery.

This document explains how to wire, flash, and operate MycoBrain V1 using the current hardened firmware stack.

---

## Hardware Architecture

### Major Components

* 2× ESP32‑S3‑WROOM‑1U modules (ESP‑1 and ESP‑2)
* 1× SX1262 LoRa module (SPI)
* Multiple JST‑PH connectors for:

  * I2C (DI)
  * Analog Inputs (AI)
  * MOSFET Outputs (AO)
  * Battery (BATT) and Solar (SOL)
* Dual USB‑C ports (UART‑0 and UART‑2)

### Functional Split

| MCU        | Role                                                   |
| ---------- | ------------------------------------------------------ |
| ESP‑Side‑A | Sensors, I2C scanning, analog sampling, MOSFET control |
| ESP‑Side‑B | UART↔LoRa routing, reliability, command channel        |

---

## Connector & Wiring Conventions

### I2C Cable Color Legend (Bring‑Up Standard)

* **Black** → GND
* **Red** → 5 V
* **Orange** → 3.3 V
* **Yellow** → SCL
* **Green** → SDA

### Sensor Addressing (BME680/688)

* Default I2C addresses: **0x76** and **0x77**
* Two sensors may share one bus if one is strapped to each address.

### Power Safety

Many Bosch BME breakout boards are **3.3 V logic**. If uncertain:

* Power sensors from **3.3 V (orange)**
* Always share common GND

---

## SX1262 LoRa Pin Mapping (Confirmed)

From MycoBrain V1 schematic:

| SX1262 Signal | ESP32‑S3 GPIO              |
| ------------- | -------------------------- |
| SCK           | GPIO 9                     |
| MOSI          | GPIO 8 (practical default) |
| MISO          | GPIO 12                    |
| NSS / CS      | GPIO 13                    |
| DIO1          | GPIO 14                    |
| DIO2          | GPIO 11                    |
| BUSY          | GPIO 10                    |
| RESET         | Not wired (RadioLib NC)    |

This mapping is used by Side‑B and Gateway firmware.

---

## Firmware Stack

### Side‑A Firmware

* Periodic telemetry generation
* I2C scanning + Bosch chip‑ID probing
* Analog input sampling (AI1‑AI4)
* MOSFET output control (AO1‑AO3)
* Command execution from Side‑B
* UART transport using MDP v1 (COBS + CRC)

### Side‑B Firmware

* UART receiver from Side‑A
* LoRa transmitter (uplink telemetry)
* LoRa receiver (downlink commands)
* ACK + retransmit logic
* Command routing to Side‑A

### Gateway Firmware (Optional)

* LoRa receiver + USB serial output
* Command injection over LoRa
* NDJSON‑friendly output for ingestion

---

## Communication Protocol (Summary)

All MycoBrain V1 devices communicate using **MDP v1 (Mycosoft Device Protocol)**:

* Binary payloads
* **COBS framing**
* **0x00 frame delimiter**
* **CRC16‑CCITT** integrity
* Cumulative ACK + retry support

Detailed protocol specification is provided in **MycoBrainV1‑Protocol.md**.

---

## Bring‑Up Checklist

1. Power board via USB‑C
2. Flash Side‑A and Side‑B firmware
3. Attach BME sensor to I2C port
4. Confirm I2C scan shows 0x76 / 0x77
5. Confirm Side‑B forwards telemetry
6. (Optional) Power gateway and verify LoRa RX

---

## Troubleshooting

### No I2C Devices Found

* Check SDA/SCL orientation
* Lower I2C speed to 100 kHz
* Verify pull‑ups and sensor power

### LoRa Not Transmitting

* Verify SX1262 pin mapping
* Check frequency (915 vs 868 MHz)
* Ensure RadioLib initialization succeeds

### ESP32‑S3 Serial Issues

* Confirm correct USB port (CDC vs UART bridge)
* Enable USB‑CDC‑on‑boot if required

---

## Stability Rules (Hard‑Won Lessons)

1. Freeze I2C pin assignments
2. Always use COBS framing for binary data
3. Telemetry = best‑effort; Commands = reliable
4. Avoid floats in long‑term protocols
5. Log everything as NDJSON at the gateway

---

## Intended Use

MycoBrain V1 is designed as a **general‑purpose biological computing and sensing backbone**, not a single‑purpose board. Firmware roles may evolve, but the Side‑A / Side‑B split and MDP protocol are foundational.

---

© Mycosoft, Inc. — MycoBrain V1
