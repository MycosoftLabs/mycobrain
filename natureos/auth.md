# Auth — Website ↔ Agent

The website (`mycosoft.com`) is the JWT issuer. The agent on each device verifies. See `../docs/NATUREOS_DEVICES_INTEGRATION_MAY19_2026.md` for the integration picture.

## Three JWT types

| Type | Scope | TTL | Subject |
|------|-------|-----|---------|
| **Operator session** | `natureos:session` | 15 min (refreshable) | NatureOS user |
| **Per-request action** | `device:command` or `openclaw:action` | 60 s | NatureOS user |
| **Device-paired** | `device:agent` | 1 year | `device:<device_id>` |

All JWTs are EdDSA-signed (ed25519). The website publishes JWKS at `https://mycosoft.com/.well-known/jwks.json`. Agents fetch + cache.

## Pair-token mode (development / unattended)

For the standalone bench and any unattended deployment without JWT, the agent supports a single shared secret:

```bash
MYCOBRAIN_AUTH_MODE=pair_token
MYCOBRAIN_PAIR_TOKEN=<long-random-string>
```

Clients pass `X-Pair-Token: <token>` on every write. Use TLS or a trusted LAN.

## mTLS mode (high-security sites)

For unattended sites where we don't want any tokens-in-the-clear:

```bash
MYCOBRAIN_AUTH_MODE=mtls   # planned, not in v1 scaffold
MYCOBRAIN_TLS_CERT=/etc/mycobrain/agent.crt
MYCOBRAIN_TLS_KEY=/etc/mycobrain/agent.key
MYCOBRAIN_TLS_CA=/etc/mycobrain/mycosoft-ca.crt
```

The agent serves `:8787` with the per-device cert. The website's outbound proxy uses a client cert signed by the same CA. Both sides verify.

## Pairing flow (issues the device-paired JWT)

```
website POST /api/devices/{id}/pair
   → website calls agent POST /pair { claimed_by, natureos_pubkey, nonce }
   → agent returns { device_id, host_kind, agent_pubkey, signed_nonce }
   → website signs the long-lived JWT, stores it as the device's credential
   → website pushes JWT back to the agent (the agent stores it for cross-call use,
     e.g. heartbeat to MAS)
```

Once paired, the agent's `POST /pair` returns 410 until `RESET_PAIRING=1` is set on the host.

## Token rotation

The agent supports `POST /api/devices/{id}/rotate-key` (website-side). Mechanism:

1. Website mints a new key, signs replacement JWT
2. Website POSTs to agent's `/rotate-key` (auth: old JWT)
3. Agent accepts, persists new pubkey, returns new signed nonce
4. From now on, only the new JWT is accepted

(v1 scaffold leaves rotation as TODO; baked into the design.)
