[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17841,
    [ValidateRange(320,600)][int]$TimeoutSeconds = 390
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repo 'GameClient\ShatteredFrontier.uproject'
$bridge = Join-Path $repo 'BackendServices\WorldContent.Tool\bin\Debug\net10.0\WorldContent.Tool.dll'
$sdk = Join-Path $repo 'Artifacts\Persistence\Toolchains\dotnet-10.0.402\dotnet.exe'
foreach ($required in @($editor,$project,$bridge,$sdk)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required pilot input missing: $required" }
}
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) {
    throw 'Test port is already in use.'
}
$root = Join-Path $repo ('Artifacts\CombatPilot\network-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $root
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$result = [ordered]@{ status='Running'; map='/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'; scope='One dedicated server, one client; local attack RPC and server-authored WorldContent slots'; runs=@() }
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra) {
    $log = Join-Path $root ($Name+'.log')
    $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $arguments = @(('"'+$project+'"'),$Url,'-unattended','-NullRHI','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt'))
    $null = $process.Handle
    $owned.Add($process)
    $result.runs += [ordered]@{ name=$Name; pid=$process.Id; log=$log; arguments=$arguments }
    return $process
}
function Wait-Log([Diagnostics.Process]$Process,[string]$Name,[string]$Pattern) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $log = Join-Path $root ($Name+'.log')
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern $Pattern -Quiet)) { return }
        if ($Process.HasExited) { throw "$Name exited before $Pattern." }
        Start-Sleep -Milliseconds 300
    }
    throw "Timed out waiting for $Name ($Pattern)."
}
try {
    $url = $result.map
    $server = Start-Owned 'server' $url @('-server',('-port='+$Port),'-multihome=127.0.0.1','-CombatPilotScenario')
    Wait-Log $server 'server' 'COMBAT_PILOT_READY Boars=10'
    $client = Start-Owned 'client' ('127.0.0.1:'+$Port) @('-game','-CombatPilotScenario')
    Wait-Log $client 'client' 'COMBAT_PILOT_CLIENT_ATTACK Index=3'
    Wait-Log $server 'server' 'COMBAT_PILOT_BOAR_DEAD Group=boar_farm_edge Slot=0'
    # Shipped HidesPerBoar is 2: the killer's bag gets them once, and the owning client sees them.
    Wait-Log $server 'server' 'COMBAT_PILOT_REWARD Death=boar_farm_edge:0:\d+ Hides=2 Granted=1 Total=2'
    Wait-Log $client 'client' 'COMBAT_PILOT_CLIENT_HIDES Count=2'
    Wait-Log $server 'server' 'COMBAT_PILOT_PLAYER_DEAD'
    Wait-Log $server 'server' 'COMBAT_PILOT_PLAYER_RESPAWN X=-35970 Y=-14180 HP=80'
    Wait-Log $server 'server' 'COMBAT_PILOT_SAFE_CHECK Protected=1 PlayerHP=80 BoarHP=60 Reward=0 Passed=1'
    Wait-Log $server 'server' 'COMBAT_PILOT_BOAR_SPAWN Group=boar_farm_edge Slot=0 Cycle=1'
    $serverText = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
    $clientText = Get-Content -LiteralPath (Join-Path $root 'client.log') -Raw
    if ([regex]::Matches($serverText,'COMBAT_PILOT_BOAR_HIT Group=boar_farm_edge Slot=0').Count -ne 3 -or
        [regex]::Matches($clientText,'COMBAT_PILOT_CLIENT_ATTACK Index=').Count -ne 3 -or
        [regex]::Matches($serverText,'COMBAT_PILOT_REWARD .* Granted=1').Count -ne 1 -or
        [regex]::Matches($clientText,'COMBAT_PILOT_CLIENT_HIDES Count=[1-9]').Count -ne 1 -or
        $serverText -match 'COMBAT_PILOT_SLOT_ERROR|Fatal error:|Assertion failed:') { throw 'Combat evidence incomplete or server failed.' }
    if ($server.HasExited -or $client.HasExited) { throw 'A network process exited before the final respawn barrier.' }
    $result.status='Passed'
} catch { $result.status='Failed'; $result.error=$_.Exception.Message; throw }
finally {
    foreach ($process in $owned) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
        foreach ($run in $result.runs) { if ($run.pid -eq $process.Id) { $run.exitCode=$process.ExitCode } }
    }
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($result.status)"
}
