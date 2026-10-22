[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$EvidenceRoot,
    [ValidateRange(40,120)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$ownedRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'Artifacts\ConstructionSandbox'))
if (-not $EvidenceRoot) { $EvidenceRoot = Join-Path $ownedRoot ('save-restart-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')) }
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($ownedRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'EvidenceRoot must be a new child below Artifacts/ConstructionSandbox.' }
if (Test-Path -LiteralPath $EvidenceRoot) { throw 'EvidenceRoot exists; refusing overwrite.' }
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
$executable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw 'Installed commandlet executable is missing.' }
$ordinarySaveRoot = Join-Path $repoRoot 'GameClient\Saved\ConstructionSandbox'
function Get-OrdinarySaveSnapshot {
    Assert-NoReparsePath $ordinarySaveRoot
    if (-not (Test-Path -LiteralPath $ordinarySaveRoot -PathType Container)) { return [ordered]@{ exists=$false; entries=@() } }
    $entries = [Collections.Generic.List[object]]::new()
    $count = 0
    foreach ($path in [IO.Directory]::EnumerateFileSystemEntries($ordinarySaveRoot)) {
        if (++$count -gt 512) { throw 'Ordinary save directory exceeds bounded read-only inspection limit.' }
        Assert-NoReparsePath $path
        $item = Get-Item -LiteralPath $path -Force
        $entry = [ordered]@{ name=$item.Name; directory=[bool]$item.PSIsContainer; lastWriteUtc=$item.LastWriteTimeUtc.ToString('o') }
        if (-not $item.PSIsContainer) {
            $entry.length = $item.Length
            $entry.sha256 = if ($item.Length -le 65536) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash } else { $null }
        }
        $entries.Add($entry)
    }
    return [ordered]@{ exists=$true; lastWriteUtc=(Get-Item -LiteralPath $ordinarySaveRoot).LastWriteTimeUtc.ToString('o'); entries=@($entries | Sort-Object { $_.name }) }
}
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null
$fixtureId = [Guid]::NewGuid().ToString('N')
$copyRoot = Join-Path $EvidenceRoot ('Project-' + $fixtureId)
$copyProject = Join-Path $copyRoot 'ShatteredFrontier.uproject'
$copyModule = Join-Path $copyRoot 'Binaries\Win64\UnrealEditor-ShatteredFrontier.dll'
$results = [ordered]@{ status='Running'; fixtureId=$fixtureId; project=$copyProject; copiedFiles=@(); runs=[Collections.Generic.List[object]]::new(); ordinarySaveRoot=$ordinarySaveRoot }
$ownedProcesses = [Collections.Generic.List[Diagnostics.Process]]::new()
$ordinaryBefore = $null
function Invoke-FiniteRun([string]$Name,[string]$Map,[string[]]$Extra) {
    $log = Join-Path $EvidenceRoot ($Name + '.log')
    $arguments = @(('"' + $copyProject + '"'),$Map,'-game','-SFDevUnlimitedStock','-NullRHI','-unattended','-nosound','-nop4','-NoSplash','-ExecCmds="t.MaxFPS 60"',('-abslog="' + $log + '"')) + $Extra
    $run = [ordered]@{ name=$Name; arguments=$arguments; startedUtc=[DateTime]::UtcNow.ToString('o') }
    $results.runs.Add($run)
    $process = Start-Process -FilePath $executable -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $EvidenceRoot ($Name + '.stdout.txt')) -RedirectStandardError (Join-Path $EvidenceRoot ($Name + '.stderr.txt'))
    $null = $process.Handle  # PS 5.1 keeps ExitCode only if the handle is opened before exit
    $ownedProcesses.Add($process)
    $run.pid = $process.Id
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { throw "$Name exceeded its finite timeout." }
    $process.Refresh()
    $run.exitCode = $process.ExitCode
    $run.exitedUtc = [DateTime]::UtcNow.ToString('o')
    if ($process.ExitCode -ne 0) { throw "$Name exited $($process.ExitCode)." }
    if (-not (Test-Path -LiteralPath $log -PathType Leaf) -or (Get-Item -LiteralPath $log).Length -gt 16777216) { throw "$Name log missing or exceeds bounded read limit." }
    $content = Get-Content -LiteralPath $log -Raw
    if ($content -match 'CONSTRUCTION_RESTART_FAILED|Fatal error:|Assertion failed:') { throw "$Name recorded a driver or engine failure." }
    return $content
}
function Get-VerifiedRun([string]$Content,[string]$Role) {
    $contexts = [regex]::Matches($Content,'CONSTRUCTION_RESTART_CONTEXT Role=(Writer|Reader) Project="([^"]+)" Module="([^"]+)" Saved="([^"]+)"')
    if ($contexts.Count -ne 1 -or $contexts[0].Groups[1].Value -ne $Role) { throw "$Role requires exactly one admitted context." }
    $context = $contexts[0]
    $expected = @($copyProject,$copyModule,(Join-Path $copyRoot 'Saved'))
    for ($index = 0; $index -lt 3; ++$index) {
        $actual = [IO.Path]::GetFullPath($context.Groups[$index+2].Value).TrimEnd('\','/')
        if (-not $actual.Equals([IO.Path]::GetFullPath($expected[$index]).TrimEnd('\','/'),[StringComparison]::OrdinalIgnoreCase)) { throw "$Role loaded project/module/saved outside its exact copy." }
    }
    $successMarkers = [regex]::Matches($Content,'CONSTRUCTION_RESTART_SUCCESS Role=(Writer|Reader) Generation=(\d+) SourceRevision=(\d+) LiveRevision=(\d+) Cells=(\d+) Doors=(\d+) ContentDigest=([0-9a-fA-F]{64}) SimpleCollision=1')
    if ($successMarkers.Count -ne 1 -or $successMarkers[0].Groups[1].Value -ne $Role) { throw "$Role lacks exactly one semantic success marker." }
    $marker = $successMarkers[0]
    $liveRevision = if ($Role -eq 'Writer') { 5 } else { 1 }
    if ([uint64]$marker.Groups[2].Value -ne 1 -or [uint64]$marker.Groups[3].Value -ne 5 -or [uint64]$marker.Groups[4].Value -ne $liveRevision -or [int]$marker.Groups[5].Value -ne 3 -or [int]$marker.Groups[6].Value -ne 1) { throw "$Role semantic counts/revisions/generation differ from finite fixture." }
    return [ordered]@{ role=$Role; generation=1; sourceRevision=5; liveRevision=$liveRevision; cells=3; doors=1; contentDigest=$marker.Groups[7].Value.ToLowerInvariant(); simpleCollision=$true; context=[ordered]@{ project=$context.Groups[2].Value; module=$context.Groups[3].Value; saved=$context.Groups[4].Value } }
}
try {
    $ordinaryBefore = Get-OrdinarySaveSnapshot
    $results.ordinaryBefore = $ordinaryBefore
    $closure = @('ShatteredFrontier.uproject','Config\DefaultEngine.ini','Config\DefaultGame.ini','Config\DefaultInput.ini','Content\Frontier\Maps\Greenlands_E1.umap','Content\Frontier\Maps\Greenlands_ConstructionSandbox.umap','Binaries\Win64\UnrealEditor-ShatteredFrontier.dll','Binaries\Win64\UnrealEditor.modules')
    $copies = [Collections.Generic.List[object]]::new()
    foreach ($relative in $closure) {
        $source = Join-Path (Join-Path $repoRoot 'GameClient') $relative
        $target = [IO.Path]::GetFullPath((Join-Path $copyRoot $relative))
        if (-not $target.StartsWith($copyRoot + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Copy target escaped fresh project root.' }
        Assert-NoReparsePath $source
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing closure file: $relative" }
        New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force | Out-Null
        [IO.File]::Copy($source,$target,$false)
        Assert-NoReparsePath $target
        $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        $targetHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        if ($sourceHash -ne $targetHash) { throw "Copy hash mismatch: $relative" }
        $copies.Add([ordered]@{ path=$relative; sourceSha256=$sourceHash; copySha256=$targetHash })
    }
    $results.copiedFiles = $copies.ToArray()
    [IO.File]::WriteAllText((Join-Path $copyRoot '.construction-restart-fixture'),("SF_CONSTRUCTION_RESTART_FIXTURE_V1`n$fixtureId`n"),[Text.UTF8Encoding]::new($false))
    $provenance = @('GameClient\Source\ShatteredFrontier\Private\Construction\SFConstructionSandboxController.cpp','GameClient\Source\ShatteredFrontier\Private\Construction\SFConstructionSandboxState.cpp','GameClient\Source\ShatteredFrontier\Private\Construction\SFConstructionSaveStore.cpp','GameClient\Source\ShatteredFrontier\Private\Tests\SFConstructionSaveRestartScenario.cpp','tools\Test-ConstructionSandboxSaveRestart.ps1')
    $results.provenance = @($provenance | ForEach-Object { [ordered]@{ path=$_; sha256=(Get-FileHash -LiteralPath (Join-Path $repoRoot $_) -Algorithm SHA256).Hash } })
    $normal = Invoke-FiniteRun 'normal-mode-negative' '/Game/Frontier/Maps/Greenlands_E1' @('-ConstructionSaveRestartWriter','-testexit=E1_WORLD_READY')
    if ($normal -notmatch 'E1_WORLD_READY.*NetMode=0' -or $normal -match 'CONSTRUCTION_RESTART_|CONSTRUCTION_TOOLS_READY|CONSTRUCTION_PILOT_READY') { throw 'Normal-mode opt-in isolation failed.' }
    $writerText = Invoke-FiniteRun 'writer' '/Game/Frontier/Maps/Greenlands_ConstructionSandbox' @('-ConstructionSaveRestartWriter')
    $writer = Get-VerifiedRun $writerText 'Writer'
    if ($writerText -notmatch 'CONSTRUCTION_SAVE_RESULT Success=1 Generation=1 SourceRevision=5') { throw 'Writer lacks actual player-save result.' }
    $results.writer = $writer
    $final = Join-Path $copyRoot 'Saved\ConstructionSandbox\generation-00000000000000000001.sfcs'
    Assert-NoReparsePath $final
    if (-not (Test-Path -LiteralPath $final -PathType Leaf) -or (Get-Item -LiteralPath $final).Length -gt 65536) { throw 'Writer did not publish bounded generation1.' }
    $savedHash = (Get-FileHash -LiteralPath $final -Algorithm SHA256).Hash
    $results.publishedFile = [ordered]@{ path=$final; sha256=$savedHash; bytes=(Get-Item -LiteralPath $final).Length }
    # Invoke-FiniteRun returns only after writer WaitForExit. Preserve explicit ordering evidence.
    $results.writerExitedBeforeReader = $ownedProcesses[$ownedProcesses.Count-1].HasExited
    if (-not $results.writerExitedBeforeReader) { throw 'Writer is still running; reader launch refused.' }
    $readerText = Invoke-FiniteRun 'reader' '/Game/Frontier/Maps/Greenlands_ConstructionSandbox' @('-ConstructionSaveRestartReader')
    $reader = Get-VerifiedRun $readerText 'Reader'
    if ($readerText -notmatch 'CONSTRUCTION_LOAD_RESULT Success=1 Generation=1 SourceRevision=5 LiveRevision=1') { throw 'Reader lacks actual player-load result.' }
    if ($reader.contentDigest -ne $writer.contentDigest) { throw 'Restart changed canonical construction/door content.' }
    if ((Get-FileHash -LiteralPath $final -Algorithm SHA256).Hash -ne $savedHash) { throw 'Read-only load changed published save bytes.' }
    $results.reader = $reader
    $results.status = 'Passed'
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
    try {
        $results.ordinaryAfter = Get-OrdinarySaveSnapshot
        $results.ordinarySaveUnchanged = $null -ne $ordinaryBefore -and (($ordinaryBefore | ConvertTo-Json -Depth 8 -Compress) -ceq ($results.ordinaryAfter | ConvertTo-Json -Depth 8 -Compress))
    } catch { $results.ordinarySaveUnchanged=$false; $results.ordinaryInspectionError=$_.Exception.Message }
    if (-not $results.cleanupComplete -or -not $results.ordinarySaveUnchanged) { $results.status='Failed'; $results.error='Owned cleanup incomplete or ordinary save snapshot changed/unverifiable.' }
    $results | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'results.json') -Encoding utf8
    if (-not $results.cleanupComplete -or -not $results.ordinarySaveUnchanged) { throw $results.error }
}
Write-Output "PASS: fixed-root player save/load survives isolated sequential process restart. Evidence: $EvidenceRoot"
