# OpenClaw — Live Control from NatureOS

## What Morgan wants

> "I want to control them over the NatureOS device manager dashboard the same way I controlled the COM7 bench MycoBrain over serial — but now Jetson(OpenClaw)+MycoBrain pairs."

That means: from `https://mycosoft.com/natureos/devices/<device_id>`, with a single click, fire a claw action and see the result update in the UI within ~500ms. Same UX as the bench-day serial console, just over WiFi/HTTP and across the whole fleet.

## End-to-end flow

```
┌────────────────────────────────────────────────────────────────────┐
│  Operator's browser                                                 │
│  /natureos/devices/mycobrain-jet-a-001                              │
│                                                                     │
│  [Grip] [Release] [Position: 90°] [Calibrate] [Estop]              │
│      │                                                              │
│      │  click "Grip"                                                │
│      ▼                                                              │
└──────│──────────────────────────────────────────────────────────────┘
       │  POST /api/devices/mycobrain-jet-a-001/openclaw/action
       │  { "action": "grip", "request_id": "ui-7d34c0e1" }
       │  Cookie: sb-<...> (Supabase session)
       ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Website (Next.js)                                                    │
│  app/api/devices/[deviceId]/openclaw/action/route.ts                  │
│                                                                       │
│  1. Verify session JWT via createServerClient                         │
│  2. Check isCompanyEmail(email)                                       │
│  3. Look up device in registry → agent_url = http://192.168.0.228:8787│
│  4. Mint per-action JWT (60s, scope=openclaw:action, sub=email)       │
│  5. POST <agent_url>/openclaw/action  Authorization: Bearer <jwt>     │
└──────│────────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Jetson at 192.168.0.228                                              │
│  mycobrain-agent.service :8787                                        │
│                                                                       │
│  1. Verify JWT against JWKS (cached from website's well-known)        │
│  2. Append "received" record to openclaw_audit.jsonl                  │
│  3. Single-flight lock; check estop_latched                           │
│  4. Send MDP COMMAND 0x0030 (claw_grip) to Side A via SerialBridge    │
│  5. Wait for ACK (≤ 3s)                                                │
│  6. Append "completed" record; publish state to MQTT                  │
│  7. Return { ok, request_id, audit_id, completed_at, result }         │
└──────│────────────────────────────────────────────────────────────────┘
       │  Side A (ESP32-S3) drives the servo (PCA9685 or LEDC PWM)
       │  Side A replies with ACK + new position
       ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Back up the chain                                                    │
│  Agent → website → browser                                            │
│                                                                       │
│  Browser updates the position readout from the MQTT subscription      │
│  (mycosoft/devices/mycobrain-jet-a-001/openclaw/state, retained)      │
│  AND from the action response.                                        │
└──────────────────────────────────────────────────────────────────────┘
```

## Latency budget

| Hop | Target | Notes |
|-----|--------|-------|
| Browser → website | ≤ 50ms | LAN or Cloudflare edge |
| Website → agent | ≤ 50ms | LAN |
| Agent serial bridge → Side A | ≤ 100ms | UART + COBS+CRC16 |
| Side A servo move | 200–500ms | physical actuation |
| Side A ACK back to agent | ≤ 100ms | |
| Agent → MQTT (retained state) | ≤ 50ms | |
| MQTT subscriber → browser | ≤ 100ms | SSE / WebSocket |

Total: ~600–900ms for a "grip" action, of which 200–500ms is the physical servo move.

## UI panel layout (website-side spec for PR #9)

Rendered iff `info.openclaw.available === true`:

```
┌─ OpenClaw ──────────────────────────────────── [● ready / red estop banner] ─┐
│                                                                              │
│ Position: ▓▓▓▓▓░░░░░  90° / 180°                                            │
│ Gripper:  [Closed] [Holding: no]                                            │
│ Force ADC: ▓░░░░░░░░░  240 / 3000                                           │
│                                                                              │
│ ─── Quick actions ──────────────────────────────────────                    │
│   [Grip]  [Release]  [Home]  [Estop]                                        │
│   [Position] ━━━━━━━●━━━━━━━ 90°  [Apply]                                  │
│                                                                              │
│ ─── Recent actions ─────────────────────────────────────                    │
│   14:22:30  morgan@   grip                    2.1s   ✓                      │
│   14:18:12  morgan@   release                 1.0s   ✓                      │
│   ...                                                                        │
└──────────────────────────────────────────────────────────────────────────────┘
```

Action vocabulary matches `docs/OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md`. Retired actions (`open`, `close`, `home`, `move_to`, `grasp`) return 405 with the new name as a hint.

## MQTT subscriptions the website holds

For each device card in the fleet view, the website maintains an SSE stream backed by Mosquitto subscriptions to:

- `mycosoft/devices/+/presence` — retained, drives dot color
- `mycosoft/devices/+/openclaw/state` — retained, drives panel readouts
- `mycosoft/devices/+/telemetry` — rolling, drives the chart
- `mycosoft/devices/+/events` — rolling, drives the alert banner

When a device card is opened (detail page), it also opens a WebSocket directly to the agent's `/ws/telemetry` for live MDP frame tail in the debug panel.

## Safety policies enforced in the agent

1. Every claw action requires a valid Supabase-signed per-action JWT.
2. Estop dominates — any claw action while `estop_latched=true` returns HTTP 423 until `clear_estop`.
3. Single-flight per device — concurrent claw actions get HTTP 429.
4. Rate-limited — default 5 actions / 10s burst, 60 / 5min sustained.
5. Audit-log every action (received → started → completed/failed) to `/var/log/mycobrain/openclaw_audit.jsonl`.

## Standalone bench (Device #4)

`mycobrain-bench` has no OpenClaw. `info.openclaw.available === false`. The panel is hidden. Commands to Side A still work via the Command Console (which sends MDP `output_control` for LED / buzzer / MOSFET — exactly the COM7-serial-test vocabulary).
