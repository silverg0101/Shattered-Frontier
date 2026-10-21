[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17842,
    [ValidateRange(30,120)][int]$TimeoutSeconds = 120,
    [switch]$CaptureFrames
)
$ErrorActionPreference='Stop'
$repoRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $repoRoot ('Artifacts\ConstructionSandbox\gathering-skill-choices-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project=Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw 'Installed editor is missing.' }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) { throw 'Test port is already in use.' }
if (Test-Path -LiteralPath $root) { throw 'Evidence root exists.' }
$null=New-Item -ItemType Directory -Path $root
$owned=[Collections.Generic.List[Diagnostics.Process]]::new()
$clock=[Diagnostics.Stopwatch]::StartNew()
$capturePath=if ($CaptureFrames) { [IO.Path]::GetFullPath((Join-Path $root 'first-skills.png')) } else { '' }
$results=[ordered]@{ status='Running'; scope='Dedicated editor server and two concurrent clients: owned skill preference RPCs, local and authoritative cooldown rejection, independent skills and owner-only replication. Optional first-client rendered panel screenshot; no skill-value override, harvesting, UI-quality or durable progression acceptance.'; timeoutSeconds=$TimeoutSeconds; captureFrames=[bool]$CaptureFrames; runs=@(); captures=@() }
if ($CaptureFrames) { $results.capturePath=$capturePath }
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra,[string]$Capture='') {
    $user=Join-Path $root ($Name+'-user')
    $null=New-Item -ItemType Directory -Path $user
    $log=Join-Path $root ($Name+'.log')
    $arguments=@(('"'+$project+'"'),$Url,'-unattended')
    if ([string]::IsNullOrWhiteSpace($Capture)) { $arguments+='-NullRHI' }
    else { $arguments+=@('-RenderOffscreen','-ResX=1280','-ResY=720',('-GatheringSkillsCapture="'+$Capture+'"')) }
    $arguments+=@('-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    $process=Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt'))
    $null = $process.Handle  # PS 5.1 keeps ExitCode only if the handle is opened before exit
    $owned.Add($process)
    $results.runs += [ordered]@{ name=$Name; pid=$process.Id; executable=$editor; arguments=$arguments; log=$log }
    return $process
}
function Assert-Deadline { if ($clock.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Total diagnostic deadline expired.' } }
function Wait-Ready([Diagnostics.Process]$Process) {
    $log=Join-Path $root 'server.log'
    while ($true) {
        Assert-Deadline
        if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern 'GATHERING_PILOT_READY' -Quiet)) { return }
        if ($Process.HasExited) { throw 'Server exited before pilot readiness.' }
        Start-Sleep -Milliseconds 200
    }
}
function Wait-Exit([Diagnostics.Process]$Process,[string]$Name) {
    while (-not $Process.WaitForExit(200)) { Assert-Deadline }
    foreach ($run in $results.runs) { if ($run.name -eq $Name) { $run.exitCode=$Process.ExitCode } }
    if ($Process.ExitCode -ne 0) { throw "$Name exited $($Process.ExitCode)." }
}
try {
    $url='/Game/Frontier/Maps/Greenlands_ConstructionSandbox?game=/Script/ShatteredFrontier.SFGameMode'
    $server=Start-Owned 'server' $url @('-server',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox','-GatheringPilot','-GatheringSkillChoicesTrace')
    Wait-Ready $server
    $first=Start-Owned 'first' ('127.0.0.1:'+$Port) @('-game','-GatheringSkillChoicesScenario') $capturePath
    $second=Start-Owned 'second' ('127.0.0.1:'+$Port) @('-game','-GatheringSkillChoicesScenario')
    Wait-Exit $first 'first'
    Wait-Exit $second 'second'
    $combined=''
    foreach ($name in @('first','second')) {
        $text=Get-Content -LiteralPath (Join-Path $root ($name+'.log')) -Raw
        if ([regex]::Matches($text,'GATHERING_SKILL_CHOICES_CLIENT_DONE').Count -ne 1 -or
            [regex]::Matches($text,'GATHERING_SKILL_CHOICES_COOLDOWN').Count -ne 1 -or
            $text -match 'GATHERING_SKILL_CHOICES_FAILED|Fatal error:|Assertion failed:|Ensure condition failed:') {
            throw "$name did not pass the finite skill choices scenario."
        }
        if ($name -eq 'first' -and $CaptureFrames) {
            if ([regex]::Matches($text,'GATHERING_SKILLS_CAPTURE_DONE').Count -ne 1 -or -not (Test-Path -LiteralPath $capturePath -PathType Leaf)) { throw 'Rendered skills panel capture was not reported or saved.' }
            $png=[IO.File]::ReadAllBytes($capturePath)
            if ($png.Length -lt 24 -or [BitConverter]::ToString($png,0,8).Replace('-','') -ne '89504E470D0A1A0A') { throw 'Rendered skills panel output is not a PNG.' }
            $pngWidth=[BitConverter]::ToUInt32([byte[]]@($png[19],$png[18],$png[17],$png[16]),0)
            $pngHeight=[BitConverter]::ToUInt32([byte[]]@($png[23],$png[22],$png[21],$png[20]),0)
            if ($pngWidth -ne 1280 -or $pngHeight -ne 720) { throw "Rendered capture dimensions were ${pngWidth}x${pngHeight}; expected 1280x720." }
            $results.captures+= [ordered]@{ name='first-skills-panel'; path=$capturePath; width=$pngWidth; height=$pngHeight; bytes=$png.Length }
        }
        $combined+=$text+"`n"
    }
    foreach ($role in @(0,1)) {
        $skill=if ($role -eq 1) { 1 } else { 0 }
        $lock=if ($role -eq 1) { 2 } else { 1 }
        if ($combined -notmatch "COOLDOWN Primary=$role LocalGuard=1 ServerRejected=1 IndependentSkill=1 Private=1" -or
            $combined -notmatch "CLIENT_DONE Primary=$role Selected=$skill InitialLock=$lock FinalLock=0 Cooldown=1 Independent=1 Practice=0 Gains=0 Values=0 Private=1") {
            throw "Role $role preference, cooldown or private-state evidence incomplete."
        }
    }
    if ($server.HasExited) { throw 'Owned server exited unexpectedly.' }
    $serverText=Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
    if ($serverText -match 'GATHERING_PILOT_OUTCOME|Fatal error:|Assertion failed:|Ensure condition failed:') { throw 'Unexpected harvest outcome or server assertion.' }
    $results.status='Passed'
} catch { $results.status='Failed'; $results.error=$_.Exception.Message; throw }
finally {
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
