# Stage + commit + push the May-19-2026 mycobrain agent work.
# Run from the mycobrain repo root in PowerShell. Assumes you have git auth (gh, SSH, or HTTPS-creds).
#
# Usage:
#   .\scripts\push-to-github.ps1                       # default branch name
#   .\scripts\push-to-github.ps1 -Branch feat/agent-may19 -Remote origin
#   .\scripts\push-to-github.ps1 -OpenPR              # also opens a PR via gh CLI
[CmdletBinding()]
param(
  [string]$Branch = "feat/unified-agent-may19-2026",
  [string]$Remote = "origin",
  [string]$BaseBranch = "main",
  [string]$Message = "feat(agents+natureos): unified host agent for Jetson/Pi/standalone, port 8787, OpenClaw + NatureOS integration",
  [switch]$OpenPR
)

$ErrorActionPreference = "Stop"
Set-Location (Resolve-Path (Join-Path $PSScriptRoot ".."))

Write-Host "[1/6] Sanity-check git repo"
git rev-parse --git-dir | Out-Null
Write-Host "  HEAD: $(git rev-parse --short HEAD)  branch: $(git rev-parse --abbrev-ref HEAD)"

Write-Host "[2/6] Creating branch $Branch (or checking out if it exists)"
$existing = git branch --list $Branch
if ($existing) {
  git checkout $Branch
} else {
  git checkout -b $Branch
}

Write-Host "[3/6] Staging new files"
git add `
  README.md `
  WHATS_NEW_MAY19_2026.md `
  docs/PLAN_UNIFIED_DEVICE_FLEET_MAY19_2026.md `
  docs/AUDIT_FOUR_MYCOBRAINS_MAY19_2026.md `
  docs/PORT_8787_HTTP_API_SPEC_MAY19_2026.md `
  docs/OPENCLAW_INTEGRATION_GUIDE_MAY19_2026.md `
  docs/NATUREOS_DEVICES_INTEGRATION_MAY19_2026.md `
  docs/MQTT_TOPIC_SCHEMA_MAY19_2026.md `
  docs/RASPBERRY_PI_ADAPTER_GUIDE_MAY19_2026.md `
  docs/STANDALONE_SERIAL_AGENT_GUIDE_MAY19_2026.md `
  agents/ `
  natureos/ `
  scripts/agent-install-jetson.sh `
  scripts/agent-install-pi.sh `
  scripts/agent-install-standalone.ps1 `
  scripts/bootstrap-dead-jetson.sh `
  scripts/probe-jetson.sh `
  scripts/push-to-github.ps1 `
  tools/python/probe_com.py

Write-Host "[4/6] Diff summary"
git diff --cached --stat

Write-Host "[5/6] Commit"
git commit -m $Message -m "Adds the unified Python host agent (mycobrain-agent) with adapters for jetson_orin, jetson_legacy, raspberry_pi, and standalone. Locks port 8787 as canonical. Adds full NatureOS integration contract (OpenAPI + JSON Schema + auth + UI specs). Includes install scripts and per-host env templates. See WHATS_NEW_MAY19_2026.md."

Write-Host "[6/6] Push"
git push -u $Remote $Branch

if ($OpenPR) {
  Write-Host "Opening PR via gh CLI"
  gh pr create --base $BaseBranch --head $Branch --title "Unified MycoBrain host agent + NatureOS integration (May 19 2026)" --body-file WHATS_NEW_MAY19_2026.md
}

Write-Host ""
Write-Host "Done. To open a PR manually:"
Write-Host "  gh pr create --base $BaseBranch --head $Branch --title 'Unified MycoBrain host agent + NatureOS integration' --body-file WHATS_NEW_MAY19_2026.md"
