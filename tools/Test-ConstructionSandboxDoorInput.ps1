[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$EvidenceRoot,
    [ValidateRange(40,90)][int]$TimeoutSeconds = 85,
    [switch]$SkipNormalModeCheck
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$ownedRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'Artifacts\ConstructionSandbox'))
if (-not $EvidenceRoot) { $EvidenceRoot = Join-Path $ownedRoot ('door-input-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')) }
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($ownedRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'EvidenceRoot must be a new child under Artifacts/ConstructionSandbox.'
}
if (Test-Path -LiteralPath $EvidenceRoot) { throw 'EvidenceRoot already exists; refusing overwrite.' }
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Editor not found: $editor" }
if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { throw "Project not found: $project" }
$null = New-Item -ItemType Directory -Path $EvidenceRoot
$ownedProcesses = [Collections.Generic.List[Diagnostics.Process]]::new()
$results = [ordered]@{ status = 'Running'; engine = $editor; project = $project; runs = @(); scope = 'Automated bound input and actual CharacterMovement; NullRHI; standalone development; no human/rendered/packaged/network acceptance' }
function Invoke-FiniteGame([string]$Name, [string]$Map, [string[]]$Extra) {
    $log = Join-Path $EvidenceRoot ($Name + '.log')
    $arguments = @(('"' + $project + '"'), $Map, '-game', '-NullRHI', '-unattended', '-nosound', '-nop4',
        '-ConstructionDoorInputScenario', '-ExecCmds="t.MaxFPS 60"', ('-abslog="' + $log + '"')) + $Extra
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $EvidenceRoot ($Name + '.stdout.txt')) -RedirectStandardError (Join-Path $EvidenceRoot ($Name + '.stderr.txt'))
    $ownedProcesses.Add($process)
    $results.runs += [ordered]@{ name = $Name; pid = $process.Id; map = $Map; arguments = $arguments; log = $log; exitCode = $null; elapsedSeconds = $null }
    while (-not $process.WaitForExit(500)) {
        if ($watch.Elapsed.TotalSeconds -gt $TimeoutSeconds) { throw "$Name exceeded $TimeoutSeconds seconds (owned PID $($process.Id))." }
    }
    $process.WaitForExit()
    $results.runs[-1].exitCode = $process.ExitCode
    $results.runs[-1].elapsedSeconds = $watch.Elapsed.TotalSeconds
    if ($process.ExitCode -ne 0) { throw "$Name exited $($process.ExitCode); inspect $log" }
    if (-not (Test-Path -LiteralPath $log -PathType Leaf)) { throw "$Name produced no log." }
    $text = [IO.File]::ReadAllText($log)
    if ($text -match 'CONSTRUCTION_DOOR_INPUT_FAILED|Fatal error:|Assertion failed:') { throw "$Name recorded a scenario or engine failure." }
    return $text
}
try {
    $sourceFiles = @(
        'GameClient\Source\ShatteredFrontier\Private\Tests\SFConstructionDoorInputScenario.cpp',
        'GameClient\Source\ShatteredFrontier\Private\Construction\SFConstructionSandboxController.cpp',
        'GameClient\Source\ShatteredFrontier\Private\Construction\SFConstructionSandboxState.cpp',
        'GameClient\Source\ShatteredFrontier\Private\Interaction\SFE1PlayerController.cpp',
        'GameClient\Binaries\Win64\UnrealEditor-ShatteredFrontier.dll'
    )
    $results.provenance = @($sourceFiles | ForEach-Object {
        $path = Join-Path $repoRoot $_
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required source/build not found: $path" }
        [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    })
    if (-not $SkipNormalModeCheck) {
        $normal = Invoke-FiniteGame 'normal-mode-negative' '/Game/Frontier/Maps/Greenlands_E1' @('-testexit=E1_WORLD_READY')
        if ($normal -notmatch 'E1_WORLD_READY.*NetMode=0' -or $normal -match 'CONSTRUCTION_DOOR_INPUT_|CONSTRUCTION_TOOLS_READY|CONSTRUCTION_PILOT_READY') {
            throw 'Normal E1 flag isolation failed or normal mode did not become ready.'
        }
        $results.normalModeFlagIgnored = $true
    } else { $results.normalModeFlagIgnored = $null }
    $sandbox = Invoke-FiniteGame 'sandbox-input' '/Game/Frontier/Maps/Greenlands_ConstructionSandbox' @()
    function Number([string]$Value) { [double]::Parse($Value,[Globalization.CultureInfo]::InvariantCulture) }
    $expected = [ordered]@{
        DETERMINISTIC_SETUP=0; DOOR_SELECTION_STALE_DENIED=0; DOOR_YAW_STALE_DENIED=0; FOUR_YAWS_CONFIRMED=0
        DOOR_PLACED=1; TARGETED_DOOR_REMOVED=0; STRUCTURAL_SELECTION_STALE_DENIED=1; BUILD_OFF_DOOR_OPENED=1
        DOOR_CLOSED_WITHOUT_PROBE=1; CHARACTER_MOTION_BLOCKED=1; MEASURED_WALK_STARTED=1; CLOSED_DOOR_BLOCKED_WALK=1
        INPUT_RETREAT_CLEARED_SWING=1; WALKING_DOOR_OPENED=1; GROUNDED_SILL_TRAVERSAL=1; CLOSING_PUSH_SUCCEEDED=1
        DOOR_REOPENED_AFTER_PUSH=1; TRAPPED_CLOSE_PUSH_DENIED=1; PROBE_FALLBACK_EXACTLY_ONCE=1
    }
    $expectedRevisions = [ordered]@{
        DETERMINISTIC_SETUP=0; DOOR_SELECTION_STALE_DENIED=0; DOOR_YAW_STALE_DENIED=0; FOUR_YAWS_CONFIRMED=0
        DOOR_PLACED=1; TARGETED_DOOR_REMOVED=2; STRUCTURAL_SELECTION_STALE_DENIED=3; BUILD_OFF_DOOR_OPENED=4
        DOOR_CLOSED_WITHOUT_PROBE=5; CHARACTER_MOTION_BLOCKED=5; MEASURED_WALK_STARTED=5; CLOSED_DOOR_BLOCKED_WALK=5
        INPUT_RETREAT_CLEARED_SWING=5; WALKING_DOOR_OPENED=6; GROUNDED_SILL_TRAVERSAL=6; CLOSING_PUSH_SUCCEEDED=7
        DOOR_REOPENED_AFTER_PUSH=8; TRAPPED_CLOSE_PUSH_DENIED=8; PROBE_FALLBACK_EXACTLY_ONCE=8
    }
    $observed = @([regex]::Matches($sandbox,'CONSTRUCTION_DOOR_INPUT_STEP Name=(\w+) Doors=(\d+) Members=(\d+) Revision=(\d+) BaseEntries=(\d+) WorldSeconds=([\d.]+)'))
    if ($observed.Count -ne 19) { throw "Expected 19 semantic steps, observed $($observed.Count)." }
    foreach ($entry in $expected.GetEnumerator()) {
        $matchesForStep = @($observed | Where-Object { $_.Groups[1].Value -eq $entry.Key })
        $baseEntries = if ($entry.Key -eq 'PROBE_FALLBACK_EXACTLY_ONCE') { 1 } else { 0 }
        if ($matchesForStep.Count -ne 1 -or [int]$matchesForStep[0].Groups[2].Value -ne $entry.Value -or [int]$matchesForStep[0].Groups[3].Value -ne 0 -or [uint64]$matchesForStep[0].Groups[4].Value -ne [uint64]$expectedRevisions[$entry.Key] -or [int]$matchesForStep[0].Groups[5].Value -ne $baseEntries) {
            throw "Missing/duplicate or incorrect observed authority/base dispatch state for $($entry.Key)."
        }
    }
    $success = [regex]::Matches($sandbox,'CONSTRUCTION_DOOR_INPUT_SUCCESS Steps=19 Doors=1 BaseEntryDelta=1 AutomatedInput=1 ActualCharacterWalking=1 Rendered=0')
    if ($success.Count -ne 1) { throw 'Missing unique semantic input/walking success.' }
    $serverResults = [regex]::Matches($sandbox,'E1_INTERACTION_SERVER_RESULT[^\r\n]*Result=(\d+) Authority=1')
    if ($serverResults.Count -ne 1 -or $serverResults[0].Groups[1].Value -ne '0') { throw 'Expected exactly one accepted base probe server dispatch.' }
    $blockers = [regex]::Matches($sandbox,'CONSTRUCTION_DOOR_PUSH_BLOCKERS_READY Front=X=452\.500 Y=345\.000 Z=100\.000 Back=X=452\.500 Y=555\.000 Z=100\.000 Extent=60,5,100 MotionY=400\.\.520')
    if ($blockers.Count -ne 1) { throw 'Missing exact pair of owned closing-push path blockers.' }
    $pushStart = [regex]::Matches($sandbox,'CONSTRUCTION_DOOR_CLOSE_PUSH_START Revision=(\d+) Position=X=([\-\d.]+) Y=([\-\d.]+) Z=([\-\d.]+) VirtualDoorRay=1')
    $pushEnd = [regex]::Matches($sandbox,'CONSTRUCTION_DOOR_CLOSE_PUSH_END Revision=(\d+) Position=X=([\-\d.]+) Y=([\-\d.]+) Z=([\-\d.]+) HorizontalDelta=([\-\d.]+) CapsuleOutsideMotion=1')
    if ($pushStart.Count -ne 1 -or $pushEnd.Count -ne 1) { throw 'Missing unique close push start/end measurements.' }
    if ([uint64]$pushStart[0].Groups[1].Value -ne 6 -or [uint64]$pushEnd[0].Groups[1].Value -ne 7) { throw 'Actual E close did not advance the captured revision exactly once.' }
    if ([Math]::Abs((Number $pushStart[0].Groups[2].Value)-452.5) -gt 0.01 -or [Math]::Abs((Number $pushStart[0].Groups[3].Value)-455) -gt 0.01 -or [Math]::Abs((Number $pushStart[0].Groups[4].Value)-94) -gt 0.01) { throw 'Close push did not begin at the deterministic interior capsule position.' }
    if ([Math]::Abs((Number $pushEnd[0].Groups[2].Value)-(Number $pushStart[0].Groups[2].Value)) -gt 0.01 -or [Math]::Abs((Number $pushEnd[0].Groups[4].Value)-(Number $pushStart[0].Groups[4].Value)) -gt 0.01) { throw 'Close push changed the capsule horizontal axis or height unexpectedly.' }
    if ([Math]::Abs((Number $pushEnd[0].Groups[3].Value)-(Number $pushStart[0].Groups[3].Value)) -le 34 -or (Number $pushEnd[0].Groups[5].Value) -le 34) { throw 'Actual E close did not push the capsule clear of the door motion reservation.' }
    $reopenStart = [regex]::Matches($sandbox,'CONSTRUCTION_DOOR_REOPEN_START Revision=(\d+) Position=X=([\-\d.]+) Y=([\-\d.]+) Z=([\-\d.]+) VirtualDoorRay=1')
    $reopenEnd = [regex]::Matches($sandbox,'CONSTRUCTION_DOOR_REOPEN_END Revision=(\d+) Position=X=([\-\d.]+) Y=([\-\d.]+) Z=([\-\d.]+)')
    if ($reopenStart.Count -ne 1 -or $reopenEnd.Count -ne 1) { throw 'Missing unique clear-front reopen measurements.' }
    if ([uint64]$reopenStart[0].Groups[1].Value -ne 7 -or [uint64]$reopenEnd[0].Groups[1].Value -ne 8) { throw 'E reopen from the clear front position did not advance revision exactly once.' }
    if ([Math]::Abs((Number $reopenEnd[0].Groups[2].Value)-(Number $reopenStart[0].Groups[2].Value)) -gt 0.01 -or [Math]::Abs((Number $reopenEnd[0].Groups[3].Value)-(Number $reopenStart[0].Groups[3].Value)) -gt 0.01 -or [Math]::Abs((Number $reopenEnd[0].Groups[4].Value)-(Number $reopenStart[0].Groups[4].Value)) -gt 0.01) { throw 'E reopen from the clear front position moved the capsule.' }
    $trappedStart = [regex]::Matches($sandbox,'CONSTRUCTION_DOOR_TRAPPED_CLOSE_START Revision=(\d+) Position=X=([\-\d.]+) Y=([\-\d.]+) Z=([\-\d.]+) VirtualDoorRay=1 Blockers=2')
    $trappedEnd = [regex]::Matches($sandbox,'CONSTRUCTION_DOOR_TRAPPED_CLOSE_END Revision=(\d+) Position=X=([\-\d.]+) Y=([\-\d.]+) Z=([\-\d.]+) Preserved=1')
    if ($trappedStart.Count -ne 1 -or $trappedEnd.Count -ne 1) { throw 'Missing unique trapped close measurements.' }
    if ([uint64]$trappedStart[0].Groups[1].Value -ne 8 -or [uint64]$trappedEnd[0].Groups[1].Value -ne 8) { throw 'Blocked trapped close changed the prior current revision.' }
    if ([Math]::Abs((Number $trappedEnd[0].Groups[2].Value)-(Number $trappedStart[0].Groups[2].Value)) -gt 0.01 -or [Math]::Abs((Number $trappedEnd[0].Groups[3].Value)-(Number $trappedStart[0].Groups[3].Value)) -gt 0.01 -or [Math]::Abs((Number $trappedEnd[0].Groups[4].Value)-(Number $trappedStart[0].Groups[4].Value)) -gt 0.01) { throw 'Blocked trapped close changed the capsule location.' }
    $walk = [regex]::Matches($sandbox,'CONSTRUCTION_DOOR_WALK Start=X=([-\d.]+) Y=([-\d.]+) Z=([-\d.]+) End=X=([-\d.]+) Y=([-\d.]+) Z=([-\d.]+) OpenTraversalPeakZ=([\d.]+) WorldSeconds=([\d.]+) Teleports=0 GroundedThroughout=1 CapsuleRadius=34 CapsuleHalfHeight=88')
    if ($walk.Count -ne 1) { throw 'Missing actual grounded capsule walking record.' }
    $w = $walk[0]
    if ((Number $w.Groups[2].Value) -ge 300 -or (Number $w.Groups[5].Value) -le 600 -or [Math]::Abs((Number $w.Groups[6].Value)-(Number $w.Groups[3].Value)) -ge 1 -or (Number $w.Groups[7].Value) -le ((Number $w.Groups[3].Value)+3)) {
        throw 'Walking positions or actual open traversal sill rise did not satisfy the corridor gate.'
    }
    $startIndex = $sandbox.IndexOf('Name=MEASURED_WALK_STARTED')
    $endIndex = $sandbox.IndexOf('Name=GROUNDED_SILL_TRAVERSAL')
    if ($endIndex -le $startIndex -or $sandbox.Substring($startIndex,$endIndex-$startIndex) -match 'CONSTRUCTION_DOOR_INPUT_SETUP') { throw 'A deterministic setup occurred inside measured walking.' }
    $results.steps = @($observed | ForEach-Object { [ordered]@{ name=$_.Groups[1].Value; doors=[int]$_.Groups[2].Value; members=[int]$_.Groups[3].Value; revision=[uint64]$_.Groups[4].Value; baseEntries=[uint64]$_.Groups[5].Value; worldSeconds=(Number $_.Groups[6].Value) } })
    $results.walk = [ordered]@{ startY=(Number $w.Groups[2].Value); endY=(Number $w.Groups[5].Value); startZ=(Number $w.Groups[3].Value); endZ=(Number $w.Groups[6].Value); openTraversalPeakZ=(Number $w.Groups[7].Value); worldSeconds=(Number $w.Groups[8].Value); teleports=0; groundedThroughout=$true; capsuleRadius=34; capsuleHalfHeight=88 }
    $results.closingPush = [ordered]@{ startRevision=[uint64]$pushStart[0].Groups[1].Value; endRevision=[uint64]$pushEnd[0].Groups[1].Value; startY=(Number $pushStart[0].Groups[3].Value); endY=(Number $pushEnd[0].Groups[3].Value); horizontalDelta=(Number $pushEnd[0].Groups[5].Value); capsuleOutsideMotion=$true }
    $results.trappedClose = [ordered]@{ revision=[uint64]$trappedEnd[0].Groups[1].Value; positionPreserved=$true; denied=$true }
    $results.status = 'Passed'
    Write-Output "PASS: door controls, close push/rejection, exactly-once E routing and grounded sill traversal. Evidence: $EvidenceRoot"
} catch {
    $results.status = 'Failed'
    $results.error = $_.Exception.Message
    throw
} finally {
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
        if (-not $process.WaitForExit(5000)) { $results.cleanupIncomplete = $true }
    }
    $results.cleanupComplete = -not $results.Contains('cleanupIncomplete')
    if (-not $results.cleanupComplete) {
        $results.status = 'Failed'
        $results.error = 'An owned scenario process did not exit within the bounded cleanup wait.'
    }
    $results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'results.json') -Encoding UTF8
    if (-not $results.cleanupComplete) { throw 'An owned scenario process did not exit within the bounded cleanup wait.' }
}
