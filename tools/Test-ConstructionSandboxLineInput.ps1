[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$EvidenceRoot,
    [ValidateRange(40,90)][int]$TimeoutSeconds = 70
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$ownedRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'Artifacts\ConstructionSandbox'))
if (-not $EvidenceRoot) { $EvidenceRoot = Join-Path $ownedRoot ('line-input-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')) }
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($ownedRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'EvidenceRoot must be a new child under Artifacts/ConstructionSandbox.'
}
if (Test-Path -LiteralPath $EvidenceRoot) { throw 'EvidenceRoot already exists; refusing overwrite.' }
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Editor not found: $editor" }
if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { throw "Project not found: $project" }
function Assert-NoReparsePath([string]$Path) {
    $current = [IO.Path]::GetFullPath($Path)
    while ($current) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse path rejected: $current" }
        }
        $parent = [IO.Path]::GetDirectoryName($current)
        if (-not $parent -or $parent -eq $current) { break }
        $current = $parent
    }
}
Assert-NoReparsePath $repoRoot
Assert-NoReparsePath $EvidenceRoot
$null = New-Item -ItemType Directory -Path $EvidenceRoot
$ownedProcesses = [Collections.Generic.List[Diagnostics.Process]]::new()
$results = [ordered]@{ status = 'Running'; engine = $editor; project = $project; runs = @(); scope = 'Automated line input and native simple collision; NullRHI; standalone development; no human/rendered/packaged/network acceptance' }
function Invoke-FiniteGame([string]$Name, [string]$Map, [string[]]$Extra) {
    $log = Join-Path $EvidenceRoot ($Name + '.log')
    $arguments = @(('"' + $project + '"'), $Map, '-game', '-SFDevUnlimitedStock', '-NullRHI', '-unattended', '-nosound', '-nop4',
        '-ConstructionLineInputScenario', '-ExecCmds="t.MaxFPS 60"', ('-abslog="' + $log + '"')) + $Extra
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
    if (-not (Test-Path -LiteralPath $log -PathType Leaf) -or (Get-Item -LiteralPath $log).Length -gt 16777216) { throw "$Name log missing or exceeds bounded read limit." }
    $text = [IO.File]::ReadAllText($log)
    if ($text -match 'CONSTRUCTION_LINE_INPUT_FAILED|Fatal error:|Assertion failed:') { throw "$Name recorded a scenario or engine failure." }
    return $text
}
try {
    $sourceFiles = @(
        'GameClient\Source\ShatteredFrontier\Private\Tests\SFConstructionLineInputScenario.cpp',
        'GameClient\Source\ShatteredFrontier\Private\Construction\SFConstructionSandboxController.cpp',
        'GameClient\Source\ShatteredFrontier\Private\Construction\SFConstructionSandboxState.cpp',
        'GameClient\Source\ShatteredFrontier\Private\Construction\SFConstructionLine.cpp',
        'GameClient\Binaries\Win64\UnrealEditor-ShatteredFrontier.dll',
        'tools\Test-ConstructionSandboxLineInput.ps1'
    )
    $results.provenance = @($sourceFiles | ForEach-Object {
        $path = Join-Path $repoRoot $_
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required source/build not found: $path" }
        [ordered]@{ path=$_; sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    })
    $normal = Invoke-FiniteGame 'normal-mode-negative' '/Game/Frontier/Maps/Greenlands_E1' @('-testexit=E1_WORLD_READY')
    if ($normal -notmatch 'E1_WORLD_READY.*NetMode=0' -or $normal -match 'CONSTRUCTION_LINE_INPUT_|CONSTRUCTION_TOOLS_READY|CONSTRUCTION_PILOT_READY') { throw 'Normal E1 flag isolation failed.' }
    $results.normalModeFlagIgnored = $true
    $sandbox = Invoke-FiniteGame 'sandbox-input' '/Game/Frontier/Maps/Greenlands_ConstructionSandbox' @()
    # Name, members, revision, line mode, seed, exact ghost count (-1 means bounded).
    $expected = @(
        @('LINE_MODE_ENTERED',0,0,1,0,-1), @('SEED_READ_ONLY',0,0,1,1,-1), @('FULL_GHOST_THREE',0,0,1,1,3),
        @('GROUND_LINE_ATOMIC',3,1,1,0,-1), @('REVERSE_LINE_ATOMIC',6,2,1,0,-1),
        @('OCCUPIED_LINE_DENIED',6,2,1,1,5), @('DIAGONAL_DENIED',6,2,1,1,0), @('CAP_DENIED',6,2,1,1,0),
        @('SELECTION_CLEARS_SEED',6,2,1,0,-1), @('ROTATION_CLEARS_SEED',6,2,1,0,-1),
        @('MODE_CLEARS_SEED',6,2,0,0,0), @('PAUSE_CLEARS_SEED',6,2,1,0,-1), @('CANCEL_CLEARS_SEED',6,2,1,0,-1),
        @('DOOR_RETURNS_SINGLE',6,2,0,0,0), @('SINGLE_PLACEMENT_RETAINED',7,3,0,0,0),
        @('SINGLE_REMOVAL_RETAINED',6,4,0,0,0), @('DEMOLITION_CANCELLED_SEED',5,5,1,0,-1),
        @('INHERITED_MOVEMENT_RETAINED',5,5,1,0,-1),
        @('RMB_LOOK_NO_DEMOLITION',5,5,1,0,0),
        @('FREE_CURSOR_RESTORED',5,5,1,0,-1),
        @('DEDICATED_DEMOLITION_MODE',5,5,1,0,0)
    )
    $observed = [regex]::Matches($sandbox,'CONSTRUCTION_LINE_INPUT_STEP Name=(\w+) Members=(\d+) Doors=(\d+) Revision=(\d+) LineMode=(0|1) Seed=(0|1) GhostCells=(\d+) WorldSeconds=([\d.]+)')
    if ($observed.Count -ne $expected.Count) { throw "Expected21 named steps, observed $($observed.Count)." }
    for ($index=0; $index -lt $expected.Count; ++$index) {
        $e=$expected[$index]; $o=$observed[$index]
        if ($o.Groups[1].Value -ne $e[0] -or [int]$o.Groups[2].Value -ne $e[1] -or [int]$o.Groups[3].Value -ne 0 -or [uint64]$o.Groups[4].Value -ne $e[2] -or [int]$o.Groups[5].Value -ne $e[3] -or [int]$o.Groups[6].Value -ne $e[4]) { throw "Semantic state/order mismatch at $($e[0])." }
        $ghosts=[int]$o.Groups[7].Value
        if ($ghosts -gt 8 -or ($e[5] -ge 0 -and $ghosts -ne $e[5])) { throw "Ghost geometry/count mismatch at $($e[0])." }
    }
    if ([regex]::Matches($sandbox,'CONSTRUCTION_LINE_INPUT_STARTED').Count -ne 1 -or [regex]::Matches($sandbox,'CONSTRUCTION_LINE_INPUT_SUCCESS Steps=21 Members=5 Revision=5 AutomatedInput=1 SimpleCollision=1').Count -ne 1) { throw 'Missing unique finite input/physics success.' }
    if ($sandbox -match 'CONSTRUCTION_SAVE_RESULT|CONSTRUCTION_LOAD_RESULT') { throw 'Original-project input fixture must not dispatch player disk save/load.' }
    $results.steps=@($observed | ForEach-Object { [ordered]@{ name=$_.Groups[1].Value; members=[int]$_.Groups[2].Value; doors=[int]$_.Groups[3].Value; revision=[uint64]$_.Groups[4].Value; lineMode=[int]$_.Groups[5].Value; seed=[int]$_.Groups[6].Value; ghostCells=[int]$_.Groups[7].Value; worldSeconds=[double]::Parse($_.Groups[8].Value,[Globalization.CultureInfo]::InvariantCulture) } })
    $results.noPlayerSaveLoadDispatch=$true
    $results.status='Passed'
    Write-Output "PASS:21 semantic line/camera/demolition input/preview/physics checks and normal-mode isolation. Evidence: $EvidenceRoot"

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
