[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ArchiveRoot,
    [Parameter(Mandatory)][ValidateSet('Server','Client')][string]$Target
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/PackagedServer'))
$archive = [IO.Path]::GetFullPath($ArchiveRoot)
if (-not $archive.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'ArchiveRoot must be a child of this worktree''s ignored Artifacts/PackagedServer directory.'
}
$exe = Join-Path $archive "ShatteredFrontier/Binaries/Win64/ShatteredFrontier$Target.exe"
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Packaged executable missing: $exe" }
$source = Join-Path $repo 'content'
$destination = Join-Path $archive 'content'
if (Test-Path -LiteralPath $destination) { throw "Content was already staged: $destination" }
# The runtime's FPaths::ProjectDir()/../content resolves beside the packaged ShatteredFrontier directory.
Copy-Item -LiteralPath $source -Destination $destination -Recurse
$zone = Join-Path $destination 'world/zones/greenlands_silverbrook.json'
if (-not (Test-Path -LiteralPath $zone -PathType Leaf)) { throw "Staged zone record missing: $zone" }
$files = @(Get-ChildItem -LiteralPath $destination -File -Recurse)
$bytes = ($files | Measure-Object Length -Sum).Sum
$greenlands = Join-Path $archive 'ShatteredFrontier/ArtSource/Greenlands'
$null = New-Item -ItemType Directory -Path $greenlands -Force
foreach ($name in @('silverbrook-town-layout.json','silverbrook-census.json','silverbrook-smartobjects.json')) {
    $inputFile = Join-Path $repo "GameClient/ArtSource/Greenlands/$name"
    if (-not (Test-Path -LiteralPath $inputFile -PathType Leaf)) { throw "Runtime Silverbrook input missing: $inputFile" }
    Copy-Item -LiteralPath $inputFile -Destination (Join-Path $greenlands $name)
}
Write-Output "Staged $($files.Count) content files ($bytes bytes) beside packaged $Target."
