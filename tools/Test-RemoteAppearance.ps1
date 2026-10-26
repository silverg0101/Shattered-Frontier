# Netcheck: a dedicated server and two clients on Silverbrook. Each client changes its face after spawn (as the stored face does live)
# and passes when it sees the other player's MetaHuman for that face (not the mannequin) at least 1 m away (spawn spread).
[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17863,
    [ValidateRange(60,900)][int]$TimeoutSeconds = 420
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repo 'GameClient\ShatteredFrontier.uproject'
$map = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
$root = Join-Path $repo ('Artifacts\RemoteAppearance\run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$null = New-Item -ItemType Directory -Path $root
$owned = @()
function Start-Owned([string]$Name, [string[]]$Extra) {
    $log = Join-Path $root "$Name.log"
    $args = @("`"$project`"") + $Extra + @('-unattended','-nosound','-nop4','-NullRHI',"-UserDir=`"$root\$Name-user/`"","-abslog=`"$log`"")
    $p = Start-Process -FilePath $editor -ArgumentList $args -WindowStyle Hidden -PassThru
    $null = $p.Handle
    $script:owned += $p
    return $log
}
try {
    $serverLog = Start-Owned 'server' @($map, '-server', "-port=$Port", '-multihome=127.0.0.1')
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while (-not ((Test-Path $serverLog) -and (Select-String -LiteralPath $serverLog -Pattern 'Bringing World .* up for play' -Quiet))) {
        if ($watch.Elapsed.TotalSeconds -gt $TimeoutSeconds) { throw 'Server did not start.' }
        Start-Sleep -Seconds 1
    }
    $logs = @{}
    foreach ($c in @(@{ n='client1'; face='Hilda' }, @{ n='client2'; face='Edwin' })) {
        $logs[$c.n] = Start-Owned $c.n @("127.0.0.1:$Port", '-game', '-RemoteAppearanceNetcheck', "-RemoteAppearanceFace=$($c.face)")
        Start-Sleep -Seconds 20
    }
    $results = @{}
    while ($results.Count -lt 2) {
        foreach ($n in $logs.Keys) {
            if ($results.ContainsKey($n) -or -not (Test-Path $logs[$n])) { continue }
            $hit = Select-String -LiteralPath $logs[$n] -Pattern 'REMOTE_APPEARANCE_(PASS|FAIL)' | Select-Object -First 1
            if ($hit) { $results[$n] = $hit.Matches[0].Groups[1].Value }
        }
        if ($watch.Elapsed.TotalSeconds -gt $TimeoutSeconds) { throw 'Timed out waiting for REMOTE_APPEARANCE results.' }
        Start-Sleep -Seconds 2
    }
    foreach ($n in $logs.Keys | Sort-Object) {
        Select-String -LiteralPath $logs[$n] -Pattern 'REMOTE_APPEARANCE |PLAYER_METAHUMAN_|PLAYER_FACE' | Select-Object -Last 3 | ForEach-Object { "${n}: $($_.Line)" }
        "${n}: $($results[$n])"
    }
    Select-String -LiteralPath $serverLog -Pattern 'SPAWN_SPREAD' | Select-Object -First 2 | ForEach-Object { "server: $($_.Line)" }
    "Logs: $root"
    if ($results.Values -contains 'FAIL') { exit 1 }
}
finally {
    foreach ($p in $owned) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } }
}
