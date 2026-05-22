# What's New — 2026-05-19

Built in one session. Read this first.

## TL;DR — what landed

A **unified host-agent layer** for every MycoBrain compute companion (Jetson Orin, older Jetson, Raspberry Pi, standalone PC over USB), plus the contracts for the NatureOS `/devices` page that drives them. The existing ESP32 firmware (Side A + Side B, MDP v2.0.0) is untouched and remains authoritative.

## Read these in order

1. **[`docs/PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md`](docs/PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md)** — the master plan. Folder layout, phased rollout, open questions. 10 min.
2. **[`docs/AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md`](docs/AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md)** — state of your 4 devices today and what each one needs. 5 min.
3. **[`docs/PORT_8787_HTTP_API_SPEC_MAY19_2026.md`](docs/PORT_8787_HTTP_API_SPEC_MAY19_2026.md)** — the API every device exposes (matches the .228 / .123 deployment). 5 min.
4. **[`docs/OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md`](docs/OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md)** — operator → NatureOS → agent → claw, with audit. 4 min.
5. **[`docs/NATUREOS_DEVICES_INTEGRATION_MAY19_2026.md`](docs/NATUREOS_DEVICES_INTEGRATION_MAY19_2026.md)** — what the website needs to implement so `/natureos/devices` works. 5 min.
6. **[`docs/MQTT_TOPIC_SCHEMA_MAY19_2026.md`](docs/MQTT_TOPIC_SCHEMA_MAY19_2026.md)** — pub/sub topics tying it all together. 3 min.

Adapter-specific:
- [`docs/RASPBERRY_PI_ADAPTER_GUIDE_MAY19_2026.md`](docs/RASPBERRY_PI_ADAPTER_GUIDE_MAY19_2026.md) — Pi bring-up
- [`docs/STANDALONE_SERIAL_AGENT_GUIDE_MAY19_2026.md`](docs/STANDALONE_SERIAL_AGENT_GUIDE_MAY19_2026.md) — bench MycoBrain on this PC

## Where the new code lives

```
agents/                                  ← Python package: mycobrain-agent
  pyproject.toml
  README.md
  src/mycobrain_agent/
    __main__.py                         entry point: python -m mycobrain_agent
    config.py                           env-driven settings
    core/
      mdp_codec.py                      wire-compatible port of common_mdp/mdp_codec.h
      serial_bridge.py                  Side A/B UART, COBS+CRC16, ACK tracking
      mqtt_client.py                    Paho async, WSS or LAN, presence + topics
      registry.py                       device identity + side links + telemetry
      heartbeat.py                      30s POST to MAS /api/devices/heartbeat
    adapters/
      base.py                           the Adapter Protocol
      jetson_orin.py                    Orin Nano Super / AGX
      jetson_legacy.py                  Nano 4GB / TX2 / pre-Orin
      raspberry_pi.py                   Pi 4 / Pi 5
      standalone.py                     PC over USB-CDC (MDP or legacy JSON)
    openclaw/
      client.py                         proxy to 127.0.0.1:8000 + audit JSONL
    http/
      server.py                         FastAPI on :8787 (all routes documented)
      auth_middleware.py                JWT / pair-token verification
    upstream/                           (placeholder — wire MINDEX, NLM next)
  tests/
    test_mdp_codec.py                   CRC + COBS + roundtrip checks
  deploy/
    systemd/
      mycobrain-agent.service           Linux service unit
      install.sh                        installer (Jetson + Pi)
    windows/
      install-service.ps1               NSSM-based Windows service
    env/
      jetson_orin.env.example
      jetson_legacy.env.example
      raspberry_pi.env.example
      standalone.env.example

natureos/                               ← Website integration contracts
  README.md
  api-contract.openapi.yaml             OpenAPI 3 spec (agent :8787 + website /api/devices)
  device-schema.json                    JSON Schema for the canonical Device record
  auth.md                               JWT / pair-token / mTLS flows
  ui-flows.md                           wireflows for fleet, detail, console, pair
  openclaw-ui.md                        OpenClaw control panel spec

scripts/                                ← Bootstrap (top-level)
  agent-install-jetson.sh               sudo bash scripts/agent-install-jetson.sh
  agent-install-pi.sh                   sudo bash scripts/agent-install-pi.sh
  agent-install-standalone.ps1          .\scripts\agent-install-standalone.ps1 -Port COM7
  bootstrap-dead-jetson.sh              cold-boot the dead older Jetson
```

## What's NOT in this PR (intentionally)

- **No website-side code** — that lives in the natureos / mycosoft-site repo. The contracts in `natureos/` are what their PR should implement.
- **No firmware changes** — the existing Side A + Side B MDP firmware is the source of truth. The agent talks to it as-is.
- **No actual probing of the live devices** — Claude couldn't reach `192.168.0.228` / `.123` (those are LAN-only and the cloud fetcher can't see them). The audit and the design assume the API surface you described; verify with Phase 2 of the plan.
- **No GitHub commit** — the GitHub MCP needs a token to push. Everything is in your local `mycobrain/` working tree; commit when ready.

## What to do next (in priority order)

1. **Skim the master plan and the audit** (15 min total) — confirm I read your fleet correctly.
2. **On the Jetson at .228** — `sudo lsof -iTCP:8787 -sTCP:LISTEN` + `systemctl status` and paste me the output, so we can diff the existing service against the unified agent. Same on `.123`.
3. **Get the GitHub token wired up** so I can push these files to `Mycosoft-Inc/mycobrain` (or whichever org owns this repo) and open a PR.
4. **Plug in this bench MycoBrain** — tell me the COM port and I'll either run the standalone installer here or hand you the exact command.
5. **Cold-boot the dead Jetson** when you're ready — `scripts/bootstrap-dead-jetson.sh` is the runbook.
6. **Hand the natureos/ contracts to whoever owns the website repo** — they have everything needed to wire `/natureos/devices` to render the fleet.

## Open questions the plan flagged for Garret + RJ

1. **Port 8787 origin** — the existing March docs reference 8080 / 8110 / 8120. The unified agent picks **8787** since that's what's actually deployed. Confirm or flag.
2. **OpenClaw real endpoint paths** — the client uses sensible defaults (`/gripper/open`, `/tasks/grasp`, etc.). If OpenClaw uses different paths, only `agents/src/mycobrain_agent/openclaw/client.py` changes.
3. **Agent home repo** — currently here in `mycobrain/agents/` (device-side code, lives with firmware). If MAS already owns the on-device operator, we should consolidate — proposal is: this agent is canonical, MAS imports it.
4. **Legacy bench MycoBrain (#4)** — keep JSON bridge running for the existing site buttons, or upgrade firmware to MDP now? Plan defaults to keep-and-bridge.

---

**Total deliverables:** 7 new docs, 1 OpenAPI contract, 1 JSON Schema, 4 UI/auth specs, 1 Python package with codec/adapters/HTTP/MQTT/OpenClaw scaffolds, 4 install scripts, 4 env templates, 1 systemd unit, 1 Windows service installer, 1 test module, updated top-level README.

## Follow-up shipped 2026-05-21

After PR #4 (https://github.com/MycosoftLabs/mycobrain/pull/4) landed, three more pieces were added to make the next session frictionless:

- **`scripts/install-github-mcp.ps1`** — automates installing the official `@modelcontextprotocol/server-github` MCP into Cowork's `claude_desktop_config.json`. Backs up existing config, prompts for or accepts a PAT, verifies on write, optionally restarts Cowork. After this runs, future Cowork sessions can `create_pr`, `push_file`, `list_issues`, etc. directly.
- **`tools/python/fleet_status.py`** — stdlib-only poller that hits every paired MycoBrain agent's `:8787/status` + `/info` and prints a single table (host / device_id / host_kind / side_a / side_b / openclaw / mqtt / agent_v). Use it during Phase 2/5 verification and as an ongoing health check.
- **`scripts/push-followup.ps1`** — pushes those two onto the same feature branch (or a fresh follow-up branch).

**Why these exist:**
- The GitHub MCP installer closes the loop on the only real bottleneck that surfaced during PR #4 — Cowork had no GitHub connector configured, so the push had to go through PowerShell. With this installed, the next session pushes from chat.
- `fleet_status.py` is the verification command for Phase 5 ("verification pass") in the master plan. It uses only stdlib so it runs anywhere Python runs — no `pip install` needed on the Jetsons.

**Run them:**
```powershell
# One-time: install the GitHub MCP into Cowork
.\scripts\install-github-mcp.ps1 -Token ghp_yourPATgoesHere -Restart

# Anytime: poll the fleet
python tools\python\fleet_status.py

# Commit + push these to the existing PR branch
.\scripts\push-followup.ps1
```

## Still owed (carried into a future session)

These need either probe output or a physical interaction Claude can't do from cloud tools:

- **COM4 probe** — run `python tools\python\probe_com.py COM4` on this PC and paste output (or save to `probe_com4.txt`). Tells us whether the bench MycoBrain is on MDP or legacy JSON firmware.
- **Two Jetson probes** — run `ssh jetson@192.168.0.228 'bash -s' < scripts\probe-jetson.sh > probe_jet228.txt` and the same for `.123`. Tells us exactly what's listening on `:8787` today, so we can diff against the new agent spec and write compat shims if needed.
- **Cut over** — once the diff is clear, install the unified agent on both Jetsons via `scripts/agent-install-jetson.sh` (or run dry-run alongside the existing service first).
- **Standalone bench install** — pick the COM port (4 per current session), run `scripts/agent-install-standalone.ps1 -Port COM4` after the probe confirms which protocol.
- **Older Jetson cold-boot** — `scripts/bootstrap-dead-jetson.sh` once the box is powered on.

When you come back, just run the probe block; outputs land in `.txt` files I'll read directly.
