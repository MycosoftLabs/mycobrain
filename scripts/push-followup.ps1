# Push the May-21 follow-up files (install-github-mcp + fleet_status + this script)
# as a small second commit onto the same feature branch, OR onto a new branch
# if the original PR is already merged.
#
# Usage:
#   .\scripts\push-followup.ps1                       # default branch / remote
#   .\scripts\push-followup.ps1 -NewBranch feat/agent-followup-may21

[CmdletBinding()]
param(
  [string]$Branch = "feat/unified-agent-may19-2026",
  [string]$NewBranch,
  [string]$Remote = "origin",
  [string]$Message = "feat(scripts+tools): GitHub MCP installer, fleet-status verifier",
  [switch]$OpenPR
)

$ErrorActionPreference = "Stop"
Set-Location (Resolve-Path (Join-Path $PSScriptRoot ".."))

$target = if ($NewBranch) { $NewBranch } else { $Branch }
Write-Host "[1/4] Checking out $target"
$existing = git branch --list $target
if ($existing) {
  git checkout $target
} else {
  git checkout -b $target
}

Write-Host "[2/4] Staging follow-up files"
git add `
  scripts/install-github-mcp.ps1 `
  scripts/push-followup.ps1 `
  tools/python/fleet_status.py

git status --short

Write-Host "[3/4] Commit"
git commit -m $Message -m "Adds:`n- scripts/install-github-mcp.ps1 — automates editing %APPDATA%\Claude\claude_desktop_config.json to add @modelcontextprotocol/server-github with a PAT, with backup + verify`n- tools/python/fleet_status.py — polls every paired MycoBrain on :8787 and prints a table (used for verification + ongoing ops)`n- scripts/push-followup.ps1 — this script"

Write-Host "[4/4] Push"
git push -u $Remote $target

if ($OpenPR -and $NewBranch) {
  gh pr create --base main --head $NewBranch --title "feat: GitHub MCP installer + fleet-status (May 21 follow-up)" --body "Two additions on top of PR #4:`n`n- ``scripts/install-github-mcp.ps1`` — one-shot installer for the official ``@modelcontextprotocol/server-github`` MCP into Cowork's claude_desktop_config.json. Backs up the existing file, prompts for or accepts a PAT, verifies on write.`n- ``tools/python/fleet_status.py`` — stdlib-only fleet poller. ``python tools/python/fleet_status.py`` prints a table of every MycoBrain agent on :8787."
}

Write-Host ""
Write-Host "Done."
