# RP-02 MycoBrain JWT authorization repair — September 29, 2026

Status: narrowly repaired and tested locally; not deployed. The larger RP-02 pairing/identity/private-read work package remains open.

Baseline: `4867dd95e6a8459d1603ff068c98ef2f0b0cdcf7`, branch `codex/mycobrain-auth-audit`, independent `git clone --no-hardlinks` at `C:/Users/Owner1/.codex/worktrees/system-audit-repairs/CODE/mycobrain-auth-repair`. Original source: `D:/Users/admin2/Desktop/MYCOSOFT/CODE/mycobrain`. The auth/config files matched HEAD before work; original dirty entrypoint, heartbeat, registry and SideB firmware files were neither copied into the baseline nor edited.

The pre-fix plan was saved before source edits at `C:/Users/Owner1/Documents/Codex/2026-09-29/realtime-voice-chat/outputs/mycobrain-auth-repair-plan.md`. This batch was explicitly limited to missing-JWKS authorization. No commit, push, deployment, service restart or hardware command occurred.

## Exact changes

| File | Change / purpose |
|---|---|
| `agents/src/mycobrain_agent/http/auth_middleware.py` | Replace unverified JWT decode/success when JWKS URL is missing with `jwt.InvalidKeyError("jwks_not_configured")`. Existing `require_auth` translates this into HTTP 401 before command execution. One line added, three removed. |
| `agents/tests/test_auth_middleware.py` | Thirteen focused in-memory ASGI tests using the real auth dependency, fresh ephemeral RSA keys, fake JWKS responses and a fake command counter. No imported server composition root, serial, MQTT or hardware adapter. |
| `docs/MYCOBRAIN_AUTH_AUDIT_REPAIR_SEP29_2026.md` | This audit-linked change/validation/rollback record. |

`config.py`, explicit `auth_mode=none`, explicit pair-token behavior, JWT issuer/audience/algorithm policy, pairing responses, read permissions and host lifecycle were not changed. The existing host entrypoint failure remains documented under RP-01 and was not silently repaired or tested as working.

## Before and after evidence

Before the implementation change: **4 failed, 9 passed**. Each failing case sent an unsigned or signed fixture JWT while JWKS was `None` or the empty string; the dependency incorrectly allowed the fake command endpoint to return HTTP 200.

After the change: **13 passed**, one existing Starlette `python_multipart` deprecation warning. Cases cover:

- Missing/empty JWKS with unsigned and signed tokens: HTTP 401, zero fake command calls, no JWKS fetch.
- Configured fixture JWKS with unsigned, wrong-key or expired tokens: HTTP 401, zero fake command calls.
- Correctly signed, unexpired RSA fixture with matching configured key: HTTP 200, exactly one fake command call.
- Missing bearer: HTTP 401, zero fake command calls.
- Explicit `none` mode: retains existing success behavior.
- Explicit pair-token mode: missing/wrong fixture rejects; correct fixture accepts.

The test runner additionally asserts that `mycobrain_agent.__main__`, `mycobrain_agent.http.server`, serial bridge, MQTT client, `serial`, and `paho.mqtt.client` were not imported. Its Python audit hook rejects `socket.connect` and `socket.sendto`, except the socket module's Windows `_fallback_socketpair` connection used by asyncio. ASGI requests and JWKS responses are in-memory fixtures, not traffic to a service.

The file guard rejects Python `open` events for string/bytes paths whose lowercased basename is exactly `.env`, `agent.env`, or `.credentials.local`. It is not a general credential-read sandbox: other filenames, inherited environment values and access outside those audit events are not covered. The inspected tests load no production credential files and use fresh transient RSA fixtures. The historical test-log footer's broad credential-access wording refers only to this named-basename guard; source, tests and historical logs were not changed or rerun for this documentation correction.

Dependencies: Python 3.12, PyJWT **2.15.1**, existing cryptography **48.0.0**, FastAPI **0.111.1**, pytest **8.4.2**, pytest-asyncio **0.23.8**. PyJWT was absent, so only its wheel was installed via isolated pip into `work/mycobrain-auth/deps`, with no global or repository dependency modification. Test RSA keys are generated transiently in memory and never stored or printed.

Receipts under `C:/Users/Owner1/Documents/Codex/2026-09-29/realtime-voice-chat/work/mycobrain-auth/`: `run_auth_tests.py`, `auth-tests-before.txt`, `auth-tests-after.txt`, `auth-repair-receipt.json`. Scoped Git whitespace/diff checks pass.

## Limits and rollback

This fixes one fail-open branch, not complete device authorization. Pairing still returns placeholders; issuer/audience/key-lifecycle and private reads still require a separate defined contract. The original host composition remains broken; no installed firmware/service identity or deployed configuration was verified. Explicit unauthenticated mode remains explicit existing behavior. No statement is made that a device is newly protected before a separately approved deployment.

Rollback: discard the scoped patch or revert only the three listed files in this isolated branch. Keep original repositories and unrelated work untouched. There is no deployed change to roll back.
