# `natureos/` — Website integration contracts

This directory holds the **contracts** between the MycoBrain device fleet and the website at `mycosoft.com/natureos/devices`. The website-side code lives in a different repo; this folder is the source of truth for what that code must implement.

| File | Purpose |
|------|---------|
| `api-contract.openapi.yaml` | Machine-readable OpenAPI 3 spec for both the agent's `:8787` surface AND the website's `/api/devices/*` proxy endpoints |
| `device-schema.json` | JSON Schema for the canonical Device record (used by MAS, MINDEX, NatureOS UI) |
| `auth.md` | JWT issuance, pair token, device pairing, optional mTLS |
| `ui-flows.md` | UI wireflows for the fleet list, detail, command console, pair wizard |
| `openclaw-ui.md` | OpenClaw control panel spec |

Companion docs in `../docs/`:

- `PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md` — master plan
- `PORT_8787_HTTP_API_SPEC_MAY19_2026.md` — agent API in prose
- `NATUREOS_DEVICES_INTEGRATION_MAY19_2026.md` — website integration in prose
- `OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md` — OpenClaw sequence
- `MQTT_TOPIC_SCHEMA_MAY19_2026.md` — pub/sub topic map
