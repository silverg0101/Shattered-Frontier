# Mandatory preflight for normal client cooks. Emits supported UAT CookDir arguments; never starts Unreal.
# Canonical ignored cache is a byte-verified release input, not a dependency on a former worktree.
[CmdletBinding()]
param(
    [string]$Main = 'C:\Projects\Shattered-Frontier',
    [ValidateSet('Client')][string]$TargetType = 'Client',
    [switch]$LinkAssemblies
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$contractPath = Join-Path $repo 'GameClient\Config\ClientCookContract.json'
$contract = Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
if ($contract.schema_version -ne 1 -or $contract.target_type -ne $TargetType) { throw 'Unsupported client cook contract.' }
$cache = [IO.Path]::GetFullPath((Join-Path $Main $contract.assembly_cache_relative))
$manifestPath = Join-Path $cache 'assembly-manifest.json'
$built = Join-Path $cache 'Built'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf) -or -not (Test-Path -LiteralPath $built -PathType Container)) {
    throw "Approved MetaHuman assembly cache is missing: $cache. Restore the approved manifest and assemblies; mannequin fallback does not satisfy client release preflight."
}
if ((Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash -ine $contract.assembly_manifest_sha256) {
    throw 'Assembly manifest differs from the reviewed client cook contract.'
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
foreach ($directory in @((Get-Item -LiteralPath $built -Force)) + @(Get-ChildItem -LiteralPath $built -Recurse -Directory -Force)) {
    if ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Approved assembly cache must contain physical directories, not further links.' }
}
$entries = @($manifest.Files)
$files = @(Get-ChildItem -LiteralPath $built -Recurse -File -Force)
if ($entries.Count -ne $contract.assembly_files -or $files.Count -ne $entries.Count) { throw 'Assembly file census differs from the reviewed manifest.' }
$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
[long]$bytes = 0
foreach ($entry in $entries) {
    $path = [IO.Path]::GetFullPath((Join-Path $built $entry.Path))
    if (-not $path.StartsWith($built.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or -not $seen.Add($path)) {
        throw 'Assembly manifest has a duplicate or escaping path.'
    }
    $file = Get-Item -LiteralPath $path -Force
    if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint -or $file.Length -ne $entry.Bytes -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $entry.Sha256) {
        throw "Approved assembly bytes differ: $($entry.Path)"
    }
    $bytes += $file.Length
}
if ($bytes -ne $contract.assembly_bytes) { throw 'Approved assembly byte total differs.' }
foreach ($file in $files) { if (-not $seen.Contains($file.FullName)) { throw "Unmanifested assembly file: $($file.FullName)" } }

$destination = Join-Path $repo 'GameClient\Content\Frontier\Characters\Player\Built'
if (-not (Test-Path -LiteralPath $destination)) {
    if (-not $LinkAssemblies) { throw 'Built assemblies are not linked in this checkout. Run this preflight with -LinkAssemblies; no existing directory will be replaced.' }
    New-Item -ItemType Junction -Path $destination -Target $built | Out-Null
}
$link = Get-Item -LiteralPath $destination -Force
if ($link.LinkType -ne 'Junction' -or [IO.Path]::GetFullPath($link.Target).TrimEnd('\') -ine $built.TrimEnd('\')) {
    throw "Built must be the approved cache junction; existing content is not replaced: $destination"
}

# Derive all approved runtime face IDs from the owning release config, never duplicate the list in the tool.
$ini = Get-Content -LiteralPath (Join-Path $repo 'GameClient\Config\DefaultGame.ini')
$inSettings = $false
$faces = [Collections.Generic.List[string]]::new()
foreach ($line in $ini) {
    if ($line -match '^\[(.+)\]') { $inSettings = $Matches[1] -eq '/Script/ShatteredFrontier.SFE1Settings'; continue }
    if ($inSettings -and $line -match '^\+PlayerFaceIds=([A-Za-z][A-Za-z0-9]*)\s*$') { $faces.Add($Matches[1]) }
}
if ($faces.Count -eq 0 -or @($faces | Select-Object -Unique).Count -ne $faces.Count) { throw 'Approved player face declaration is empty or duplicated.' }
$packages = @($contract.required_packages) + @($faces | ForEach-Object { "/Game/Frontier/Characters/Player/Built/MHC_$_/BP_MHC_$_" })
foreach ($package in $packages) {
    $asset = Join-Path $repo ('GameClient/Content/' + $package.Substring('/Game/'.Length) + '.uasset')
    if (-not (Test-Path -LiteralPath $asset -PathType Leaf)) { throw "Required client avatar package is missing: $package" }
}
$directories = @($contract.cook_roots | ForEach-Object { [IO.Path]::GetFullPath((Join-Path $repo ('GameClient/Content/' + $_.Substring('/Game/'.Length)))) })
foreach ($directory in $directories) { if (-not (Test-Path -LiteralPath $directory -PathType Container)) { throw "Missing client cook root: $directory" } }
[pscustomobject]@{
    TargetType = $TargetType
    AssemblyManifestSha256 = $contract.assembly_manifest_sha256
    AssemblyFiles = $entries.Count
    AssemblyBytes = $bytes
    FaceIds = @($faces)
    RequiredPackages = $packages
    CookRoots = @($contract.cook_roots)
    UatArguments = @('-CookDir=' + ($directories -join '+'))
    ContractSha256 = (Get-FileHash -LiteralPath $contractPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Status = 'PreflightPassed; cook/package/runtime closure not yet verified'
}
