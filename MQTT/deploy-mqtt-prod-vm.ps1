#Requires -Version 5.1
# Wrapper: loads MAS .credentials.local and runs deploy_mqtt_prod_guest.py (needs paramiko).
# Per CEO_DEPLOY_GUIDE.md — Docker Mosquitto on MQTT VM.
# Date: 2026-04-08

$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$masCreds = Join-Path (Split-Path $here -Parent) "..\MAS\mycosoft-mas\.credentials.local"
if (-not (Test-Path $masCreds)) {
  $masCreds = "C:\Users\admin2\Desktop\MYCOSOFT\CODE\MAS\mycosoft-mas\.credentials.local"
}
if (Test-Path $masCreds) {
  Get-Content $masCreds | ForEach-Object {
    if ($_ -match "^([^#=]+)=(.*)$") {
      [Environment]::SetEnvironmentVariable($matches[1].Trim(), $matches[2].Trim(), "Process")
    }
  }
}

$masRoot = if (Test-Path $masCreds) { Split-Path (Resolve-Path $masCreds) -Parent } else { "C:\Users\admin2\Desktop\MYCOSOFT\CODE\MAS\mycosoft-mas" }
$deployPy = Join-Path $here "deploy_mqtt_prod_guest.py"
Set-Location $masRoot
poetry run python $deployPy
