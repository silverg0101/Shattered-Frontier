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
if (-not (Test-Path -LiteralPath $destination -PathType Container)) { throw "Packaged content missing: $destination" }
# DefaultGame.ini stages these NonUFS files for both server platforms.
foreach ($file in @(Get-ChildItem -LiteralPath $source -File -Recurse)) {
    $relative = [IO.Path]::GetRelativePath($source, $file.FullName)
    $staged = Join-Path $destination $relative
    if (-not (Test-Path -LiteralPath $staged -PathType Leaf) -or (Get-FileHash -LiteralPath $staged -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) {
        throw "Packaged content missing or changed: $staged"
    }
}
$greenlands = Join-Path $archive 'ShatteredFrontier/Content/RuntimeData/Greenlands'
foreach ($name in @('silverbrook-town-layout.json','silverbrook-census.json','silverbrook-smartobjects.json')) {
    $inputFile = Join-Path $repo "GameClient/Content/RuntimeData/Greenlands/$name"
    $staged = Join-Path $greenlands $name
    if (-not (Test-Path -LiteralPath $staged -PathType Leaf) -or (Get-FileHash -LiteralPath $staged -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $inputFile -Algorithm SHA256).Hash) {
        throw "Packaged Silverbrook input missing or changed: $staged"
    }
}
Write-Output "Verified packaged content and Silverbrook runtime JSON for $Target."
