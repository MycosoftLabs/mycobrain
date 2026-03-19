# Jetson Optional Cortex Path

**Date:** March 7, 2026  
**Status:** Current  
**Related:** MycoBrain Rail Unification, MYCOBRAIN_JETSON_GATEWAY_BUILD_PLAN_MAR07_2026.md

## Principle: Identity Unchanged

**Device identity (device_id, device_name, device_role) lives on MycoBrain.** The Jetson is an **optional middle layer** for:

- Local inference (voice, vision, NLM)
- Aggregation (LoRa, WiFi gateways)
- Backhaul to MAS

The Jetson does **not** replace the MycoBrain as the canonical device. Heartbeats still originate from the MycoBrain service (or gateway) with the same device_id.

## Optional Cortex Path

```
MycoBrain (Side A + B) ──serial──► Gateway/PC (MycoBrain Service)
                                        │
                                        ├──► MAS heartbeat (device_id from MycoBrain)
                                        │
                                        └── [Optional] Jetson
                                                  │
                                                  ├── Local inference (voice, FCI)
                                                  ├── Forward telemetry to MAS
                                                  └── Never changes device_id
```

## Implementation Notes

- Jetson runs MycoBrain service or a gateway process that forwards heartbeats with the same device_id.
- If Jetson does local processing (e.g. FCI summarization), results are bridged via `POST /api/devices/{device_id}/fci-summary` — same device_id.
- See MYCOBRAIN_JETSON_GATEWAY_BUILD_PLAN_MAR07_2026.md for hardware and connection details.
