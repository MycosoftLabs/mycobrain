# Agent entrypoint and OpenClaw syntax recovery

Two tracked files were truncated: the agent entrypoint ended inside shutdown gathering, and the MDP OpenClaw client ended inside its action audit call. Both failed compilation. This four-file repair restores their missing definitions and adds inert contract tests. It does not qualify agent startup on a real host or authorize physical device operation.

## Recovered behavior

The entrypoint restores its `main()` function and module footer, gathers/cancels its owned asyncio tasks on completion or cancellation, and propagates a server failure after cleanup. **It constructs OpenClaw without a serial bridge.** The existing enabled-by-default setting cannot itself bind claw control. A separate reviewed integration must establish trusted device identity, firmware/schema compatibility, authorization, failsafes and audit policy before any physical binding.

The client retains the current MDP transport introduced by commit `f492b23`; it does not restore the obsolete HTTP client from `10e7c48`. Its availability predicate requires an explicitly supplied bridge and a boolean Side A link flag. That flag is connectivity evidence, not a trusted identity or safety qualification.

An action response needs an object payload with `success is True`. Missing frames, missing success, non-object payloads, false, integers and truthy strings do not become success. Positive replies are labeled `device_acknowledgement_unverified` and audited as `acknowledged`, not physical completion. The retained `completed_at` response field marks completion of the host call only. No position, force, calibration or actuator outcome is independently verified.

Audit receipt/start appends must succeed before dispatch. Audit errors are explicit; transport errors are sanitized. The file is appended and flushed, but this is not a globally unique, transactional, tamper-evident or power-loss-qualified audit system. The local estop latch is checked again after waiting for the action lock; latch changes require a positive reply. It remains process-local and is not a hardware safety interlock.

## Verification

Two baseline compile checks reproduced the truncation errors. Eighteen new tests now pass: parsing, inert entrypoint composition, shutdown/cancellation, server error propagation, held-unbound OpenClaw, link flags, strict replies, audit-before-dispatch, transport errors and local latch behavior. Eight payload variations are subtests within one test, not eight separately counted pytest cases.

```sh
python -B agents/tests/test_entrypoint_openclaw_recovery.py -v
```

With the declared agent test dependencies installed, the existing authentication suite can also be run from `agents/` with its `src` directory on the Python path:

```sh
python -m pytest --noconftest -q tests/test_auth_middleware.py tests/test_entrypoint_openclaw_recovery.py
```

All31 selected tests passed together:18new plus13unchanged JWT authentication regressions. The original authentication source/test bytes were preserved. An existing Starlette multipart deprecation warning remains. The local runner additionally blocked outbound socket calls, process launches, named credential-file reads and serial-device path opens; that guard is not an operating-system sandbox. All services, adapters and replies in the recovery tests are inert fixtures, with only disposable local audit files written.

## Unresolved runtime and device-deployment blockers

- Current `Settings` lacks `resolved_mqtt_client_id`, required during MQTT construction, and `mas_heartbeat_url`, required by heartbeat dispatch. A fake composition test does not establish real startup while these contracts are missing.
- `/info` still references retired `openclaw_base_url` when a client is available. The default unbound client avoids that branch but does not repair its contract.
- Generic `/command` still defaults absent success to true, and the serial bridge currently resolves pending replies by sequence without the full endpoint/type contract. These paths were not changed and must not be treated as qualified physical-command acknowledgement.
- The old guide promises HTTP405 for retired actions; the current HTTP handler maps this client's ValueError to400. This recovery prevents dispatch but does not qualify that older route-status contract.
- Registry link freshness/identity, physical emergency stops, parameter safety envelopes, persistence and durable audit, serial ownership, MQTT presence truthfulness and actual serial/MQTT resource shutdown require separate work. Cancelling asyncio fixtures does not prove that a real MQTT executor thread or serial handle has stopped.

No agent server, network connection, serial discovery, MQTT broker, device or production deployment was used to qualify this repair. Keep physical control unbound until those integration and safety contracts are verified.

## Rollback

Revert only this focused recovery diff if required, preserving the prior fail-closed JWT repair. That restores the known truncation errors and makes these modules unparsable; it is not a functional downgrade path for a running deployment. No runtime data, configuration, credentials, firmware or device state was migrated.
