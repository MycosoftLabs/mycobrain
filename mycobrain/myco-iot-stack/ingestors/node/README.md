# Node ingestor (IoT Hub built-in endpoint -> verify -> ADX)

## What it does
- Connects to the **IoT Hub built-in Event Hubs-compatible endpoint**
- Decodes **CBOR** (or JSON if you send JSON)
- Re-encodes the unsigned envelope in **canonical CBOR** and recomputes BLAKE2b-256 hash
- Verifies **Ed25519 signature**
- Dedupes on (deviceId,msgId)
- Ingests into ADX `SensorRaw` (and optional `SensorPoints`)

## Install
```bash
npm i
cp .env.example .env
```

## Run
```bash
node index.js
```

## Environment variables
See `.env.example`.

