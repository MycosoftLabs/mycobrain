# Install the official GitHub MCP server into Cowork (Claude desktop) so future
# sessions can interact with GitHub natively — create PRs, push files, list
# issues, etc. — without you running scripts.
#
# What it does:
#   1. Locates %APPDATA%\Claude\claude_desktop_config.json
#   2. Reads it (or creates the skeleton if absent)
#   3. Merges in an entry for @modelcontextprotocol/server-github
#   4. Prompts for your GitHub PAT (if not passed) and stores it in the env block
#   5. Writes the config back atomically; offers to restart Cowork
#
# Usage:
#   .\scripts\install-github-mcp.ps1                          # prompts for PAT
#   .\scripts\install-github-mcp.ps1 -Token ghp_xxx           # non-interactive
#   .\scripts\install-github-mcp.ps1 -Token ghp_xxx -Restart  # restart Cowork after
#
# Get a PAT: https://github.com/settings/tokens → Generate new token → scopes:
# repo (full), workflow, read:org, read:user. Fine-grained tokens also fine if
# you scope to MycosoftLabs/mycobrain (and any other repos you want Claude to touch).
#
# Reversible: re-running with a new -Token overwrites the existing entry. To remove,
# edit the JSON and delete the "github" key under "mcpServers".

[CmdletBinding()]
param(
  [string]$Token,
  [switch]$Restart,
  [string]$ServerName = "github"
)

$ErrorActionPreference = "Stop"

# --- 1. Locate config ---
$configPath = Join-Path $env:APPDATA "Claude\claude_desktop_config.json"
$configDir  = Split-Path $configPath -Parent
if (-not (Test-Path $configDir)) {
  Write-Warning "$configDir does not exist — is Cowork (Claude desktop) installed?"
  exit 1
}

Write-Host "[1/5] Config path: $configPath"
if (-not (Test-Path $configPath)) {
  Write-Host "  No existing config — creating a fresh one."
  $config = [ordered]@{ mcpServers = [ordered]@{} }
} else {
  try {
    $config = Get-Content $configPath -Raw | ConvertFrom-Json -AsHashtable
  } catch {
    Write-Warning "Existing config is not valid JSON. Aborting so we don't trash it."
    Write-Warning "Open $configPath in Cursor, fix any syntax error, then re-run."
    exit 1
  }
  if (-not $config.ContainsKey("mcpServers")) {
    $config["mcpServers"] = [ordered]@{}
  }
}

# --- 2. PAT ---
if (-not $Token) {
  $secure = Read-Host -AsSecureString "Paste your GitHub PAT (input hidden)"
  $bstr = [System.Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
  try {
    $Token = [System.Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
  } finally {
    [System.Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
  }
}
if ([string]::IsNullOrWhiteSpace($Token)) {
  Write-Warning "No PAT provided. Aborting."
  exit 1
}
if (-not ($Token -match '^(ghp_|github_pat_)[A-Za-z0-9_]+$')) {
  Write-Warning "Token does not look like a GitHub PAT (expected ghp_... or github_pat_...). Continuing anyway."
}

# --- 3. Merge ---
Write-Host "[2/5] Merging MCP entry under mcpServers.$ServerName"
$config["mcpServers"][$ServerName] = [ordered]@{
  command = "npx"
  args    = @("-y", "@modelcontextprotocol/server-github")
  env     = [ordered]@{
    GITHUB_PERSONAL_ACCESS_TOKEN = $Token
  }
}

# --- 4. Backup + write atomically ---
if (Test-Path $configPath) {
  $backup = "$configPath.bak.$(Get-Date -Format yyyyMMddHHmmss)"
  Copy-Item $configPath $backup
  Write-Host "[3/5] Backed up existing config to $backup"
} else {
  Write-Host "[3/5] (No prior config to back up)"
}

$tmp = "$configPath.tmp"
$config | ConvertTo-Json -Depth 10 | Set-Content -Path $tmp -Encoding UTF8
Move-Item -Path $tmp -Destination $configPath -Force
Write-Host "[4/5] Wrote $configPath"

# --- 5. Verify ---
try {
  $verify = Get-Content $configPath -Raw | ConvertFrom-Json
  if (-not $verify.mcpServers.$ServerName.env.GITHUB_PERSONAL_ACCESS_TOKEN) {
    Write-Warning "Config wrote, but the github server entry is missing the PAT. Inspect $configPath."
    exit 1
  }
} catch {
  Write-Warning "Config wrote, but failed to re-parse. Inspect $configPath."
  exit 1
}
Write-Host "[5/5] Verified: mcpServers.$ServerName.env.GITHUB_PERSONAL_ACCESS_TOKEN present."

# --- Restart Cowork ---
if ($Restart) {
  Write-Host ""
  Write-Host "Restarting Cowork (Claude desktop)..."
  Get-Process -Name "Claude" -ErrorAction SilentlyContinue | Stop-Process -Force
  Start-Sleep -Seconds 2
  $coworkPath = Join-Path $env:LOCALAPPDATA "Programs\Claude\claude.exe"
  if (Test-Path $coworkPath) {
    Start-Process $coworkPath
    Write-Host "Restarted."
  } else {
    Write-Host "Couldn't find $coworkPath. Restart Cowork manually."
  }
} else {
  Write-Host ""
  Write-Host "Done. Restart Cowork manually (close it from the tray, reopen) — or run:"
  Write-Host "  .\scripts\install-github-mcp.ps1 -Restart"
}

Write-Host ""
Write-Host "After restart, in a fresh Cowork chat, ask Claude to list your MycosoftLabs repos."
Write-Host "If it succeeds, the GitHub MCP is wired in."
