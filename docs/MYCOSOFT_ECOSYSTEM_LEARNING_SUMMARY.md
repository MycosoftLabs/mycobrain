# Mycosoft Ecosystem - Comprehensive Learning Summary

## Executive Summary

This document summarizes the complete Mycosoft development ecosystem, including MycoBrain hardware, MAS (Multi-Agent System), NatureOS, MINDEX, and Website codebases. This knowledge base prepares for adding new capabilities: **WiFi Sense** (CSI-based sensing) and **MycoDRONE** (autonomous deployment/recovery).

---

## System Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    mycosoft.com Domain                      │
│                 (GoDaddy + Vercel + GitHub)                 │
└────────────────────┬────────────────────────────────────────┘
                     │
        ┌────────────┴─────────────┬─────────────┬────────────┐
        │                          │             │            │
   ┌────▼────┐              ┌──────▼──────┐  ┌──▼──────┐ ┌──▼──────┐
   │ Website │              │  NatureOS   │  │  MAS    │ │ MINDEX  │
   │ (Main)  │              │  (Web App)  │  │ (Agents)│ │(Ledger) │
   └─────────┘              └──────┬──────┘  └────┬────┘ └────┬────┘
                                    │              │           │
                             ┌──────┴──────────────┴───────────┴──────┐
                             │   Shared Infrastructure                │
                             │  PostgreSQL, Redis, Qdrant, Prometheus │
                             └────────────────┬───────────────────────┘
                                              │
                                       ┌──────▼─────────┐
                                       │   MycoBrain   │
                                       │   (Hardware)  │
                                       └────────────────┘
```

---

## 1. MycoBrain V1 Hardware Platform

### Hardware Architecture

**Dual-ESP32-S3 Controller Board:**
- **2× ESP32-S3-WROOM-1U-N16R8** modules (ESP-1 and ESP-2)
- **1× SX1262 LoRa module** (SPI interface)
- **Multiple JST-PH connectors:**
  - I2C (DI) - Digital Interface
  - Analog Inputs (AI1-AI4)
  - MOSFET Outputs (AO1-AO3)
  - Battery (BATT) and Solar (SOL)
- **Dual USB-C ports** (UART-0 and UART-2)

### Functional Split

| MCU        | Role                                                   |
| ---------- | ------------------------------------------------------ |
| ESP-Side-A | Sensors, I2C scanning, analog sampling, MOSFET control |
| ESP-Side-B | UART↔LoRa routing, reliability, command channel        |

### Communication Protocol: MDP v1 (Mycosoft Device Protocol)

**Framing:**
- `COBS_ENCODE(payload + CRC16_LE) + 0x00`
- CRC: **CRC16-CCITT-FALSE** (poly 0x1021, init 0xFFFF)

**Message Types:**
- `TELEMETRY=0x01` - Sensor data (best-effort)
- `COMMAND=0x02` - Downlink commands (reliable)
- `ACK=0x03` - Acknowledgment
- `EVENT=0x05` - Device events
- `HELLO=0x06` - Discovery/announcement

**Endpoints:**
- `EP_SIDE_A = 0xA1`
- `EP_SIDE_B = 0xB1`
- `EP_GATEWAY = 0xC0`
- `EP_BCAST = 0xFF`

**Header Structure:**
```c
typedef struct mdp_hdr_v1_t {
  uint16_t magic;   // 0xA15A
  uint8_t  version; // 1
  uint8_t  msg_type;
  uint32_t seq;     // sender sequence
  uint32_t ack;     // cumulative ack for peer
  uint8_t  flags;   // ACK_REQUESTED, IS_ACK, IS_NACK
  uint8_t  src;
  uint8_t  dst;
  uint8_t  rsv;
} mdp_hdr_v1_t;
```

### Firmware Structure

**Side-A Firmware** (`firmware/side_a/src/main.cpp`):
- Periodic telemetry generation
- I2C scanning + Bosch BME chip-ID probing
- Analog input sampling (AI1-AI4)
- MOSFET output control (AO1-AO3)
- Command execution from Side-B
- UART transport using MDP v1 (COBS + CRC)

**Side-B Firmware** (`firmware/side_b/src/main.cpp`):
- UART receiver from Side-A
- LoRa transmitter (uplink telemetry)
- LoRa receiver (downlink commands)
- ACK + retransmit logic
- Command routing to Side-A

**Gateway Firmware** (`firmware/gateway/src/main.cpp`):
- LoRa receiver + USB serial output
- Command injection over LoRa
- NDJSON-friendly output for ingestion

### SX1262 LoRa Pin Mapping

| SX1262 Signal | ESP32-S3 GPIO |
| ------------- | ------------- |
| SCK           | GPIO 9        |
| MOSI          | GPIO 8        |
| MISO          | GPIO 12       |
| NSS / CS      | GPIO 13       |
| DIO1          | GPIO 14       |
| DIO2          | GPIO 11       |
| BUSY          | GPIO 10       |
| RESET         | Not wired     |

### Current Commands

| CMD_ID | Name | Description |
|--------|------|-------------|
| 0x0001 | CMD_SET_I2C | Configure I2C bus (SDA, SCL, frequency) |
| 0x0002 | CMD_SCAN_I2C | Trigger I2C bus scan |
| 0x0003 | CMD_SET_TELEM_MS | Set telemetry period (ms) |
| 0x0004 | CMD_SET_MOS | Set MOSFET output (idx 1-3, value 0/1) |
| 0x0007 | CMD_SAVE_NVS | Save configuration to NVS |
| 0x0008 | CMD_LOAD_NVS | Load configuration from NVS |
| 0x0009 | CMD_REBOOT | Reboot device |

---

## 2. MINDEX (Mycological Intelligence Exchange)

### Purpose

MINDEX is Mycosoft's canonical fungal knowledge graph: a Postgres/PostGIS database, FastAPI service layer, ETL pipelines, and ledger adapters that tie live telemetry, taxonomy, and IP assets into NatureOS, MYCA agents, and device firmware.

### Architecture

```
┌────────────┐    ingest    ┌────────────┐    async SQLA    ┌──────────────┐
│ External   │ ───────────▶ │  mindex_etl│ ───────────────▶ │ Postgres +   │
│ data/APIs  │              │  jobs      │                  │ PostGIS      │
└────────────┘              └────────────┘                  │ (core/bio/…) │
                                                               ▲      │
                                                               │      ▼
                                                        ┌──────────────┐
                                                        │ FastAPI app │
                                                        │ mindex_api  │
                                                        └──────────────┘
                                                               │REST
                                                               ▼
                                                        NatureOS • MYCA • Devices
```

### Database Schemas

- `core` - Canonical taxonomy
- `bio` - Biological data (genomes, traits)
- `obs` - Observations
- `telemetry` - Device telemetry
- `ip` - IP assets
- `ledger` - Blockchain/ledger bindings
- `app.*` - Read-optimized views

### MycoBrain Integration

**Database Tables:**
- `telemetry.device` - Device registry
- `telemetry.mycobrain_device` - MycoBrain-specific config
- `telemetry.device_command` - Queued downlink commands
- `telemetry.device_setpoint` - Automation setpoints
- `telemetry.mdp_telemetry_log` - Idempotent telemetry ingestion

**API Endpoints:**
- `POST /mycobrain/telemetry/ingest` - Ingest telemetry
- `GET /mycobrain/devices` - List devices
- `GET /mycobrain/devices/{id}/status` - Device status
- `POST /mycobrain/devices/{id}/commands` - Queue command
- `POST /mycobrain/devices/{id}/setpoints` - Set automation

**Protocol Support:**
- `mindex_api/protocols/mdp_v1.py` - COBS framing + CRC16 + frame helpers

---

## 3. MAS (Multi-Agent System)

### Purpose

MYCA (Mycosoft AI) Multi-Agent System provides AI workforce capabilities for:
- Biological sample analysis
- Device management automation
- Data enrichment and pattern recognition
- Natural language interaction

### Architecture

- **Orchestrator** (`mycosoft_mas/core/main.py`) - FastAPI service
- **Agents** (`mycosoft_mas/agents/`) - Specialized AI agents
- **Integrations** (`mycosoft_mas/integrations/`) - External service clients
- **Services** (`mycosoft_mas/services/`) - Background services

### Integration Points

- **NatureOS → MAS**: Agent execution requests
- **MAS → MINDEX**: Context enrichment queries
- **MAS → MycoBrain**: Device control via MINDEX command queue

---

## 4. NatureOS (Web Application)

### Purpose

NatureOS is a cloud-native "operating system for nature" - a layered Azure architecture that ingests heterogeneous environmental signals, stores them in MINDEX, processes them with event-driven algorithms, and exposes everything through a unified Core API.

### Technology Stack

- **Frontend**: Next.js (React/TypeScript)
- **Backend**: .NET Core API (C#)
- **Database**: PostgreSQL/PostGIS
- **Event Processing**: Azure Event Grid, Service Bus
- **IoT**: Azure IoT Hub

### MycoBrain Integration

- **Dashboard Widgets**: Device status, telemetry visualization
- **Device Manager**: Device discovery, configuration, command interface
- **API Routes**: Server-side proxy to MAS/MINDEX (API keys never exposed to browser)

---

## 5. Website (mycosoft.com)

### Purpose

Public-facing website with:
- Product information
- Device portals
- NatureOS dashboard integration
- SDK documentation

### Technology Stack

- **Framework**: Next.js
- **Hosting**: Vercel
- **Domain**: GoDaddy DNS

---

## 6. Integration Patterns

### Service Communication

1. **NatureOS → MAS**: Agent execution
   - Server-side API routes
   - Bearer token authentication
   - JSON request/response

2. **NatureOS → MINDEX**: Device data queries
   - Server-side API routes
   - X-API-Key header
   - RESTful endpoints

3. **MAS → MINDEX**: Context enrichment
   - HTTP client with API key
   - Async/await patterns

4. **MycoBrain → MINDEX**: Telemetry ingestion
   - MDP v1 protocol over LoRa/UART
   - Gateway converts to HTTP POST
   - Idempotent ingestion (device + seq + timestamp)

### Data Flow

```
MycoBrain (ESP32) 
  → LoRa/UART 
  → Gateway (ESP32) 
  → Serial/USB 
  → MINDEX Ingestion API 
  → PostgreSQL 
  → NatureOS Dashboard
  → MAS Agents (context)
```

---

## 7. Development Environment

### Local Development

**Docker Compose Stack:**
- PostgreSQL (port 5432)
- Redis (port 6379)
- MAS API (port 8001)
- MINDEX API (port 8002)
- NatureOS (port 3000)
- Website (port 3002)

**Environment Variables:**
- `MAS_API_BASE_URL=http://localhost:8001`
- `MINDEX_API_BASE_URL=http://localhost:8002`
- `NATUREOS_API_URL=http://localhost:3000`
- `MAS_API_KEY=REPLACE_WITH_SECURE_VALUE`
- `MINDEX_API_KEY=REPLACE_WITH_SECURE_VALUE`

### Deployment Targets

1. **Local**: Docker Compose (development)
2. **Proxmox**: k3s or Docker Swarm (on-prem)
3. **Azure**: Container Apps / AKS (cloud)

---

## 8. Key Design Principles

### Reliability

- **MDP v1**: COBS framing + CRC16 for integrity
- **Cumulative ACKs**: Reliable command delivery
- **Retry Logic**: Configurable RTO and max retries
- **Idempotent Ingestion**: Device + seq + timestamp prevents duplicates

### Security

- **API Keys**: Service-to-service authentication
- **Device Keys**: Per-device SHA-256 hashed keys
- **HTTPS/TLS**: Production deployments
- **Key Vault**: Azure Key Vault for secrets

### Scalability

- **Microservices**: Independent scaling
- **Event-Driven**: Async processing
- **Caching**: Redis for hot data
- **Database Views**: Read-optimized queries

---

## 9. Current Capabilities

### MycoBrain V1

- ✅ Dual-ESP32-S3 architecture
- ✅ LoRa communication (SX1262)
- ✅ I2C sensor support (BME680/688)
- ✅ Analog input sampling
- ✅ MOSFET output control
- ✅ MDP v1 protocol
- ✅ Command/telemetry reliability
- ✅ Gateway firmware

### MINDEX

- ✅ Device registry
- ✅ Telemetry storage
- ✅ Command queue
- ✅ Setpoint automation
- ✅ Taxonomy database
- ✅ Ledger bindings

### NatureOS

- ✅ Device dashboard
- ✅ Telemetry visualization
- ✅ Command interface
- ✅ MAS integration
- ✅ MINDEX integration

---

## 10. Extension Points for New Capabilities

### Firmware Extension

- **New MDP Commands**: Add to `mdp_commands.h`
- **New Telemetry Fields**: Extend `TelemetryV1` struct
- **New Sensors**: I2C or analog interfaces
- **New Communication**: WiFi, Bluetooth, Ethernet

### MINDEX Extension

- **New Tables**: Device capability tables
- **New API Endpoints**: Feature-specific routes
- **New Schemas**: Pydantic models for new data types

### NatureOS Extension

- **New Dashboard Widgets**: React components
- **New API Routes**: Next.js API routes
- **New UI Pages**: Feature-specific views

### MAS Extension

- **New Agents**: Specialized AI agents
- **New Integrations**: External service clients
- **New Services**: Background processing

---

## 11. Preparation for WiFi Sense & MycoDRONE

### WiFi Sense Requirements

1. **Hardware**: ESP32-S3 WiFi CSI capability
2. **Firmware**: CSI capture + streaming
3. **MINDEX**: CSI data storage schema
4. **Edge Compute**: CSI processing service
5. **NatureOS**: WiFi Sense dashboard widget
6. **MAS**: WiFi Sense analysis agents

### MycoDRONE Requirements

1. **Hardware**: Flight controller integration
2. **Firmware**: MAVLink bridge, payload control
3. **MINDEX**: Drone mission tracking, device recovery
4. **NatureOS**: Drone control interface, mission planning
5. **MAS**: Autonomous mission planning agents

---

## Conclusion

The Mycosoft ecosystem is a well-architected, modular system with clear separation of concerns:
- **MycoBrain**: Hardware platform with MDP v1 protocol
- **MINDEX**: Central data store and API
- **MAS**: AI agent system
- **NatureOS**: Web application and dashboard
- **Website**: Public-facing site

All components communicate via well-defined APIs and protocols, making it straightforward to add new capabilities like WiFi Sense and MycoDRONE.

---

**Document Version**: 1.0  
**Last Updated**: 2025-01-XX  
**Author**: AI Assistant (Cursor)
