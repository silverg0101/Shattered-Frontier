[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9.-]*$')][string]$ServerAddress,
    [ValidateRange(1024, 65535)][int]$Port = 17777,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$ClientExecutable,
    [ValidateRange(3, 120)][int]$HoldSeconds = 3
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$standaloneClient = $PSBoundParameters.ContainsKey('ClientExecutable')
if ($standaloneClient) {
    $clientItem = Get-Item -LiteralPath $ClientExecutable -ErrorAction Stop
    if ($clientItem.PSIsContainer -or $clientItem.Extension -ine '.exe') {
        throw 'ClientExecutable must resolve to an existing .exe file.'
    }
    $clientExecutablePath = [IO.Path]::GetFullPath($clientItem.FullName)
} else {
    $clientExecutablePath = $editor
}
$logs = Join-Path $repoRoot ('GameClient\Saved\Logs\E1-Remote-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$null = New-Item -ItemType Directory -Path $logs
$ownedProcesses = [Collections.Generic.List[Diagnostics.Process]]::new()
$clientLogs = [Collections.Generic.List[string]]::new()
try {
    foreach ($index in 1..2) {
        $clientLog = Join-Path $logs "Client$index.log"
        $clientLogs.Add($clientLog)
        if ($standaloneClient) {
            $arguments = @('ShatteredFrontier', "${ServerAddress}:$Port", '-NullRHI', '-unattended', '-nosound', '-nop4', ('-abslog="' + $clientLog + '"'))
        } else {
            $arguments = @(('"' + $project + '"'), "${ServerAddress}:$Port", '-game', '-NullRHI', '-unattended', '-nosound', '-nop4', ('-abslog="' + $clientLog + '"'))
        }
        $process = Start-Process -FilePath $clientExecutablePath -ArgumentList $arguments -WindowStyle Hidden -PassThru
        $null = $process.Handle  # PS 5.1 keeps ExitCode only if the handle is opened before exit
        $ownedProcesses.Add($process)
        Write-Output "Client$index PID $($process.Id)"
        $deadline = (Get-Date).AddSeconds(120)
        $connected = $false
        while ((Get-Date) -lt $deadline) {
            if ($process.HasExited) { throw "Client$index exited before successful connection; inspect $clientLog" }
            if (Test-Path -LiteralPath $clientLog) {
                $failure = Select-String -LiteralPath $clientLog -Pattern 'NetworkFailure:|Network Failure:|Server is incompatible with the local version|incompatible version of the game' -List
                if ($failure) { throw "Client$index reported a network or server compatibility failure: $($failure.Line). Inspect $clientLog" }
            }
            if ((Test-Path -LiteralPath $clientLog) -and (Select-String -LiteralPath $clientLog -Pattern 'Welcomed by server \(Level: /Game/Frontier/Maps/Greenlands_E1, Game: /Script/ShatteredFrontier.SFE1GameMode\)')) {
                $connected = $true
                break
            }
            Start-Sleep -Milliseconds 500
        }
        if (-not $connected) { throw "Client$index did not receive the expected E1 map/mode welcome; inspect $clientLog" }
    }
    # Leave time for the server to complete both pawn assignments before cleanup.
    $holdDeadline = [DateTime]::UtcNow.AddSeconds($HoldSeconds)
    while ([DateTime]::UtcNow -lt $holdDeadline) {
        foreach ($process in $ownedProcesses) {
            if ($process.HasExited) { throw 'A client exited during the connection hold' }
        }
        foreach ($clientLog in $clientLogs) {
            if (Select-String -LiteralPath $clientLog -Pattern 'NetworkFailure:|Network Failure:|Server is incompatible with the local version|incompatible version of the game' -Quiet) {
                throw "A client reported a network failure during the connection hold. Inspect $clientLog"
            }
        }
        Start-Sleep -Milliseconds 250
    }
    foreach ($process in $ownedProcesses) {
        if ($process.HasExited) { throw 'A client exited during the connection check' }
    }
    foreach ($clientLog in $clientLogs) {
        $failure = Select-String -LiteralPath $clientLog -Pattern 'NetworkFailure:|Network Failure:|Server is incompatible with the local version|incompatible version of the game' -List
        if ($failure) { throw "A client reported a network or server compatibility failure after welcome: $($failure.Line). Inspect $clientLog" }
    }
    Write-Output "PASS: two E1 welcomes; both client processes stayed alive without reported network failure for ${HoldSeconds}s at ${ServerAddress}:$Port. Logs: $logs"
    Write-Output 'Inspect server logs separately for unique authoritative possessions. This does not certify rendered movement, identity or capacity.'
} finally {
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
    }
}
