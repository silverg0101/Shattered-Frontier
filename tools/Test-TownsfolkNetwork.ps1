[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17850,
    [ValidateRange(60,600)][int]$TimeoutSeconds = 360,
    [ValidateRange(30,300)][int]$SampleSeconds = 120,
    [ValidateRange(1,30)][double]$DayMinutes = 4,
    [ValidateRange(0.05,2)][double]$HourToleranceSeconds = 0.5,
    # Attack: the first client swings at a resident. Quest: the server runs the quest pilot on
    # content/quests/silverbrook_introductions.json (talk_resident) and the first client walks to the held resident.
    [ValidateSet('Attack','Quest')][string]$Leg = 'Attack'
)
# Townsfolk step 4 (AI Townsfolk doc section 9): a dedicated server on Greenlands_Silverbrook owns the town state; two
# NullRHI clients simulate residents on NM_Client. Passes when both clients agree on the hour
# (within HourToleranceSeconds of real time) and on market state, both see the server's danger cell, and residents claim
# Smart Objects on each client. The server runs a fast day (DayMinutes) from 10:00 with markets closing at 12:00, so the
# samples span an open and a closed market. Hand-off: the first client walks to a resident standing still and swings at
# it; both clients must see the same server actor (resident id and serial) with their own Mass copy hidden, and both
# must resume the resident on its schedule from the actor's last position once the server releases it.
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root = Join-Path $repoRoot ('Artifacts\Townsfolk\network-' + $Leg.ToLower() + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw 'Installed editor is missing.' }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) { throw 'Test port is already in use.' }
if (Test-Path -LiteralPath $root) { throw 'Evidence root exists.' }
$null = New-Item -ItemType Directory -Path $root
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$clock = [Diagnostics.Stopwatch]::StartNew()
$settings = '-ini:Game:[/Script/ShatteredFrontier.SFTownsfolkSettings]:'
$results = [ordered]@{
    status='Running'
    leg=$Leg
    scope='Mechanism only: dedicated editor server town state (clock, markets, one danger cell) replicated to two NullRHI NM_Client clients simulating residents. Attack hand-off and release (the resident fights back, then flees; no guards in Silverbrook yet). No rendering, looks or quest hand-off.'
    serverOverrides="GameDayMinutes=$DayMinutes StartHour=10 MarketCloseHour=12 DangerSeconds=1800 HandoffReleaseMetres=10 HandoffQuietSeconds=8, danger cell at (20,-20) m; clients keep shipped settings"
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
function Read-Log([string]$Name) {
    # Copy first: the running editor holds the log open.
    $copy=Join-Path $root ($Name+'.read.log')
    Copy-Item -LiteralPath (Join-Path $root ($Name+'.log')) -Destination $copy -Force
    $text=Get-Content -LiteralPath $copy -Raw
    if ($text -match 'Fatal error:|Assertion failed:|Ensure condition failed:|TOWNSFOLK_DISABLED') { throw "$Name logged an engine failure or TOWNSFOLK_DISABLED." }
    return $text
}
function Read-Samples([string]$Text) {
    foreach ($m in [regex]::Matches($Text,'TOWNSFOLK_STATE Net=(\d+) Utc=([\d.]+) Day=(\d+) Hour=([\d.]+) OpenMarkets=(-?\d+) Danger=(\d+)')) {
        [pscustomobject]@{ Net=[int]$m.Groups[1].Value; Utc=[double]$m.Groups[2].Value; Hours=[int]$m.Groups[3].Value*24+[double]$m.Groups[4].Value
            Open=[int]$m.Groups[5].Value; Danger=[int]$m.Groups[6].Value }
    }
}
try {
    $map='/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
    $serverFlags=@('-server',('-port='+$Port),'-multihome=127.0.0.1',
        ($settings+'GameDayMinutes='+$DayMinutes),($settings+'StartHour=10'),($settings+'MarketCloseHour=12'),($settings+'DangerSeconds=1800'),
        ($settings+'HandoffReleaseMetres=10'),($settings+'HandoffQuietSeconds=8'),'-TownsfolkDangerX=20','-TownsfolkDangerY=-20')
    if ($Leg -eq 'Quest') {
        # The quest pilot is on in Silverbrook (GM-01); only the quest it runs changes.
        $serverFlags+=@('-ini:Game:[/Script/ShatteredFrontier.SFQuestPilotSettings]:QuestFile=silverbrook_introductions.json')
    }
    $server=Start-Owned 'server' $map $serverFlags
    Wait-Log $server 'server' 'TOWNSFOLK_STATE_CHANGE[^\r\n]*Cell='
    if ($Leg -eq 'Quest') { Wait-Log $server 'server' 'QUEST_PILOT_READY Quest=quest/silverbrook_introductions' }
    $first=Start-Owned 'first' ('127.0.0.1:'+$Port) @('-game',$(if ($Leg -eq 'Quest') { '-TownsfolkQuestScenario' } else { '-TownsfolkHandoffScenario' }))
    # Quest: every player gets the quest and shares the one held resident, so both walk to it and back.
    $second=Start-Owned 'second' ('127.0.0.1:'+$Port) @(@('-game')+$(if ($Leg -eq 'Quest') { @('-TownsfolkQuestScenario') } else { @() }))
    Wait-Log $first 'first' 'TOWNSFOLK_STATE Net=3'
    Wait-Log $second 'second' 'TOWNSFOLK_STATE Net=3'
    $sampleStart=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0+5  # skip the first seconds while server time syncs
    $until=$clock.Elapsed.TotalSeconds+$SampleSeconds+5
    while ($clock.Elapsed.TotalSeconds -lt $until) {
        Assert-Deadline
        foreach ($p in @($server,$first,$second)) { if ($p.HasExited) { throw 'An owned process exited during sampling.' } }
        Start-Sleep -Milliseconds 500
    }
    $serverText=Read-Log 'server'
    $changes=@([regex]::Matches($serverText,'TOWNSFOLK_STATE_CHANGE Utc=([\d.]+) Day=\d+ Hour=[\d.]+ OpenMarkets=(-?\d+)') | ForEach-Object { [double]$_.Groups[1].Value })
    $secondsPerHour=$DayMinutes*60/24
    $clients=[ordered]@{}
    foreach ($name in 'first','second') {
        $text=Read-Log $name
        if ($text -notmatch 'TOWNSFOLK_READY Residents=(\d+)[^\r\n]*Net=3 SmartObjects=SFClientSmartObjectSubsystem') { throw "$name did not start residents on NM_Client with the client SmartObject subsystem." }
        $residents=[int]$Matches[1]
        if ($text -notmatch 'TOWNSFOLK_SPOTS Doors=(\d+) Stalls=(\d+) Benches=(\d+)' -or [int]$Matches[1] -eq 0) { throw "$name created no Smart Objects." }
        $spots="Doors=$($Matches[1]) Stalls=$($Matches[2]) Benches=$($Matches[3])"
        $hours=@([regex]::Matches($text,'TOWNSFOLK_HOUR [^\r\n]*ProcessorMs=([\d.]+) DoorClaims=(\d+) StallClaims=(\d+) BenchClaims=(\d+)'))
        if ($hours.Count -lt 3) { throw "$name reported fewer than three game hours." }
        $ms=@($hours | Select-Object -Skip 1 | ForEach-Object { [double]$_.Groups[1].Value })  # the first hour holds the first-frame route planning
        $claims=0; foreach ($h in $hours) { $claims+=[int]$h.Groups[2].Value+[int]$h.Groups[3].Value+[int]$h.Groups[4].Value }
        if ($claims -eq 0) { throw "$name residents claimed no Smart Object slots." }
        $samples=@(Read-Samples $text | Where-Object { $_.Utc -ge $sampleStart })
        if ($samples.Count -lt $SampleSeconds/2) { throw "$name logged too few state samples ($($samples.Count))." }
        if (@($samples | Where-Object Danger -lt 1).Count) { throw "$name lost the server's danger cell." }  # the attack adds its own
        $clients[$name]=[ordered]@{ residents=$residents; spots=$spots; claims=$claims; hoursReported=$hours.Count
            processorMsMean=[math]::Round(($ms | Measure-Object -Average).Average,3); processorMsMax=($ms | Measure-Object -Maximum).Maximum
            samples=$samples; text=$text }
    }
    # Hand-off: one server actor (an attack, or the quest's held resident), seen by both clients with no second copy,
    # then released and resumed.
    if ($serverText -match 'TOWNSFOLK_HANDOFF_DENIED[^\r\n]*') { throw "Server denied a hand-off: $($Matches[0])" }
    $reason=if ($Leg -eq 'Quest') { 'quest' } else { 'attack' }
    if ($serverText -notmatch ('TOWNSFOLK_HANDOFF Id=(\S+) Serial=(\d+) Reason='+$reason)) { throw "Server made no $reason hand-off." }
    $handId=$Matches[1]; $serial=$Matches[2]; $health=$null
    if ($Leg -eq 'Attack') {
        if ($serverText -notmatch ('TOWNSFOLK_HANDOFF_HIT Id='+[regex]::Escape($handId)+' Serial='+$serial+' Health=(\d+)')) { throw 'Server applied no hit.' }
        $health=[int]$Matches[1]
    } else {
        # The quest: held for the objective, counted when the player reached it, quest complete, then let go.
        if ($serverText -notmatch ('QUEST_PILOT_TALK_HELD Role=stall_keeper Id='+[regex]::Escape($handId)+' Serial='+$serial)) { throw 'Quest pilot did not hold the resident.' }
        if ($serverText -notmatch ('QUEST_PILOT_EVENT Kind=talk_resident Target=resident_role/stall_keeper Op=talk:'+[regex]::Escape($handId)+':'+$serial+' Result=0')) { throw 'talk_resident objective was not accepted.' }
        if ($serverText -notmatch 'QUEST_PILOT_COMPLETE Quest=quest/silverbrook_introductions') { throw 'Quest did not complete.' }
        if ($clients.first.text -notmatch 'TOWNSFOLK_QUEST_SCENARIO_REACHED') { throw 'Client never reached the held resident.' }
    }
    if ($serverText -notmatch ('TOWNSFOLK_HANDOFF_RELEASE Id='+[regex]::Escape($handId)+' Serial='+$serial)) { throw 'Server never released the resident.' }
    $returns=@()
    foreach ($name in 'first','second') {
        $text=$clients[$name].text; $id=[regex]::Escape($handId)
        if ($text -notmatch ('TOWNSFOLK_HANDOFF_SEEN Net=3 Id='+$id+' Serial='+$serial)) { throw "$name never saw the hand-off actor." }
        $views=@([regex]::Matches($text,'TOWNSFOLK_HANDOFF_VIEW Id='+$id+' Serial='+$serial+' Health=\d+ Hidden=\d MassOutdoors=(\d)'))
        if ($views.Count -eq 0) { throw "$name logged no view of the hand-off actor." }
        if (@($views | Where-Object { $_.Groups[1].Value -ne '0' }).Count) { throw "$name showed the resident twice (Mass copy outdoors during the hand-off)." }
        if ($text -notmatch ('TOWNSFOLK_HANDOFF_RETURN Net=3 Id='+$id+' Serial='+$serial+' At=V\(X=([-\d.]+), Y=([-\d.]+)')) { throw "$name never returned the resident." }
        $returns+=,@([double]$Matches[1],[double]$Matches[2])
        $after=@([regex]::Matches($text,'TOWNSFOLK_HANDOFF_AFTER Id='+$id+' Outdoors=(\d) HandedOff=(\d)'))
        if (-not @($after | Where-Object { $_.Groups[1].Value -eq '1' -and $_.Groups[2].Value -eq '0' }).Count) { throw "$name did not show the resident back on its schedule." }
        $clients[$name].handoffViews=$views.Count
    }
    $gap=[math]::Sqrt([math]::Pow($returns[0][0]-$returns[1][0],2)+[math]::Pow($returns[0][1]-$returns[1][1],2))
    if ($gap -gt 100) { throw "Clients' last positions of the actor differ by $([math]::Round($gap)) cm." }
    $results.handoff=[ordered]@{ reason=$reason; resident=$handId; serial=[int]$serial; healthAfterHit=$health; returnGapCm=[math]::Round($gap,1) }
    # Hour: each of first's samples against second's hour interpolated at the same machine time.
    $a=$clients.first.samples; $b=$clients.second.samples; $worst=0.0; $compared=0; $marketCompared=0; $marketSkipped=0; $seen=@{}
    foreach ($s in $a) {
        $j=0; while ($j -lt $b.Count-1 -and $b[$j+1].Utc -lt $s.Utc) { $j++ }
        if ($j -ge $b.Count-1 -or $b[$j].Utc -gt $s.Utc) { continue }
        $t=($s.Utc-$b[$j].Utc)/[math]::Max(0.001,$b[$j+1].Utc-$b[$j].Utc)
        $other=$b[$j].Hours+$t*($b[$j+1].Hours-$b[$j].Hours)
        $worst=[math]::Max($worst,[math]::Abs($s.Hours-$other)*$secondsPerHour); $compared++
        # Market: both neighbours must agree with this sample, unless the server changed it within 1.5 s.
        if (@($changes | Where-Object { [math]::Abs($_-$s.Utc) -lt 1.5 }).Count) { $marketSkipped++; continue }
        if ($b[$j].Open -ne $s.Open -or $b[$j+1].Open -ne $s.Open) { throw "Market state differs at Utc $($s.Utc): $($s.Open) vs $($b[$j].Open)/$($b[$j+1].Open)." }
        $marketCompared++; $seen[$s.Open]=$true
    }
    if ($compared -lt $SampleSeconds/2) { throw "Too few overlapping samples ($compared)." }
    if ($worst -gt $HourToleranceSeconds) { throw "Clients' hours differ by $([math]::Round($worst,3)) s of real time (tolerance $HourToleranceSeconds)." }
    if ($seen.Count -lt 2) { throw 'Samples did not cover both an open and a closed market.' }
    foreach ($name in 'first','second') { $clients[$name].Remove('samples'); $clients[$name].Remove('text') }
    $results.clients=$clients
    $results.hour=[ordered]@{ compared=$compared; worstRealSeconds=[math]::Round($worst,3); toleranceSeconds=$HourToleranceSeconds }
    $results.market=[ordered]@{ compared=$marketCompared; skippedNearChange=$marketSkipped; serverChanges=$changes.Count }
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
