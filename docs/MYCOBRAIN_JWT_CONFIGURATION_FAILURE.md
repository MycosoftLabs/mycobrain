# Reject JWT writes when verification is not configured

JWT-authenticated write requests previously accepted a token when the JWKS URL was absent: the verifier decoded the claims without checking the signature. JWT mode now rejects this configuration with HTTP 401 before a command handler or JWKS request runs.

This repair addresses the missing-JWKS portion of system audit finding RP-02. It does not enable a device, change pairing, alter explicitly configured unauthenticated or pair-token modes, or establish deployed protection.

## Implementation

- `agents/src/mycobrain_agent/http/auth_middleware.py`: raise `jwt.InvalidKeyError("jwks_not_configured")` instead of the unverified-decode fallback. The existing dependency converts the verification failure into HTTP 401.
- `agents/tests/test_auth_middleware.py`: thirteen regression cases exercise the real authorization dependency through an in-memory ASGI application. Command execution and JWKS responses are fixtures; RSA keys are generated transiently.

Cases cover missing/empty configuration with signed and unsigned tokens, invalid signature and expiration, a valid configured fixture, absent bearer credentials, and the existing explicit authentication modes. All rejected cases assert that the fake command does not run.

## Reproduce

From the repository root, use a disposable Python environment and install the agent's declared development dependencies:

```sh
python -m venv .venv-auth
# Activate the environment using the command appropriate for your shell.
python -m pip install -e "./agents[dev]"
python -m pytest -q agents/tests/test_auth_middleware.py
```

The focused release check uses Python 3.12, PyJWT 2.15.1, cryptography 48.0.0, FastAPI 0.111.1, pytest 8.4.2 and pytest-asyncio 0.23.8. These are observed validation versions, not a new dependency lock.

## Validation

Against current upstream base `79b1fc00d61a6d068f791e4e4638305d26b47892`, the added suite reproduced four failures and nine passes before the source repair. After the repair, all thirteen cases pass, with one existing dependency deprecation warning. This is a new validation on the release base; the earlier audit base was `4867dd95e6a8459d1603ff068c98ef2f0b0cdcf7`.

The audit runner additionally blocked outbound socket calls, allowed the Windows asyncio socket-pair implementation, blocked file-open events for three named credential-file basenames, and checked that server composition and hardware modules were not imported. This is a bounded test guard, not an operating-system sandbox or a guarantee covering every credential filename. No production service or device was contacted.

## Remaining acceptance and rollback

Issuer, audience, accepted-algorithm policy, key rotation/revocation, private-read authorization, pairing identity and the host entrypoint remain separate work. These tests establish this dependency's behavior; they do not certify the full application, physical device identity, a deployed configuration or a firmware image.

After review, a deployment must provide an intentional authentication mode and verified key configuration. Do not rely on this patch to make explicit unauthenticated mode secure. No deployment is performed by this change.

Rollback by reverting the focused commit after reviewing the security consequence: reverting restores the missing-JWKS fail-open behavior. No database migration, credential rotation or device firmware rollback is required by this source change.
