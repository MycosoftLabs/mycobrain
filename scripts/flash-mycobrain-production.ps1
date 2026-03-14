<#
.SYNOPSIS
    Compile and upload MycoBrain production firmware (Side A or Side B) by role and board.

.DESCRIPTION
    Uses PlatformIO to build and flash firmware for Mushroom 1 / Hyphae 1 boxes.
    Side A: role-specific builds (mushroom1 | hyphae1).
    Side B: single build (no role).

.PARAMETER Board
    Which board to flash: SideA or SideB.

.PARAMETER Role
    Device role (Side A only): mushroom1 or hyphae1. Ignored for SideB.

.PARAMETER Port
    Serial port (e.g. COM3, COM7). Auto-detects if not specified.

.EXAMPLE
    .\flash-mycobrain-production.ps1 -Board SideA -Role mushroom1 -Port COM7
    .\flash-mycobrain-production.ps1 -Board SideB -Port COM8
#>

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("SideA", "SideB")]
    [string]$Board,

    [Parameter(Mandatory = $false)]
    [ValidateSet("mushroom1", "hyphae1")]
    [string]$Role = "mushroom1",

    [Parameter(Mandatory = $false)]
    [string]$Port
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path "$ScriptDir/..").Path

# PlatformIO project paths
$SideAPath = Join-Path $RepoRoot "firmware/MycoBrain_SideA_MDP"
$SideBPath = Join-Path $RepoRoot "firmware/MycoBrain_SideB_MDP"

if (-not (Test-Path $SideAPath)) {
    Write-Error "Side A firmware not found at $SideAPath"
}
if (-not (Test-Path $SideBPath)) {
    Write-Error "Side B firmware not found at $SideBPath"
}

# Resolve project dir and PlatformIO env
$ProjectDir = $null
$EnvName = $null

if ($Board -eq "SideA") {
    $ProjectDir = $SideAPath
    $EnvName = $Role
} else {
    $ProjectDir = $SideBPath
    $EnvName = "esp32-s3-devkitc-1"
}

Write-Host "MycoBrain Production Flash" -ForegroundColor Cyan
Write-Host "  Board: $Board"
Write-Host "  Role:  $Role (used for Side A only)"
Write-Host "  Env:   $EnvName"
Write-Host "  Dir:   $ProjectDir"
Write-Host ""

# Check for pio
$pio = Get-Command pio -ErrorAction SilentlyContinue
if (-not $pio) {
    Write-Error "PlatformIO (pio) not found. Install: pip install platformio"
}

# Build
Write-Host "Building firmware..." -ForegroundColor Yellow
Push-Location $ProjectDir
try {
    & pio run -e $EnvName
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
} finally {
    Pop-Location
}

# Upload
Write-Host "Uploading to device..." -ForegroundColor Yellow
Push-Location $ProjectDir
try {
    if ($Port) {
        & pio run -e $EnvName -t upload --upload-port $Port
    } else {
        & pio run -e $EnvName -t upload
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Upload failed"
    }
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "Flash complete." -ForegroundColor Green
Write-Host "  Board: $Board | Role: $Role | Port: $(if ($Port) { $Port } else { 'auto' })"
