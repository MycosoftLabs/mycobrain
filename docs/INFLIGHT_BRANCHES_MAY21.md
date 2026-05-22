# In-flight Branches as of 2026-05-21

**Snapshot of `MycosoftLabs/mycobrain` after PR #4 merged**, captured during the follow-up session.

| Branch | Owner | State | Files | Status | What it does |
|--------|-------|-------|-------|--------|--------------|
| `main` | — | HEAD `0f7da2a` | — | clean | Contains PR #4 (unified agent + NatureOS contracts) |
| `feat/unified-agent-may19-2026` | nodefather (Claude) | merged via PR #4 | — | can be deleted | The May 19 branch |
| `feat/agent-followup-may21` | nodefather (Claude) | LOCAL ONLY (commit `bd90277`) | scripts/install-github-mcp.ps1, scripts/auto-probe-all.bat, scripts/push-followup.ps1, tools/python/auto_probe_all.py, tools/python/fleet_status.py, docs/GITHUB_PAT_SETUP.md, docs/INFLIGHT_BRANCHES_MAY21.md, docs/OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md (revised), agents/src/mycobrain_agent/openclaw/client.py (revised), .gitignore | **needs push** (blocked on PAT write scope) | This session's follow-up work |
| `claude/integrate-seeed-claw-SPwlV` | nodefather (Claude Code?) | 1 commit ahead of main, **13 behind** | mdp_claw.h, install_openclaw.sh, mycobrain-openclaw.service, openclaw.json, openclaw skills (mycobrain-control, sensecraft-publish), Side A/B firmware mods, SEEED_OPENCLAW_INTEGRATION.md | **needs rebase** | Actual OpenClaw integration code — MDP claw commands 0x0030-0x003F, JSON↔MDP bridge on Side B, Node.js daemon at ws://127.0.0.1:18789 |
| `claude/patch-cve-2026-31431-VgbgI` | nodefather (Claude Code?) | 1 commit ahead, 2 behind | CVE detection/mitigation scripts, Kyverno + Falco k8s policies, GitHub Actions runner precheck | **PR #3 open** | Security patch for CVE-2026-31431 (Copy Fail) — orthogonal to OpenClaw work |

## Coordination notes

### 1. `claude/integrate-seeed-claw-SPwlV` and this session's follow-up

These two branches **don't conflict** — the seeed-claw branch adds firmware claw control (`firmware/common/mdp_claw.h`, OpenClaw daemon deploy) and this session's follow-up adds agent-side wiring (`agents/src/mycobrain_agent/openclaw/client.py`) plus tooling. They are complementary halves.

**Recommended merge order:**

1. Push and merge the `feat/agent-followup-may21` follow-up first (small, additive, no firmware changes)
2. Rebase `claude/integrate-seeed-claw-SPwlV` onto current main (will need to resolve a couple of doc conflicts since `OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md` was updated in both branches)
3. Open PR for seeed-claw, merge

The agent's OpenClaw client has been pre-revised in this session's follow-up to match the MDP claw commands defined in the seeed-claw branch. So once both land, no further reconciliation is needed.

### 2. `claude/patch-cve-2026-31431-VgbgI` (PR #3)

Independent. Can be merged anytime. The 2 commits behind main are tiny — quick rebase, then merge. CVE-2026-31431 is the "Copy Fail" kernel vuln, mitigation is a Falco rule + Kyverno policy + a runner precheck shell script. Recommended: merge soon since CVE patches lose value with delay.

### 3. The original `feat/unified-agent-may19-2026` branch

PR #4 is merged. The branch can be deleted on GitHub. Local working trees should switch back to `main` and re-pull.

## What changed in this session vs the May 19 set

| File | What changed | Why |
|------|--------------|-----|
| `docs/OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md` | Full revision (HTTP→MDP, port 8000→18789, action vocabulary) | Reconcile with seeed-claw branch reality |
| `agents/src/mycobrain_agent/openclaw/client.py` | Rewritten to send MDP claw commands via serial bridge (was HTTP to localhost:8000) | Same reason |
| `agents/src/mycobrain_agent/config.py` | (next pass) Remove `openclaw_base_url`, add `openclaw_daemon_ws` for awareness probe | Same |
| `docs/PORT_8787_HTTP_API_SPEC_MAY19_2026.md` | (next pass) Update OpenClaw action table to match | Same |
| `docs/GITHUB_PAT_SETUP.md` | NEW | Captures the read-only-by-default fine-grained PAT trap that bit twice today |
| `docs/INFLIGHT_BRANCHES_MAY21.md` | NEW (this file) | Tracks the parallel branches discovered via API |
| `scripts/install-github-mcp.ps1` | NEW | Auto-installs the official GitHub MCP into Cowork config |
| `scripts/auto-probe-all.bat` | NEW | One-double-click probe of COM4 + both Jetsons via paramiko |
| `scripts/push-followup.ps1` | NEW | Push helper that uses gh CLI auth |
| `tools/python/auto_probe_all.py` | NEW | Harness invoked by auto-probe-all.bat |
| `tools/python/fleet_status.py` | NEW | Stdlib-only fleet poller |
| `.gitignore` | Added `probe_*.txt`, `probe_done.flag`, `**/_*-RUN.bat` | Keep secrets and probe outputs out of git |
| `WHATS_NEW_MAY19_2026.md` | Appended "Follow-up shipped 2026-05-21" + "Still owed" sections | Capture the post-merge state |

## When push works (PAT write scope granted)

```bash
# from cloud sandbox
PAT='<new-token-with-write-scope>'
cd /sessions/gallant-amazing-tesla/mnt/mycobrain
git push "https://x-access-token:$PAT@github.com/MycosoftLabs/mycobrain.git" \
  feat/agent-followup-may21:feat/agent-followup-may21
# then via API
curl -X POST -H "Authorization: Bearer $PAT" \
  https://api.github.com/repos/MycosoftLabs/mycobrain/pulls \
  -d '{"title":"feat: agent follow-up (MCP installer, fleet status, OpenClaw reconciliation)",
       "head":"feat/agent-followup-may21","base":"main",
       "body":"<contents of WHATS_NEW_MAY19_2026.md follow-up section>"}'
```

Both steps are pre-staged. The moment the PAT has `Contents: write` and `Pull requests: write`, this is one command away from being live.
