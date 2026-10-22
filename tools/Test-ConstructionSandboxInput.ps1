[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$EvidenceRoot,
    [ValidateRange(30,90)][int]$TimeoutSeconds = 60,
    [switch]$SkipNormalModeCheck
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$ownedRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'Artifacts\ConstructionSandbox'))
if (-not $EvidenceRoot) { $EvidenceRoot = Join-Path $ownedRoot ('input-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')) }
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
$results = [ordered]@{ status = 'Running'; engine = $editor; project = $project; runs = @(); scope = 'Automated engine input; NullRHI; standalone development; no physical keyboard/rendered/packaged/network acceptance' }
function Invoke-FiniteGame([string]$Name, [string]$Map, [string[]]$Extra) {
    $log = Join-Path $EvidenceRoot ($Name + '.log')
    $arguments = @(('"' + $project + '"'), $Map, '-game', '-SFDevUnlimitedStock', '-NullRHI', '-unattended', '-nosound', '-nop4',
        '-ConstructionInputScenario', '-ExecCmds="t.MaxFPS 60"', ('-abslog="' + $log + '"')) + $Extra
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $EvidenceRoot ($Name + '.stdout.txt')) -RedirectStandardError (Join-Path $EvidenceRoot ($Name + '.stderr.txt'))
    $null = $process.Handle  # PS 5.1 keeps ExitCode only if the handle is opened before exit
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
    if ($text -match 'CONSTRUCTION_INPUT_FAILED|Fatal error:|Assertion failed:') { throw "$Name recorded a scenario or engine failure." }
    return $text
}
try {
    $sourceFiles = @(
        'GameClient\Source\ShatteredFrontier\Private\Tests\SFConstructionInputScenario.cpp',
        'GameClient\Source\ShatteredFrontier\Private\Construction\SFConstructionSandboxController.cpp',
        'GameClient\Source\ShatteredFrontier\Private\Construction\SFConstructionSandboxState.cpp',
        'GameClient\Binaries\Win64\UnrealEditor-ShatteredFrontier.dll'
    )
    $results.provenance = @($sourceFiles | ForEach-Object {
        $path = Join-Path $repoRoot $_
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required source/build not found: $path" }
        [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    })
    if (-not $SkipNormalModeCheck) {
        $normal = Invoke-FiniteGame 'normal-mode-negative' '/Game/Frontier/Maps/Greenlands_E1' @('-testexit=E1_WORLD_READY')
        if ($normal -notmatch 'E1_WORLD_READY.*NetMode=0' -or $normal -match 'CONSTRUCTION_INPUT_|CONSTRUCTION_TOOLS_READY|CONSTRUCTION_PILOT_READY') {
            throw 'Normal E1 flag isolation failed or normal mode did not become ready.'
        }
        $results.normalModeFlagIgnored = $true
    } else { $results.normalModeFlagIgnored = $null }
    $sandbox = Invoke-FiniteGame 'sandbox-input' '/Game/Frontier/Maps/Greenlands_ConstructionSandbox' @()
    $expected = [ordered]@{
        OWNING_PLAYER_READY = 0; DISABLED_CLICK_DENIED = 0; SHAPE_STALE_GHOST_DENIED = 0; ROTATION_STALE_GHOST_DENIED = 0
        SLOPE_ROTATION_PLACED = 1; SAFE_REMOVAL_APPLIED = 0; SLAB_STALE_GHOST_DENIED = 0; HALF_SLAB_PLACED = 1; HALF_SLAB_REMOVED = 0
        CUBE_PLACED = 1; SUPPORTED_STACK_PLACED = 2
        REMOVAL_REQUIRES_CONFIRMATION = 2; CANCELLED_CONFIRMATION_DENIED = 2; SUPPORT_LOSS_GRACE_STARTED = 1
        WORLD_TIME_GRACE_PRESERVED = 1; WORLD_TIME_COLLAPSE_APPLIED = 0
    }
    $observed = @([regex]::Matches($sandbox, 'CONSTRUCTION_INPUT_STEP Name=(\w+) Count=(\d+) Revision=(\d+) WorldSeconds=([\d.]+)'))
    if ($observed.Count -ne $expected.Count) { throw "Expected $($expected.Count) semantic steps, observed $($observed.Count)." }
    foreach ($entry in $expected.GetEnumerator()) {
        $matchesForStep = @($observed | Where-Object { $_.Groups[1].Value -eq $entry.Key -and [int]$_.Groups[2].Value -eq $entry.Value })
        if ($matchesForStep.Count -ne 1) { throw "Missing/duplicate or incorrect authoritative state for $($entry.Key)." }
    }
    $success = [regex]::Matches($sandbox, 'CONSTRUCTION_INPUT_SUCCESS Steps=16 Members=0 WorldGraceSeconds=([\d.]+) AutomatedEngineInput=1 PhysicalKeyboard=0 Rendered=0')
    if ($success.Count -ne 1 -or [double]::Parse($success[0].Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture) -lt 10.0) {
        throw 'Missing unique semantic success or ten seconds of actual world grace.'
    }
    $results.steps = @($observed | ForEach-Object { [ordered]@{ name = $_.Groups[1].Value; members = [int]$_.Groups[2].Value; revision = [uint64]$_.Groups[3].Value; worldSeconds = [double]::Parse($_.Groups[4].Value, [Globalization.CultureInfo]::InvariantCulture) } })
    $results.status = 'Passed'
    Write-Output "PASS: owning-player engine input, semantic geometry/state assertions and world-time collapse. Evidence: $EvidenceRoot"
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
