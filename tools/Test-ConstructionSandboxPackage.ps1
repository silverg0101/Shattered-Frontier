[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackageRoot,
    [ValidateRange(45,180)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$ownedRoot = Join-Path $repoRoot 'Artifacts\ConstructionSandbox\Packages'
function Assert-LocalPath([string]$Path) {
    if ($Path -notmatch '^[A-Za-z]:[\\/]' -or $Path -match '[\r\n"%&|<>^!`]' -or $Path -match '(^|[\\/])\.\.([\\/]|$)') { throw 'Expected a safe absolute local path.' }
    $full = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $cursor = $full
    while ($cursor) {
        if ((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Reparse-point path refused: $cursor" }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
    return $full
}
$packageRootFull = Assert-LocalPath $PackageRoot
if (-not $packageRootFull.StartsWith($ownedRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'PackageRoot must be a child of the owned Packages directory.' }
$manifestPath = Join-Path $packageRootFull 'package.json'
$package = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($package.status -ne 'PackageBuiltAcceptancePending' -or -not $package.originalInputsUnchanged -or $package.targetType -ne 'Game') { throw 'Package manifest has no accepted Game build provenance.' }
$archiveRoot = Join-Path $packageRootFull 'Archive'
$game = Assert-LocalPath $package.gameExecutable
if (-not $game.StartsWith($archiveRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or $game -notmatch '[\\/]Binaries[\\/]Win64[\\/]ShatteredFrontier\.exe$') { throw 'Manifest must select the direct archived Game executable.' }
$archivePaths = [Collections.Generic.List[string]]::new()
$pending = [Collections.Generic.Stack[string]]::new()
$pending.Push($archiveRoot)
while ($pending.Count) {
    $directory = Assert-LocalPath $pending.Pop()
    foreach ($item in Get-ChildItem -LiteralPath $directory -Force) {
        $null = Assert-LocalPath $item.FullName
        if ($item.PSIsContainer) { $pending.Push($item.FullName) }
        else { $archivePaths.Add($item.FullName.Substring($archiveRoot.Length + 1).Replace('\', '/')) }
    }
}
if (-not $archivePaths.Count -or @(Compare-Object @($archivePaths) @($package.archiveFiles.path)).Count) { throw 'Archive file set differs from the package manifest.' }
foreach ($entry in $package.archiveFiles) {
    $path = Assert-LocalPath (Join-Path $archiveRoot $entry.path)
    if (-not $path.StartsWith($archiveRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.sha256 -or
        (Get-Item -LiteralPath $path).Length -ne $entry.bytes) { throw "Archive provenance mismatch: $($entry.path)" }
}
$relativeGame = $game.Substring($archiveRoot.Length + 1).Replace('\', '/')
if ($relativeGame -notin @($package.archiveFiles.path)) { throw 'Direct Game executable is absent from the hashed archive manifest.' }
$runRoot = Join-Path $packageRootFull ('acceptance-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
if (Test-Path -LiteralPath $runRoot) { throw 'Acceptance run already exists.' }
$null = New-Item -ItemType Directory -Path $runRoot
$ownedProcesses = [Collections.Generic.List[Diagnostics.Process]]::new()
$results = [ordered]@{
    status = 'Running'; packageManifest = $manifestPath; packageManifestSha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    sourceRevision = $package.sourceRevision; executable = $game; runs = @()
    scope = 'Packaged Game finite structural/line/door input and E1 flag isolation; NullRHI; no rendered/manual/save-restart/multiplayer acceptance.'
}
function Invoke-Scenario([string]$Name, [string]$Map, [string]$Flag, [string]$Success, [string]$StepPrefix, [int]$StepCount) {
    $userDir = Assert-LocalPath (Join-Path $runRoot ($Name + '-user'))
    $null = New-Item -ItemType Directory -Path $userDir
    $log = Join-Path $runRoot ($Name + '.log')
    # No .uproject/editor argument. The first sandbox run intentionally has no map override.
    $arguments = @()
    if ($Map) { $arguments += $Map }
    $arguments += @('-NullRHI', '-unattended', '-nosound', '-nop4', ('-UserDir="' + $userDir + '/"'), ('-abslog="' + $log + '"'), '-ExecCmds="t.MaxFPS 60"')
    $arguments += $Flag
    if ($Name -eq 'normal-e1') { $arguments += '-testexit=E1_WORLD_READY' }
    # Unreal FParse expects quotes immediately after '=' for path-valued switches.
    $quoted = $arguments
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $game -ArgumentList $quoted -WorkingDirectory ([IO.Path]::GetDirectoryName($game)) -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $runRoot ($Name + '.stdout.txt')) -RedirectStandardError (Join-Path $runRoot ($Name + '.stderr.txt'))
    $null = $process.Handle  # PS 5.1 keeps ExitCode only if the handle is opened before exit
    $ownedProcesses.Add($process)
    $run = [ordered]@{ name = $Name; pid = $process.Id; arguments = $quoted; userDir = $userDir; log = $log; exitCode = $null; steps = 0 }
    $results.runs += $run
    while (-not $process.WaitForExit(500)) {
        if ($watch.Elapsed.TotalSeconds -gt $TimeoutSeconds) { throw "$Name exceeded $TimeoutSeconds seconds (owned PID $($process.Id))." }
    }
    $process.WaitForExit()
    $run.exitCode = $process.ExitCode
    $run.elapsedSeconds = $watch.Elapsed.TotalSeconds
    if ($process.ExitCode -ne 0) { throw "$Name failed with exit code $($process.ExitCode)." }
    $content = Get-Content -LiteralPath $log -Raw
    if ($content -match 'CONSTRUCTION_\w*FAILED|Fatal error:|Assertion failed:|Ensure condition failed:') { throw "$Name recorded a runtime failure." }
    if ([regex]::Matches($content, $Success).Count -ne 1) { throw "$Name has no unique expected semantic success marker." }
    if (-not (Test-Path -LiteralPath (Join-Path $userDir 'Saved') -PathType Container)) { throw "$Name did not create Saved under the explicit isolated UserDir." }
    if ($StepPrefix) {
        $steps = [regex]::Matches($content, [regex]::Escape($StepPrefix) + ' Name=(\w+)')
        if ($steps.Count -ne $StepCount -or @($steps | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique).Count -ne $StepCount) { throw "$Name did not report $StepCount unique semantic steps." }
        $run.steps = $steps.Count
    }
    if ($Name -eq 'normal-e1' -and $content -match 'CONSTRUCTION_(INPUT_|LINE_INPUT_|DOOR_INPUT_|TOOLS_READY|PILOT_READY|RESTART_)') { throw 'Normal E1 admitted a construction fixture or sandbox mode.' }
    $run.logSha256 = (Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash.ToLowerInvariant()
    $run.passed = $true
    Write-Output "PASS: packaged $Name ($($run.steps) steps)."
}
try {
    Invoke-Scenario 'default-sandbox-structural' '' '-ConstructionInputScenario' 'CONSTRUCTION_INPUT_SUCCESS Steps=16 Members=0 WorldGraceSeconds=[\d.]+ AutomatedEngineInput=1 PhysicalKeyboard=0 Rendered=0' 'CONSTRUCTION_INPUT_STEP' 16
    Invoke-Scenario 'sandbox-line' '/Game/Frontier/Maps/Greenlands_ConstructionSandbox' '-ConstructionLineInputScenario' 'CONSTRUCTION_LINE_INPUT_SUCCESS Steps=21 Members=5 Revision=5 AutomatedInput=1 SimpleCollision=1' 'CONSTRUCTION_LINE_INPUT_STEP' 21
    Invoke-Scenario 'sandbox-door' '/Game/Frontier/Maps/Greenlands_ConstructionSandbox' '-ConstructionDoorInputScenario' 'CONSTRUCTION_DOOR_INPUT_SUCCESS Steps=19 Doors=1 BaseEntryDelta=1 AutomatedInput=1 ActualCharacterWalking=1 Rendered=0' 'CONSTRUCTION_DOOR_INPUT_STEP' 19
    Invoke-Scenario 'normal-e1' '/Game/Frontier/Maps/Greenlands_E1' '-ConstructionInputScenario' 'E1_WORLD_READY[^\r\n]*NetMode=0' '' 0
    $results.status = 'Passed'
} catch {
    $results.status = 'Failed'
    $results.error = $_.Exception.Message
    throw
} finally {
    $complete = $true
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
        if (-not $process.WaitForExit(5000)) { $complete = $false }
    }
    $results.cleanupComplete = $complete
    if (-not $complete) { $results.status = 'Failed'; $results.error = 'Owned process cleanup incomplete.' }
    [IO.File]::WriteAllText((Join-Path $runRoot 'results.json'), ($results | ConvertTo-Json -Depth 10), [Text.UTF8Encoding]::new($false))
    Write-Output "Packaged evidence: $runRoot"
    if (-not $complete) { throw $results.error }
}
