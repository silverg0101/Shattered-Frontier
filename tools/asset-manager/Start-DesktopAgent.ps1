# Starts host_agent.py for this desktop's RTX 3060 ComfyUI (the one Rust & Mercy's worker runs on :18190).
# The token is created once in %LOCALAPPDATA%\sfam-agent-token; paste it into the asset manager's Settings.
param(
    [string]$ComfyUrl = 'http://127.0.0.1:18190',
    [string]$ComfyDir = 'C:\Projects\Rust & Mercy\.vendor\comfy-h3',
    [string]$Log = 'C:\RustMercyWorker\logs\comfy-nvidia0.log',
    [int]$Gpu = 0,
    [int]$Port = 8291
)
$tokenFile = Join-Path $env:LOCALAPPDATA 'sfam-agent-token'
if (-not (Test-Path $tokenFile)) { [guid]::NewGuid().ToString('N') | Set-Content -NoNewline $tokenFile }
$token = (Get-Content $tokenFile -Raw).Trim()
Write-Host "Agent token: $token"
& py -3 (Join-Path $PSScriptRoot 'host_agent.py') --comfy $ComfyUrl --comfy-dir $ComfyDir --log $Log --gpu $Gpu --port $Port --token $token
