[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$RunRoot,
    [switch]$Preflight
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$packageRoot = Join-Path $repoRoot 'Artifacts\ConstructionSandbox\Packages'

function Get-SafePath([string]$Path) {
    if ($Path -notmatch '^[A-Za-z]:[\\/]' -or $Path -match '[\r\n"%&|<>^!`]' -or $Path -match '(^|[\\/])\.\.([\\/]|$)') {
        throw "Expected an absolute local path without traversal or shell metacharacters: $Path"
    }
    $full = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $cursor = $full
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            if ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Reparse-point path refused: $cursor"
            }
        }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
    return $full
}
function Get-Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Write-Json([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
}
function Assert-CleanInputs {
    $changes = @(& git -C $repoRoot status --porcelain --untracked-files=no -- @inputSelectors)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect tracked project inputs.' }
    if ($changes.Count) { throw 'Tracked GameClient packaging inputs are dirty; preserve/commit them before packaging.' }
}
function Set-IniSection([string]$Path, [string]$Section, [string[]]$Keys, [string[]]$Values) {
    $lines = [Collections.Generic.List[string]]::new()
    $inside = $false
    $found = $false
    foreach ($line in [IO.File]::ReadAllLines($Path)) {
        if ($line -match '^\s*\[(.+)\]\s*$') {
            $inside = $Matches[1] -eq $Section
            if ($inside) {
                if ($found) { throw "Duplicate copied INI section: $Section" }
                $found = $true
                $lines.Add($line)
                foreach ($value in $Values) { $lines.Add($value) }
                continue
            }
        }
        if ($inside) {
            $skip = $false
            foreach ($key in $Keys) {
                if ($line -match ('^\s*[+!.-]?' + [regex]::Escape($key) + '\s*=')) { $skip = $true }
            }
            if ($skip) { continue }
        }
        $lines.Add($line)
    }
    if (-not $found) { throw "Required copied INI section missing: $Section" }
    [IO.File]::WriteAllLines($Path, $lines, [Text.UTF8Encoding]::new($false))
}
function Get-OwnedFiles([string]$Root) {
    # Inspect each directory before descending, including hidden files; never follow links.
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($Root)
    while ($pending.Count) {
        $directory = Get-SafePath $pending.Pop()
        foreach ($item in Get-ChildItem -LiteralPath $directory -Force) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Generated link refused: $($item.FullName)" }
            if ($item.PSIsContainer) { $pending.Push($item.FullName) } else { $item }
        }
    }
}

$engineRootFull = Get-SafePath $EngineRoot
if (-not $RunRoot) { $RunRoot = Join-Path $packageRoot ('win64-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')) }
$runRootFull = Get-SafePath $RunRoot
$packageRoot = Get-SafePath $packageRoot
if (-not $runRootFull.StartsWith($packageRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'RunRoot must be a new child of Artifacts/ConstructionSandbox/Packages.'
}
if (Test-Path -LiteralPath $runRootFull) { throw 'RunRoot already exists; refusing reuse.' }
$uat = Join-Path $engineRootFull 'Engine\Build\BatchFiles\RunUAT.bat'
$versionFile = Join-Path $engineRootFull 'Engine\Build\Build.version'
$installedMarker = Join-Path $engineRootFull 'Engine\Build\InstalledBuild.txt'
foreach ($required in @($uat, $versionFile, $installedMarker, (Join-Path $engineRootFull 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing installed-engine prerequisite: $required" }
}
$version = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8 -or $version.PatchVersion -ne 3) {
    throw 'This reviewed route requires installed Unreal Engine 5.8.3.'
}
$inputSelectors = @('GameClient/ShatteredFrontier.uproject', 'GameClient/Source', 'GameClient/Config', 'GameClient/Content', 'GameClient/Plugins')
Assert-CleanInputs
$revision = & git -C $repoRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve source revision.' }
$tracked = @(& git -C $repoRoot -c core.quotepath=false ls-files -- @inputSelectors)
if ($LASTEXITCODE -ne 0 -or -not $tracked.Count) { throw 'Cannot enumerate committed packaging inputs.' }
$inputs = @(
    foreach ($relative in $tracked) {
        if ($relative -notmatch '^GameClient/(ShatteredFrontier\.uproject|Source/.+|Config/.+|Content/.+|Plugins/.+)$' -or $relative -match '[\r\n"%&|<>^!`]') {
            throw "Unsupported tracked input name: $relative"
        }
        $source = Get-SafePath (Join-Path $repoRoot $relative)
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing tracked input: $relative" }
        [ordered]@{ path = $relative; sha256 = Get-Hash $source; bytes = (Get-Item -LiteralPath $source).Length }
    }
)
foreach ($requiredRelative in @('GameClient/ShatteredFrontier.uproject', 'GameClient/Source/ShatteredFrontier.Target.cs', 'GameClient/Config/DefaultEngine.ini', 'GameClient/Config/DefaultGame.ini', 'GameClient/Content/Frontier/Maps/Greenlands_ConstructionSandbox.umap', 'GameClient/Content/Frontier/Maps/Greenlands_E1.umap')) {
    if ($requiredRelative -notin $inputs.path) { throw "Required committed input missing: $requiredRelative" }
}
$projectCopy = Join-Path $runRootFull 'Project\GameClient\ShatteredFrontier.uproject'
$stage = Join-Path $runRootFull 'Stage'
$archive = Join-Path $runRootFull 'Archive'
$maps = @('/Game/Frontier/Maps/Greenlands_ConstructionSandbox', '/Game/Frontier/Maps/Greenlands_E1')
$uatArgs = @('BuildCookRun', "-project=$projectCopy", '-target=ShatteredFrontier', '-platform=Win64', '-clientconfig=Development',
    '-noP4', '-unattended', '-utf8output', '-build', '-cook', '-stage', '-pak', '-archive',
    ('-MapsToCook=' + ($maps -join '+')), "-stagingdirectory=$stage", "-archivedirectory=$archive")
$record = [ordered]@{
    status = 'PreflightPassed'; sourceRevision = "$revision"; runRoot = $runRootFull
    engine = [ordered]@{ root = $engineRootFull; version = $version; versionSha256 = Get-Hash $versionFile; installedMarkerSha256 = Get-Hash $installedMarker; uatSha256 = Get-Hash $uat }
    inputs = $inputs; maps = $maps; target = 'ShatteredFrontier'; targetType = 'Game'; configuration = 'Development'
    executable = $uat; arguments = $uatArgs
    configurationTransform = 'Copied project only: sandbox GameDefaultMap/ServerDefaultMap; replace MapsToCook with sandbox and E1 negative-test map.'
    scope = 'Isolated standalone Development packaging; no deployment, gameplay acceptance or multiplayer acceptance.'
}
if ($Preflight) {
    Write-Output "PREFLIGHT PASS: $($inputs.Count) clean tracked inputs; UE 5.8.3; Game/Win64/Development; destination $runRootFull"
    return
}

$null = New-Item -ItemType Directory -Path $runRootFull
$summary = Join-Path $runRootFull 'package.json'
try {
    $record.status = 'Copying'
    Write-Json $summary $record
    foreach ($inputFile in $inputs) {
        $destination = Join-Path (Join-Path $runRootFull 'Project') $inputFile.path
        $null = New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($destination)) -Force
        [IO.File]::Copy((Join-Path $repoRoot $inputFile.path), $destination, $false)
        if ((Get-Hash $destination) -ne $inputFile.sha256) { throw "Input changed while copying: $($inputFile.path)" }
    }
    Assert-CleanInputs
    $copiedConfig = Join-Path $runRootFull 'Project\GameClient\Config'
    Set-IniSection (Join-Path $copiedConfig 'DefaultEngine.ini') '/Script/EngineSettings.GameMapsSettings' @('GameDefaultMap', 'ServerDefaultMap') @("GameDefaultMap=$($maps[0])", "ServerDefaultMap=$($maps[0])")
    Set-IniSection (Join-Path $copiedConfig 'DefaultGame.ini') '/Script/UnrealEd.ProjectPackagingSettings' @('MapsToCook') @('!MapsToCook=ClearArray', ('+MapsToCook=(FilePath="' + $maps[0] + '")'), ('+MapsToCook=(FilePath="' + $maps[1] + '")'))
    $record.derivedConfig = @('DefaultEngine.ini', 'DefaultGame.ini' | ForEach-Object { [ordered]@{ path = $_; sha256 = Get-Hash (Join-Path $copiedConfig $_) } })
    $record.status = 'Building'
    Write-Json $summary $record
    $log = Join-Path $runRootFull 'UAT.log'
    Write-Output "Building isolated sandbox; UAT log: $log"
    & $uat @uatArgs *> $log
    $record.uatExitCode = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed (exit $LASTEXITCODE); retain $log" }
    $receipt = Join-Path $runRootFull 'Project\GameClient\Binaries\Win64\ShatteredFrontier.target'
    $receiptData = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
    if ($receiptData.TargetName -ne 'ShatteredFrontier' -or $receiptData.TargetType -ne 'Game' -or $receiptData.Platform -ne 'Win64' -or $receiptData.Configuration -ne 'Development') {
        throw 'Actual target receipt does not match Game/Win64/Development.'
    }
    $cookedFiles = @(Get-OwnedFiles (Join-Path $runRootFull 'Project\GameClient\Saved\Cooked'))
    foreach ($map in $maps) {
        $suffix = $map.Replace('/Game/', '/Content/') + '.umap'
        if (-not @($cookedFiles | Where-Object { $_.FullName.Replace('\', '/').EndsWith($suffix, [StringComparison]::OrdinalIgnoreCase) }).Count) {
            throw "Cooked map missing: $map"
        }
    }
    $archiveFiles = @(Get-OwnedFiles $archive)
    $gameExecutables = @($archiveFiles | Where-Object { $_.FullName -match '[\\/]Binaries[\\/]Win64[\\/]ShatteredFrontier\.exe$' })
    if ($gameExecutables.Count -ne 1) { throw 'Expected exactly one direct archived Game executable.' }
    $record.gameExecutable = $gameExecutables[0].FullName
    $record.receipt = [ordered]@{ path = $receipt; sha256 = Get-Hash $receipt; data = $receiptData }
    $stageFiles = @(Get-OwnedFiles $stage)
    $stagingManifests = @($stageFiles | Where-Object { $_.Name -match '^Manifest_(UFS|NonUFS)Files' })
    if (-not @($stagingManifests | Where-Object { $_.Name -like 'Manifest_UFSFiles*' }).Count -or
        -not @($stagingManifests | Where-Object { $_.Name -like 'Manifest_NonUFSFiles*' }).Count) {
        throw 'UFS/NonUFS staging manifests missing; cannot verify staged closure.'
    }
    $record.stageFiles = @($stageFiles | Sort-Object FullName | ForEach-Object {
        [ordered]@{ path = $_.FullName.Substring($stage.Length + 1).Replace('\', '/'); bytes = $_.Length; sha256 = Get-Hash $_.FullName }
    })
    $record.archiveFiles = @($archiveFiles | Sort-Object FullName | ForEach-Object {
        [ordered]@{ path = $_.FullName.Substring($archive.Length + 1).Replace('\', '/'); bytes = $_.Length; sha256 = Get-Hash $_.FullName }
    })
    $record.status = 'PackageBuiltAcceptancePending'
    Write-Output "PACKAGE BUILT; gameplay acceptance pending. Direct executable: $($record.gameExecutable)"
} catch {
    $record.status = 'Failed'
    $record.error = $_.Exception.Message
    throw
} finally {
    $unchanged = $true
    foreach ($inputFile in $inputs) {
        $original = Join-Path $repoRoot $inputFile.path
        if (-not (Test-Path -LiteralPath $original -PathType Leaf) -or (Get-Hash $original) -ne $inputFile.sha256) { $unchanged = $false }
    }
    $record.originalInputsUnchanged = $unchanged
    if (-not $unchanged) { $record.status = 'Failed'; $record.error = 'Original project inputs changed during packaging.' }
    Write-Json $summary $record
    if (-not $unchanged) { throw $record.error }
}
