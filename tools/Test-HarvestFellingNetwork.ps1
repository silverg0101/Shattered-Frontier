[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17840,
    [ValidateRange(45,180)][int]$TimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root = Join-Path $repoRoot ('Artifacts\ConstructionSandbox\harvest-felling-network-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw 'Installed editor is missing.' }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) { throw 'Test port is already in use.' }
if (Test-Path -LiteralPath $root) { throw 'Evidence root exists.' }
$null = New-Item -ItemType Directory -Path $root
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$clock = [Diagnostics.Stopwatch]::StartNew()
$results = [ordered]@{
    status='Running'
    scope='Mechanism only: dedicated editor server, two concurrent NullRHI clients and one fresh late join; actual owned harvesting RPCs, owner-private progression and replicated resting pose. Collision source V1 visually rejected; no art, motion, sound, independent physics or production persistence acceptance.'
    startupOverride='Typed release-author setting enabled only on the fresh diagnostic server; clients retain the shipped disabled setting and follow replicated authority.'
    timeoutSeconds=$TimeoutSeconds
    runs=@()
}
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra) {
    $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $log = Join-Path $root ($Name+'.log')
    $arguments = @(('"'+$project+'"'),$Url,'-unattended','-NullRHI','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt'))
    $null = $process.Handle  # PS 5.1 keeps ExitCode only if the handle is opened before exit
    $owned.Add($process)
    $results.runs += [ordered]@{ name=$Name; pid=$process.Id; executable=$editor; arguments=$arguments; log=$log }
    return $process
}
function Assert-Deadline {
    if ($clock.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Total diagnostic deadline expired.' }
}
function Wait-Log([Diagnostics.Process]$Process,[string]$Name,[string]$Marker) {
    $log=Join-Path $root ($Name+'.log')
    while ($true) {
        Assert-Deadline
        if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern $Marker -Quiet)) { return }
        if ($Process.HasExited) { throw "$Name exited before $Marker." }
        Start-Sleep -Milliseconds 200
    }
}
function Wait-Exit([Diagnostics.Process]$Process,[string]$Name) {
    while (-not $Process.WaitForExit(200)) { Assert-Deadline }
    foreach ($run in $results.runs) { if ($run.name -eq $Name) { $run.exitCode=$Process.ExitCode } }
    if ($Process.ExitCode -ne 0) { throw "$Name exited $($Process.ExitCode)." }
}
function Read-Success([string]$Name) {
    $text=Get-Content -LiteralPath (Join-Path $root ($Name+'.log')) -Raw
    if ([regex]::Matches($text,'HARVEST_TREE_FALL_CLIENT_DONE').Count -ne 1 -or $text -match 'HARVEST_TREE_FALL_FAILED|Fatal error:|Assertion failed:|Ensure condition failed:') {
        throw "$Name did not pass the bounded felling scenario."
    }
    return $text
}
try {
    $url='/Game/Frontier/Maps/Greenlands_ConstructionSandbox?game=/Script/ShatteredFrontier.SFGameMode'
    $server=Start-Owned 'server' $url @('-server',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox','-GatheringPilot','-ini:Game:[/Script/ShatteredFrontier.SFHarvestTreeFallSettings]:bEnablePreparedOakFelling=True')
    Wait-Log $server 'server' 'GATHERING_PILOT_READY'
    $first=Start-Owned 'first' ('127.0.0.1:'+$Port) @('-game','-HarvestTreeFellingNetworkScenario')
    $second=Start-Owned 'second' ('127.0.0.1:'+$Port) @('-game','-HarvestTreeFellingNetworkScenario')
    Wait-Exit $first 'first'
    Wait-Exit $second 'second'
    $initial=(Read-Success 'first')+"`n"+(Read-Success 'second')
    if ($initial -notmatch 'CLIENT_DONE Primary=1 LateJoin=0 Wood=3 Practice=3 Cuts=3 Charges=0 Revision=3 Generation=0 MatchingFalls=1 Resting=1 Physics=0[^\r\n]*Private=1 ReplayUnchanged=1' -or
        $initial -notmatch 'CLIENT_DONE Primary=0 LateJoin=0 Wood=0 Practice=0 Cuts=3 Charges=0 Revision=3 Generation=0 MatchingFalls=1 Resting=1 Physics=0[^\r\n]*Private=1') {
        throw 'Initial harvester/observer outcomes are incomplete (expects shipped pilot defaults).'
    }
    if ($server.HasExited) { throw 'Owned server exited before late join.' }
    Assert-Deadline
    $late=Start-Owned 'late' ('127.0.0.1:'+$Port) @('-game','-HarvestTreeFellingNetworkScenario','-HarvestTreeFellingLateJoin')
    Wait-Exit $late 'late'
    $lateText=Read-Success 'late'
    if ($lateText -notmatch 'CLIENT_DONE Primary=0 LateJoin=1 Wood=0 Practice=0 Cuts=3 Charges=0 Revision=3 Generation=0 MatchingFalls=1 Resting=1 Physics=0[^\r\n]*Private=1 ReplayUnchanged=0 HistoricalFragments=0') {
        throw 'Late join depleted/resting/private/historical-fragment evidence incomplete.'
    }
    $serverText=Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
    if ([regex]::Matches($serverText,'GATHERING_PILOT_OUTCOME Operation=[123] Result=2').Count -ne 3 -or
        $serverText -match 'Fatal error:|Assertion failed:|Ensure condition failed:') { throw 'Server must commit exactly three real harvests without engine assertions.' }
    $results.status='Passed'
} catch {
    $results.status='Failed'; $results.error=$_.Exception.Message; throw
} finally {
    foreach ($process in $owned) {
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id
            if (-not $process.WaitForExit(5000)) { $results.status='Failed'; $results.cleanupError='Owned process did not exit after termination.' }
            foreach ($run in $results.runs) { if ($run.pid -eq $process.Id) { $run.stoppedByRunner=$true } }
        }
    }
    $results.elapsedSeconds=$clock.Elapsed.TotalSeconds
    $results.cleanupComplete=@($owned | Where-Object { -not $_.HasExited }).Count -eq 0
    $results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($results.status)"
}
