# Bootstrap the mycobrain-agent on Windows (standalone PC adapter).
# Usage:
#   .\scripts\agent-install-standalone.ps1 -Port COM7
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string]$Port,
  [string]$Nickname = "bench"
)
$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
& (Join-Path $repoRoot "agents\deploy\windows\install-service.ps1") -Port $Port -Nickname $Nickname
