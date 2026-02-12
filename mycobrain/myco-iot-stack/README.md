# Myco IoT Unified Envelope (CBOR + Ed25519) + Azure Ingest (IoT Hub -> ADX)

This repo is a **drop-in starter kit** that does 4 things:

1) **Device-side codec**: builds a *deterministic CBOR* envelope and signs it with **Ed25519**.  
2) **Cloud ingestor** (Node or Go): reads from **IoT Hub built-in Event Hubs endpoint**, validates signature + hash, dedupes, and flattens readings.  
3) **ADX schema**: KQL to create `SensorRaw` + `SensorPoints` and an ingestion mapping + optional update policy.  
4) **Key tooling**: generate device keypairs and a simple device registry JSON.

> Note: The embedded code is written to be portable (ESP32 / STM32). It avoids big CBOR dependencies by including a tiny CBOR writer for the specific schema.  
> Ed25519 signing/verify is delegated to **Monocypher** (recommended) or any other Ed25519 library you prefer.

---

## Quick start

### A) Create ADX tables + mapping
1. Open ADX Web UI (Data Explorer) and select your database.
2. Run: `adx/kusto_setup.kql`

### B) Generate device keys / registry
Node:
```bash
cd tools
npm i
node gen_keys.mjs --count 3 --out ../devices.json
```

This writes:
- `devices.json` – maps `deviceId -> publicKeyB64`
- `devices_private.json` – maps `deviceId -> privateKeyB64` (keep secret!)

### C) Run the ingestor (Node)
```bash
cd ingestors/node
npm i
cp .env.example .env
# edit .env (IoT Hub built-in endpoint + ADX ingest endpoint + device registry file path)
node index.js
```

### D) Device sends CBOR to IoT Hub
- Encode envelope with `embedded/c/myco_envelope.c`
- Send CBOR bytes as your D2C message body.

---

## Folder map

- `spec/` – envelope spec + sensor/unit code suggestions
- `embedded/c/` – deterministic CBOR encoder + envelope builder + signing hooks
- `ingestors/node/` – Event Hubs consumer -> verify -> ADX ingest
- `ingestors/go/` – same in Go
- `adx/` – KQL setup scripts
- `tools/` – key generation + device registry helpers

---

## Security note (important)

- Treat device private keys like credentials.
- In production, load `devices.json` public keys from a secure store (Key Vault, HSM-backed secrets, etc).
- The ingestors include a simple in-memory dedupe cache; for HA deployments, replace it with Redis/Cosmos/etc.

