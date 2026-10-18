[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9.-]*$')][string]$ServerAddress,
    [ValidateRange(1024, 65535)][int]$Port = 17777,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logs = Join-Path $repoRoot ('GameClient\Saved\Logs\E1-Remote-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$null = New-Item -ItemType Directory -Path $logs
$ownedProcesses = [Collections.Generic.List[Diagnostics.Process]]::new()
try {
    foreach ($index in 1..2) {
        $clientLog = Join-Path $logs "Client$index.log"
        $arguments = @(('"' + $project + '"'), "${ServerAddress}:$Port", '-game', '-NullRHI', '-unattended', '-nosound', '-nop4', ('-abslog="' + $clientLog + '"'))
        $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
        $ownedProcesses.Add($process)
        Write-Output "Client$index PID $($process.Id)"
        $deadline = (Get-Date).AddSeconds(120)
        $connected = $false
        while ((Get-Date) -lt $deadline) {
            if ($process.HasExited) { throw "Client$index exited before successful connection; inspect $clientLog" }
            if ((Test-Path -LiteralPath $clientLog) -and (Select-String -LiteralPath $clientLog -Pattern 'Welcomed by server \(Level: /Game/Frontier/Maps/Greenlands_E1, Game: /Script/ShatteredFrontier.SFE1GameMode\)')) {
                $connected = $true
                break
            }
            Start-Sleep -Milliseconds 500
        }
        if (-not $connected) { throw "Client$index did not receive the expected E1 map/mode welcome; inspect $clientLog" }
    }
    # Leave time for the server to complete both pawn assignments before cleanup.
    Start-Sleep -Seconds 3
    foreach ($process in $ownedProcesses) {
        if ($process.HasExited) { throw 'A client exited during the connection check' }
    }
    Write-Output "PASS: two independent clients received E1 welcomes from ${ServerAddress}:$Port. Logs: $logs"
    Write-Output 'Inspect server logs separately for unique authoritative possessions. This does not certify rendered movement, identity or capacity.'
} finally {
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
    }
}
