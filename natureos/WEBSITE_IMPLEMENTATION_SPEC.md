# Website Implementation Spec — `mycosoft-website` repo

## Status

This doc enumerates the exact website-side changes that close the MycoBrain × NatureOS integration. Owned by whoever ships PR #9 in the `mycosoft-website` repo (Garret/Morgan/Codex). Contract surface comes from `mycobrain/natureos/api-contract.openapi.yaml` (already shipped in PR #4).

## Existing routes (do not duplicate)

The website already has these under `app/natureos/devices/`:

```
[deviceId]/   — dynamic per-device detail page
alerts/       — fleet-wide alert feed
fleet/        — fleet grid
insights/     — aggregate analytics
map/          — geo view
network/      — network/MQTT view
onsite-ai/    — on-device AI status
registry/     — device registry CRUD
telemetry/    — telemetry browser
page.tsx      — top-level landing
```

And under `app/api/mycobrain/`:

```
[port]/       — port-keyed dynamic route
devices/      — device CRUD
events/       — event log
health/       — health probes
ports/        — known port list
route.ts      — root /api/mycobrain handler
```

The gap is **not new routes** — it's wiring them through to `:8787` (the unified agent) instead of only `:8003` (the legacy MAS service).

## Required code changes

### 1. `lib/mycobrain-service-url.ts` — multi-target resolver

Today:

```ts
const MYCOBRAIN_VM_LAN = "http://192.168.0.196:8003"
export function resolveMycoBrainServiceUrl(): string { ... single URL ... }
```

After:

```ts
// Resolve EITHER the legacy MAS service (single :8003) OR a per-device agent URL.
// Lookup priority:
//   1. If env override MYCOBRAIN_SERVICE_URL set → use as-is (back-compat)
//   2. Else, if a deviceId is provided AND the device registry has an `agent_url`
//      → return that (`http://<host-ip>:8787`)
//   3. Else → fall back to MYCOBRAIN_VM_LAN (192.168.0.196:8003)
export function resolveMycoBrainServiceUrl(deviceId?: string): string { ... }

// New helper for the unified agent endpoint specifically
export function resolveAgentUrl(deviceId: string): string | null { ... }
```

Device registry lookup goes through the existing `app/api/mycobrain/devices/` table (Drizzle).

### 2. Add `app/api/devices/[deviceId]/openclaw/action/route.ts`

Proxies to `<agent_url>/openclaw/action`. Mints a per-action JWT. Body matches the agent's `OpenClawActionRequest` from the OpenAPI spec.

```ts
import { mintActionJWT } from '@/lib/auth/device-jwt'
import { resolveAgentUrl } from '@/lib/mycobrain-service-url'

export async function POST(req: NextRequest, { params }) {
  const user = await requireCompanyUser(req)            // existing helper
  const deviceId = params.deviceId
  const agentUrl = await resolveAgentUrl(deviceId)
  if (!agentUrl) return new Response('not_found', { status: 404 })

  const body = await req.json()
  body.user_subject = user.email
  body.request_id = body.request_id || `nat-${crypto.randomUUID()}`

  const jwt = await mintActionJWT({
    sub: user.email,
    scope: 'openclaw:action',
    device_id: deviceId,
    exp: Math.floor(Date.now() / 1000) + 60,
  })

  const resp = await fetch(`${agentUrl}/openclaw/action`, {
    method: 'POST',
    headers: { Authorization: `Bearer ${jwt}`, 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  })
  return new Response(await resp.text(), { status: resp.status })
}
```

### 3. Add `app/api/devices/[deviceId]/command/route.ts`

Same pattern, scope `device:command`. Proxies to `<agent_url>/command`. Used by the COM7-style live command console.

### 4. Add `app/api/devices/[deviceId]/stream/route.ts`

Server-sent events stream. Subscribes to MQTT topics for the device and pushes events to the browser. Multiplexed kinds:

- `presence`, `telemetry`, `event`, `openclaw_state` — from MQTT
- `mdp_frame` — proxied from agent's `WS /ws/telemetry` when the detail page subscribes to it (debug panel only)

### 5. Add `app/api/devices/heartbeat/route.ts` (if not already)

Inbound from agents on devices. Body matches the agent's heartbeat shape. Updates the registry's `agent_url`, `last_seen`, `online`.

### 6. Add the OpenClaw panel component

`components/devices/OpenClawPanel.tsx` — UI spec in `mycobrain/natureos/openclaw-ui.md`. Mounted inside `app/natureos/devices/[deviceId]/page.tsx` when `info.openclaw.available === true`.

### 7. Add `app/.well-known/jwks.json/route.ts`

Returns the public keys used to sign per-action JWTs. Required for the agent to verify them.

### 8. Add `app/natureos/devices/fleet/mqtt-live/page.tsx`

A NatureOS-internal mirror of Berto's `mqtt-status.mycosoft.com` dashboard. Same MQTT subscriptions, gated to `@mycosoft.org`. Plus per-device click-through to the detail page.

## Pairing wizard

URL: `https://mycosoft.com/natureos/devices?pair=1`

Wizard steps:

1. Operator enters agent URL (e.g. `http://192.168.0.228:8787`) — or uses the LAN scan if available
2. Website POSTs to `<agent_url>/pair` with `claimed_by: user.email`, `natureos_pubkey`, `nonce`
3. Agent returns `device_id`, `host_kind`, `agent_pubkey`, signed nonce
4. Website creates the registry row, mints the long-lived device-paired JWT, stores encrypted
5. Wizard shows success, redirects to `app/natureos/devices/<device_id>`

## Env additions

```
MYCOBRAIN_DEVICE_REGISTRY_DB_URL=...     # Drizzle postgres URL
MYCOBRAIN_JWT_SIGNING_KEY=...            # for per-action JWTs (server-only)
MYCOBRAIN_MQTT_URL=wss://mqtt.mycosoft.com
MYCOBRAIN_MQTT_USERNAME=mycobrain
MYCOBRAIN_MQTT_PASSWORD=...
NEXT_PUBLIC_NATUREOS_DEVICES_BASE=https://mycosoft.com/natureos/devices
```

## Tests

- `e2e/devices-pairing.spec.ts` — happy-path pair flow, redirects, JWT issuance
- `e2e/devices-openclaw.spec.ts` — `morgan@mycosoft.org` can grip, non-company-email cannot, estop blocks subsequent actions until cleared
- `__tests__/mycobrain-service-url.test.ts` — resolver priority order (env override > device registry > fallback)

## What is NOT in this PR (next PRs)

- PR #12 — `mycobrain-service-url.ts` env drift fix (the `192.168.0.196:8003` hardcode)
- PR #13 — agent's MINDEX/NLM upstream/ wiring
- PR #14 — calibration profile push from website to device (apply calibration.json by clicking a button)
- PR #15 — firmware OTA via the captive portal endpoint (depends on PR #7 merged)

## Acceptance test

Once shipped, Morgan can:

1. Visit `https://mycosoft.com/natureos/devices` as `morgan@mycosoft.org`
2. See all four MycoBrains with correct presence dots
3. Click `mycobrain-jet-a`, fire a `grip` action, see position update in ≤1s
4. Click `mycobrain-bench`, fire a Side A `output_control` to flash LED red, see the LED change
5. View the audit tab and see his email associated with every action

That matches "the same way I controlled the COM7 bench MycoBrain over serial — but now Jetson+MycoBrain pairs."
