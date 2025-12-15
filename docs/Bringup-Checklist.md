# Bringup Checklist — MycoBrain V1

## Flash order

1. Side-A
2. Side-B
3. Gateway (optional)

## Build

```bash
cd firmware/side_a && pio run
cd ../side_b && pio run
cd ../gateway && pio run
```

## Common failures

- LoRa init fails: re-check SX1262 pinmap in `docs/Pinmap-MycoBrainV1.md`
- No telemetry: verify Side-A↔Side-B UART wiring + baud