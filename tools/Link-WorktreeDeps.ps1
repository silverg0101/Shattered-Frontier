# Links git-ignored build dependencies from the main checkout into a git worktree,
# so the worktree compiles on its own (its own Binaries/Intermediate, no shared lock).
param([string]$Main = 'C:\Projects\Shattered-Frontier')
$ErrorActionPreference = 'Stop'
$here = (git rev-parse --show-toplevel).Replace('/', '\')
if ($here -ieq $Main) { 'main checkout: nothing to link'; return }
# Licensed, git-ignored Fab content the town builder needs (Environment/Greenlands/.gitignore).
foreach ($rel in 'Artifacts\ThirdParty', 'GameClient\Content\Frontier\Environment\Greenlands\Garden',
                 'GameClient\Content\Frontier\Environment\Greenlands\Village') {
    if (-not (Test-Path (Join-Path $Main $rel))) { "missing in main: $rel"; continue }
    $dst = Join-Path $here $rel
    if (Test-Path $dst) { "exists: $rel"; continue }
    New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
    New-Item -ItemType Junction -Path $dst -Target (Join-Path $Main $rel) | Out-Null
    "linked: $rel"
}

# Give this worktree's editor its own MCP port, so it doesn't collide with the main editor on 8000
# and this session's .mcp.json reaches its own editor, not another checkout's.
$md5 = [Security.Cryptography.MD5]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($here.ToLower()))
$port = 8001 + ([BitConverter]::ToUInt16($md5, 0) % 900)  # stable per path (GetHashCode is randomized)
$cfg = Join-Path $here 'GameClient\Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini'
New-Item -ItemType Directory -Force (Split-Path $cfg) | Out-Null
Set-Content $cfg "[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]`r`nServerPortNumber=$port`r`n"
git -C $here update-index --skip-worktree .mcp.json
(Get-Content (Join-Path $here '.mcp.json') -Raw) -replace '127\.0\.0\.1:\d+/mcp', "127.0.0.1:$port/mcp" | Set-Content (Join-Path $here '.mcp.json') -NoNewline
"editor MCP port: $port (restart this worktree's editor and Claude session to pick it up)"
