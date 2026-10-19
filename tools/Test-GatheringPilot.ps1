[CmdletBinding()]
param(
    [switch]$Refining,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17811,
    [ValidateRange(30,150)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root = Join-Path $repoRoot ('Artifacts\ConstructionSandbox\gathering-network-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw 'Installed editor is missing.' }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) { throw 'Test port is already in use.' }
if (Test-Path -LiteralPath $root) { throw 'Evidence root exists.' }
$null = New-Item -ItemType Directory -Path $root
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$results = [ordered]@{ status='Running'; scope='Two editor clients; real owned RPCs and replication; synthetic targeting; session-only inventory; no durable/Linux/native-input acceptance'; runs=@() }
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra) {
    $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $log = Join-Path $root ($Name+'.log')
    $arguments = @(('"'+$project+'"'),$Url,'-unattended','-NullRHI','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt'))
    $null = $process.Handle  # PS 5.1 keeps ExitCode only if the handle is opened before exit
    $owned.Add($process)
    $results.runs += [ordered]@{ name=$Name; pid=$process.Id; arguments=$arguments; log=$log }
    return $process
}
function Wait-Log([Diagnostics.Process]$Process,[string]$Name,[string]$Marker) {
    $phase = [Diagnostics.Stopwatch]::StartNew()
    $log = Join-Path $root ($Name+'.log')
    while ($phase.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern $Marker -Quiet)) { return }
        if ($Process.HasExited) { throw "$Name exited before $Marker." }
        Start-Sleep -Milliseconds 200
    }
    throw "Timed out waiting for $Name ($Marker)."
}
function Wait-Exit([Diagnostics.Process]$Process,[string]$Name) {
    $phase = [Diagnostics.Stopwatch]::StartNew()
    while (-not $Process.WaitForExit(200)) { if ($phase.Elapsed.TotalSeconds -gt $TimeoutSeconds) { throw "Timed out waiting for $Name." } }
    foreach ($run in $results.runs) { if ($run.name -eq $Name) { $run.exitCode=$Process.ExitCode } }
    if ($Process.ExitCode -ne 0) { throw "$Name exited $($Process.ExitCode)." }
}
try {
    $url='/Game/Frontier/Maps/Greenlands_ConstructionSandbox?game=/Script/ShatteredFrontier.SFConstructionNetworkGameMode'
    $server=Start-Owned 'server' $url @('-server',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox','-GatheringPilot')
    Wait-Log $server 'server' 'GATHERING_PILOT_READY'
    $scenarioFlag=if ($Refining) { '-GatheringRefiningScenario' } else { '-GatheringPilotScenario' }
    $first=Start-Owned 'first' ('127.0.0.1:'+$Port) @('-game',$scenarioFlag)
    $second=Start-Owned 'second' ('127.0.0.1:'+$Port) @('-game',$scenarioFlag)
    Wait-Exit $first 'first'; Wait-Exit $second 'second'
    $clientText=@()
    foreach ($name in @('first','second')) {
        $text=Get-Content -LiteralPath (Join-Path $root ($name+'.log')) -Raw
        if ($text -notmatch 'GATHERING_PILOT_CLIENT_DONE' -or $text -match 'GATHERING_PILOT_SCENARIO_FAILED|Fatal error:|Assertion failed:|Ensure condition failed:') { throw "$name did not pass the finite scenario." }
        $clientText+=$text
    }
    $all=$clientText -join "`n"
    if ($all -notmatch 'CLIENT_DONE Primary=1 Inventories=1 Wood=1 Ore=0 WoodRevision=1 OreRevision=1' -or
        $all -notmatch 'CLIENT_DONE Primary=0 Inventories=1 Wood=0 Ore=1 WoodRevision=1 OreRevision=1' -or
        $all -notmatch 'OBSERVER_PRIVATE_EMPTY Wood=0 Ore=0' -or $all -notmatch 'PRIMARY_REPLAY Operation=1') { throw 'Grant/replay/private inventory evidence incomplete (runner expects shipped pilot defaults).' }
    $serverText=Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
    if ([regex]::Matches($serverText,'GATHERING_PILOT_OUTCOME Operation=1 Result=2').Count -ne 2) { throw 'Server did not commit exactly two harvests.' }
    if ($Refining) {
        if ($all -notmatch 'REFINING_PILOT_CLIENT_DONE Primary=1 Wood=0 Ore=0 Timber=4 Ingots=0 SawmillRevision=1 ForgeRevision=1' -or
            $all -notmatch 'REFINING_PILOT_CLIENT_DONE Primary=0 Wood=0 Ore=0 Timber=0 Ingots=1 SawmillRevision=1 ForgeRevision=1' -or
            [regex]::Matches($serverText,'GATHERING_PILOT_OUTCOME Operation=2 Result=15').Count -ne 2) { throw 'Refinement outputs or exactly-once commits are incomplete.' }
    }
    $results.refining=[bool]$Refining
    $results.status='Passed'
} catch { $results.status='Failed'; $results.error=$_.Exception.Message; throw }
finally {
    foreach ($process in $owned) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit(); }
        foreach ($run in $results.runs) { if ($run.pid -eq $process.Id -and -not $run.Contains('exitCode')) { $run.stoppedByRunner=$true } }
    }
    $results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($results.status)"
}
