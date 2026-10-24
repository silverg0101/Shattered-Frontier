[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(1024, 65535)][int]$Port = 17777,
    [ValidatePattern('^/Game/Frontier/[A-Za-z0-9_/]+$')][string]$Map = '/Game/Frontier/Maps/Greenlands_E1',
    [ValidateRange(1, 2)][int]$Clients = 2,
    [switch]$CastingScenario
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logs = Join-Path $repoRoot ('GameClient\Saved\Logs\E1-Smoke-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$null = New-Item -ItemType Directory -Path $logs
$ownedProcesses = [Collections.Generic.List[Diagnostics.Process]]::new()
function Launch([string[]]$LaunchArguments) {
    $process = Start-Process -FilePath $editor -ArgumentList (@('"' + $project + '"') + $LaunchArguments) -WindowStyle Hidden -PassThru
    $null = $process.Handle  # PS 5.1 keeps ExitCode only if the handle is opened before exit
    $ownedProcesses.Add($process)
    Write-Output "Started PID $($process.Id)"
}
function Wait-Log([string]$Path, [string]$Pattern, [int]$Count = 1) {
    $deadline = (Get-Date).AddSeconds(120)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path -LiteralPath $Path) {
            $matchesFound = @(Select-String -LiteralPath $Path -Pattern $Pattern)
            if ($matchesFound.Count -ge $Count) { return }
        }
        Start-Sleep -Milliseconds 500
    }
    throw "Timed out waiting for $Count matches of $Pattern in $Path"
}
try {
    if ($CastingScenario -and $Map -ne '/Game/Frontier/Maps/Sandbox/SB_Casting') { throw 'CastingScenario requires SB_Casting' }
    if (Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue) { throw "UDP $Port is already in use" }
    $serverLog = Join-Path $logs 'Server.log'
    Launch @($Map, '-server', '-NullRHI', '-unattended', '-nosound', '-nop4', "-port=$Port", '-MULTIHOME=127.0.0.1', ('-abslog="' + $serverLog + '"'))
    Wait-Log $serverLog 'E1_WORLD_READY.*NetMode=1'
    foreach ($index in 1..$Clients) {
        $clientLog = Join-Path $logs "Client$index.log"
        $clientArgs = @("127.0.0.1:$Port", '-game', '-NullRHI', '-unattended', '-nosound', '-nop4', ('-abslog="' + $clientLog + '"'))
        if ($CastingScenario -and $index -eq 1) { $clientArgs += '-CastingScenario' }
        Launch $clientArgs
        Wait-Log $clientLog 'Welcomed by server'
    }
    Wait-Log $serverLog 'E1_POSSESSION.*Pawn=SFE1Character.*Authority=1' $Clients
    $possessions = @(Select-String -LiteralPath $serverLog -Pattern 'E1_POSSESSION Controller=(\S+) Pawn=(SFE1Character\S*) Authority=1')
    $controllers = @($possessions | ForEach-Object { $_.Matches[0].Groups[1].Value } | Select-Object -Unique)
    $pawns = @($possessions | ForEach-Object { $_.Matches[0].Groups[2].Value } | Select-Object -Unique)
    if ($controllers.Count -ne $Clients -or $pawns.Count -ne $Clients) { throw "Expected $Clients distinct authoritative controller/pawn pairs" }
    if (@(Select-String -LiteralPath $serverLog -Pattern "E1_PLAYER_JOIN.*Players=$Clients.*Authority=1").Count -lt 1) { throw "Server did not report $Clients players" }
    if ($CastingScenario) {
        Wait-Log (Join-Path $logs 'Client1.log') 'CASTING_SCENARIO_PASS'
        Wait-Log $serverLog 'CASTING_SERVER_QUEUE Ability=2'
        Wait-Log $serverLog 'CASTING_SERVER_COMPLETE Ability=2'
        Write-Output "PASS: server-authoritative cast, queued channel and replicated training-target damage. Logs: $logs"
    }
    Write-Output "PASS: separate editor server and $Clients client(s) connected; $Clients server-authoritative possession(s). Logs: $logs"
    if (-not $CastingScenario) { Write-Output 'Headless connection check only: movement, rendering, lifecycle cleanup, authentication and Linux runtime are not certified.' }
} finally {
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
    }
}
