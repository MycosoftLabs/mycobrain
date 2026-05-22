# MycoBrain Agent — Windows service installer (standalone PC adapter)
# Uses NSSM to wrap the Python process as a Windows service.
# Run from an elevated PowerShell prompt.
#
# Usage:
#   .\install-service.ps1 -Port COM7

[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string]$Port,
  [string]$Nickname = "bench",
  [string]$PythonPath = "python"
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$progData = Join-Path $env:ProgramData "mycobrain"
$envFile  = Join-Path $progData "agent.env"
$venv     = Join-Path $progData "agent-venv"

Write-Host "[1/5] Creating dirs"
New-Item -ItemType Directory -Force -Path $progData | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $progData "logs") | Out-Null

Write-Host "[2/5] Setting up Python venv at $venv"
& $PythonPath -m venv $venv
& (Join-Path $venv "Scripts\pip.exe") install --upgrade pip
& (Join-Path $venv "Scripts\pip.exe") install -e (Join-Path $repoRoot "agents")

Write-Host "[3/5] Writing $envFile"
if (-not (Test-Path $envFile)) {
  Copy-Item (Join-Path $repoRoot "agents\deploy\env\standalone.env.example") $envFile
}
(Get-Content $envFile) -replace '^MYCOBRAIN_SIDE_A_PORT=.*', "MYCOBRAIN_SIDE_A_PORT=$Port" `
  -replace '^MYCOBRAIN_DEVICE_NICKNAME=.*', "MYCOBRAIN_DEVICE_NICKNAME=$Nickname" |
  Set-Content $envFile

Write-Host "[4/5] Installing as Windows service (requires NSSM in PATH)"
$python = Join-Path $venv "Scripts\python.exe"
$svc = "mycobrain-agent"
nssm install $svc $python "-m" "mycobrain_agent"
nssm set $svc AppDirectory $progData
nssm set $svc AppEnvironmentExtra "MYCOBRAIN_ENV_FILE=$envFile"
nssm set $svc Description "MycoBrain Agent (standalone)"
nssm set $svc Start SERVICE_AUTO_START

Write-Host "[5/5] Starting service"
Start-Service $svc

Write-Host "Done. Check: Get-Service $svc ; curl http://localhost:8787/status"
