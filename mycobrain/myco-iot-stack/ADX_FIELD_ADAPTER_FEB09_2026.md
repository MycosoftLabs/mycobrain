# ADX Field Adapter FEB09 2026

This folder (`myco-iot-stack/`) remains an optional, non-blocking path for field deployments:

- Local-first (default): device -> LoRa/MQTT/BLE/LTE -> Mycorrhizae/MAS -> MINDEX -> NatureOS/Website.
- Field/offsite (optional): device -> Azure IoT Hub -> Event Hubs-compatible endpoint -> (Node/Go ingestor here) -> ADX/TSI.

The key rule is that the canonical message is still `myco-envelope-v1` (deterministic bytes for hashing/signing), and verification/dedupe semantics must match the local path:

- Dedupe key: `(deviceId, seq, msgId)` (or the equivalent derived from the signed envelope)
- ACK contract: `accepted` ACK enables durable tail advancement on the device

## When to enable this adapter

- You are deploying offsite, and local hardware brokers/gateways are not feasible.
- You need a managed ingestion endpoint and can tolerate cloud dependencies.

## When NOT to enable it

- Local test site development, where MQTT/LoRa gateways and local servers are available.

## Operational notes

- Keep device public keys in a publishable registry (public keys only).
- Rotate keys with a grace window; verification should accept old keys briefly during rollout.
