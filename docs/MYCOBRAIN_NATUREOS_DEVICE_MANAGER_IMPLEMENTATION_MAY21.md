# MycoBrain × NatureOS Device Manager — Implementation Plan

**Date:** 2026-05-21
**Status:** Master plan for the multi-PR rollout that closes Stream 3 gaps from `three-stream_gap_plan_edc7b3af.plan.md` and the new asks from this session.
**Owner:** Morgan, with Claude executing.

## What this plan delivers

> Morgan's intent: "control them over the natureos device manager dashboard the same way I controlled the COM7 bench MycoBrain over serial — but now Jetson(OpenClaw)+MycoBrain pairs — and have all four devices visible at once, gated to my company login (`morgan@mycosoft.org` via Supabase Google OAuth)."

Three things in concert:

1. **Per-device structure in this repo** — every device gets its own folder under `mycobrain/devices/<device_id>/` with firmware notes, deploy runbook, calibration profile, current status doc, and link to its row on `mycosoft.com/natureos/devices`.
2. **Website implementation** — wire `app/natureos/devices/` and `app/api/mycobrain/` against the unified agent `:8787` (not just `:8003`); add OpenClaw live-control surface; reuse existing Supabase + `pathRequiresCompanyEmail` gating.
3. **OpenClaw live control** — from the device card on `/natureos/devices/<deviceId>`, an operator can fire `grip/release/position/calibrate/estop` and watch position + force feedback in real time, with the same auditability the agent's `/openclaw/action` endpoint already provides.

## Folder structure being added

```
mycobrain/
├── devices/                                 # NEW — per-device source of truth
│   ├── README.md                           # Index of all devices, status board
│   ├── _template/                          # Skeleton for new devices
│   │   ├── README.md                       # Identity, role, host, firmware build
│   │   ├── deploy.md                       # SSH/flash/install runbook
│   │   ├── calibration.json                # Saved calibration profile
│   │   ├── status.md                       # Current state, last probe, owner
│   │   └── secrets.env.example             # Per-device env without secrets
│   ├── mycobrain-jet-a/                    # Device #1 — Jetson + OpenClaw + Mushroom1
│   ├── mycobrain-pi-a/                     # Device #2 — Pi + OpenClaw + Hyphae1
│   ├── mycobrain-jet-b/                    # Device #3 — older Jetson, cold start
│   └── mycobrain-bench/                    # Device #4 — standalone serial on PC
│
├── natureos/                                # CONTRACTS shared with website team
│   ├── SUPABASE_AUTH_GATING.md             # NEW — how the @mycosoft.org gate works
│   ├── OPENCLAW_LIVE_CONTROL.md            # NEW — end-to-end claw control flow
│   └── WEBSITE_IMPLEMENTATION_SPEC.md      # NEW — exact website changes (file paths)
│
└── docs/
    └── MYCOBRAIN_NATUREOS_DEVICE_MANAGER_IMPLEMENTATION_MAY21.md   # THIS FILE
```

## The four devices (from `AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md`)

| # | device_id | Host | OpenClaw | Path to NatureOS today | Path after this lands |
|---|-----------|------|----------|------------------------|------------------------|
| 1 | `mycobrain-jet-a` | Jetson at `192.168.0.228` | yes | none | `:8787` direct + MQTT presence |
| 2 | `mycobrain-pi-a` | Pi at `192.168.0.123` | yes | none | `:8787` direct + MQTT presence |
| 3 | `mycobrain-jet-b` | older Jetson, not on net | n/a | none | `:8787` after `scripts/bootstrap-dead-jetson.sh` |
| 4 | `mycobrain-bench` | Windows PC over COM4 | no | legacy WS `:8765` | `:8787` agent + legacy WS preserved |

Today the website hits `lib/mycobrain-service-url.ts` → `http://192.168.0.196:8003`. That's a single MAS service that talks to whatever's plugged in via serial. After this plan, the website fans out: `:8787` per device-IP for the live agent, and `:8003` only for the bench/legacy fallback.

## PR rollout (this PR is #8)

| PR | Title | Scope |
|----|-------|-------|
| #4 | Unified host agent + NatureOS contracts | merged 5/22 |
| #5 | Follow-up + OpenClaw reconciliation | merged 5/22 |
| #3 | CVE-2026-31431 mitigation | merged 5/22 |
| #6 | Seeed OpenClaw firmware/daemon | open |
| #7 | Captive portal merged into MDP firmware | open |
| **#8** | **THIS — per-device structure + NatureOS impl spec** | **scaffolding only; no website code change yet** |
| #9 *(next)* | Website: route `/api/mycobrain/*` to `:8787` with fallback to `:8003`; OpenClaw panel | website repo |
| #10 | Website: live-control page mirror of COM7 serial UI but for Jetson+MycoBrain pair | website repo |
| #11 | Berto's mqtt-status.mycosoft.com — make a permanent NatureOS-internal mirror at `/natureos/devices/fleet/mqtt-live` | website repo |
| #12 | Fix `mycobrain-service-url.ts` to route per-device based on registry lookup, not hardcoded `192.168.0.196` | website repo |

Each website PR ships a strict contract — see `natureos/WEBSITE_IMPLEMENTATION_SPEC.md`.

## How Morgan controls all four devices live (target state)

1. Visit `https://mycosoft.com/natureos/devices`. Middleware sees no session → redirect to `/login`.
2. Click "Sign in with Google." Supabase OAuth flow. Email comes back. `isCompanyEmail()` checks `@mycosoft.org` / `@mycosoft.com` — Morgan's `morgan@mycosoft.org` passes.
3. Land on the fleet grid. Four cards: `mycobrain-jet-a`, `mycobrain-pi-a`, `mycobrain-jet-b` (grey/offline), `mycobrain-bench`.
4. Click `mycobrain-jet-a`. Detail page loads with:
   - Live telemetry chart (last 1h) — pulled from MQTT `mycosoft/devices/mycobrain-jet-a-001/telemetry`
   - **OpenClaw panel** (since `info.openclaw.available === true`) — Grip / Release / Position slider / Status / Calibrate / Estop buttons
   - **Command console** (the COM7-serial-style UI but now sending MDP via `:8787/command`)
   - MDP frame tail (debug)
   - Audit log
5. Click "Grip." Browser POSTs to `mycosoft.com/api/devices/mycobrain-jet-a/openclaw/action` with the operator's Supabase JWT. Website mints a per-action JWT scoped `openclaw:action`. Proxies to `http://192.168.0.228:8787/openclaw/action`. Agent verifies, audits, sends MDP `claw_grip` (0x0030) over UART to Side A. Side A's servo closes. Agent publishes new state to MQTT. UI updates within ~500ms.
6. Same flow works on every device. The bench (Device #4) has `openclaw.available: false`, so the panel is hidden but command console still works.

## Berto's `mqtt-status.mycosoft.com` — what to do with it

Berto's status dashboard subscribes to `mycosoft/devices/+/presence` and renders a fleet view. That was a quick check, but the wiring is exactly what `/natureos/devices/fleet` needs. PR #11 mounts a server-side SSE mirror of the same MQTT topics under `/natureos/devices/fleet/mqtt-live`, gated to `@mycosoft.org`. The standalone `mqtt-status.mycosoft.com` host stays as a public-friendly read-only mirror (still useful for ops).

## Implementation gates from the gap plan

The Cursor gap plan lists these MycoBrain P0/P1 items — this plan closes them all:

| Gap-plan item | Closed by |
|----------------|-----------|
| P0: Website NatureOS :8787 — contracts not in WEBSITE repo | PR #9 (uses contracts already in `mycobrain/natureos/`) |
| P0: Fleet migration: 3 of 4 devices not migrated | Per-device folders in this PR + cutover runbooks in `mycobrain/devices/*/deploy.md` |
| P1: `mycobrain_agent` upstream/ MINDEX/NLM placeholder | tracked as PR #13 (out of this PR's scope) |
| P1: Captive portal merge into SideA_MDP | PR #7 (already open) |
| P1: Website default `mycobrain-service-url.ts` 196:8003 env drift | PR #12 |
| P1: Open PR #6 seeed-claw | open, just needs review |
| P2: Single 8003 vs 8787 decision-tree doc | Section "Path resolution" in `natureos/WEBSITE_IMPLEMENTATION_SPEC.md` |
| P2: Completion doc for May 19 fleet merge | `WHATS_NEW_MAY19_2026.md` already serves this |
| P2: Refresh device-firmware agent + `mycobrain-setup` skill | tracked as MAS-repo work, out of scope here |
| P2: MDP v1 vs v2.0.0 naming consistency | doc-only cleanup; track separately |
| P2: WebSocket for live device streams | PR #9 wires it through agent's `/ws/telemetry` |

## Acceptance criteria

When the full rollout lands:

1. Open `mycosoft.com/natureos/devices` as `morgan@mycosoft.org`. See all four MycoBrains.
2. From the detail page, fire an OpenClaw `grip` and see the position/force update in the UI within 500ms.
3. The bench device shows "no claw — bench" with the legacy command console still functional.
4. The dead Jetson shows "offline — needs bootstrap"; cold-boot runbook in its device folder.
5. Audit tab shows every action with operator email + outcome.
6. MQTT-status mirror at `/natureos/devices/fleet/mqtt-live` matches Berto's external dashboard.
