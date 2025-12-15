# MycoBrain V1 Firmware + Protocol

This repo contains production firmware targets for MycoBrain V1 and the Mycosoft Device Protocol (MDP v1).

## Targets

- **Side-A (ESP32-S3)**: sensors / IO, telemetry, command execution
- **Side-B (ESP32-S3)**: UART↔LoRa router (MDP frames)
- **Gateway (ESP32-S3 + SX1262)**: LoRa↔USB bridge (NDJSON output + command injector)

## Protocol

**MDP v1 framing (authoritative):** `COBS_ENCODE(payload + CRC16_LE) + 0x00` delimiter

CRC is **CRC16-CCITT-FALSE**.

See `docs/MycoBrainV1-Protocol.md`.

## Build (PlatformIO)

```bash
cd firmware/side_a && pio run
cd ../side_b && pio run
cd ../gateway && pio run
```

## Docs

- `docs/Pinmap-MycoBrainV1.md`
- `docs/Bringup-Checklist.md`
- `docs/MycoBrainV1-Protocol.md`

## Tools

- `tools/python/mdp_decode.py`
- `tools/python/mdp_send_cmd.py`