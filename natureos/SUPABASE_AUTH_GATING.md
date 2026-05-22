# Supabase Auth Gating for `/natureos/devices`

## Status (2026-05-21)

The auth + company-email gate is **already implemented** in the website repo. This doc captures the contract so the mycobrain-side code (the agent, the device manager, OpenClaw control) can issue / verify JWTs that match.

## What exists today (website)

- `middleware.ts` — refreshes Supabase session on every request, redirects unauthenticated users to `/login`
- `lib/access/routes.ts` — `pathRequiresAuth()`, `pathRequiresCompanyEmail()` — canonical route gate
- `lib/access/types.ts` — `isCompanyEmail(email)` — accepts `@mycosoft.org` and `@mycosoft.com`
- `app/login/` — Google OAuth entry point
- `supabase/` — migrations + edge functions

## Allowed identity

For all `/natureos/*` routes (and especially `/natureos/devices`):

- User must be signed in with Supabase
- Email must match `@mycosoft.org` or `@mycosoft.com`
- For now, the only operator is `morgan@mycosoft.org`. Adding more is just adding rows to `auth.users` via Google OAuth and an explicit allowlist row in the upcoming `device_operators` table.

## How a device action gets authorized

```
Operator (browser, session JWT) → POST /api/devices/<id>/openclaw/action
                                          │
                                          ▼
Website middleware: verify session JWT, check isCompanyEmail
                                          │
                                          ▼
Route handler: mint per-action JWT (scope=openclaw:action, exp=60s,
               sub=operator email, device_id=<id>)
                                          │
                                          ▼
Proxy: POST http://<device-ip>:8787/openclaw/action
       Authorization: Bearer <per-action-jwt>
                                          │
                                          ▼
Agent (mycobrain-agent): verify JWT against JWKS at
       https://mycosoft.com/.well-known/jwks.json
                                          │
                                          ▼
Agent: audit log, execute MDP command, return result
```

## What the agent needs to verify a per-action JWT

Already implemented in `agents/src/mycobrain_agent/http/auth_middleware.py`. It reads `MYCOBRAIN_NATUREOS_JWKS_URL` (env), caches the JWKS, verifies signature, checks `exp` and `scope`. Required scopes are listed in the route's handler.

## JWKS endpoint

The website should expose `https://mycosoft.com/.well-known/jwks.json` returning the public keys used to sign per-action JWTs. PR #9 includes this if it's not already there.

## Adding new operators

1. The new operator signs in with Google via the website
2. They get a row in `auth.users`
3. An admin (Morgan) inserts a row into `device_operators(user_id, role, granted_by)` via Supabase SQL editor
4. From that point on, the company-email gate (which they already pass) plus the row presence opens device control to them
5. Audit log on every device action includes their email — no per-action allowlist check needed beyond JWT signature

## Pair-token mode (dev / unattended)

For development or sites that can't reach Supabase, the agent supports a pair-token mode (`MYCOBRAIN_AUTH_MODE=pair_token`). Used for the bench device today. The token is a single shared secret in `agent.env` — fine for a trusted LAN, NOT acceptable for any internet-exposed deployment.
