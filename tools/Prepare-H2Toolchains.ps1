[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows) { throw 'This pinned archive workflow is Windows x64 only.' }
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$cacheRoot = Join-Path $repoRoot 'Artifacts/Persistence/Toolchains'
$pins = Get-Content -LiteralPath (Join-Path $repoRoot 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Path $cacheRoot -Force | Out-Null
foreach ($pin in @($pins.sdk, $pins.postgres)) {
    if ($pin.archive -notmatch '^[a-zA-Z0-9_.-]+\.zip$' -or $pin.directory -notmatch '^[a-zA-Z0-9_.-]+$') {
        throw 'Unsafe archive/directory entry in toolchain lock.'
    }
    $archive = Join-Path $cacheRoot $pin.archive
    $destination = Join-Path $cacheRoot $pin.directory
    $tool = [IO.Path]::GetFullPath((Join-Path $destination $pin.executable))
    if (-not $tool.StartsWith($destination + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Executable path escapes its pinned directory.'
    }
    if (-not (Test-Path -LiteralPath $archive)) {
        $partial = $archive + '.partial'
        if (Test-Path -LiteralPath $partial) { throw "Partial download exists; inspect it before retry: $partial" }
        Invoke-WebRequest -Uri $pin.url -OutFile $partial
        if ((Get-FileHash -LiteralPath $partial -Algorithm $pin.hashAlgorithm).Hash.ToLowerInvariant() -ne $pin.hash) {
            throw "Downloaded archive hash mismatch: $($pin.archive)"
        }
        Move-Item -LiteralPath $partial -Destination $archive
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm $pin.hashAlgorithm).Hash.ToLowerInvariant() -ne $pin.hash) {
        throw "Cached archive hash mismatch: $($pin.archive)"
    }
    if (-not (Test-Path -LiteralPath $destination)) {
        # Extract only into a fresh, fixed cache child. No overwrite or cleanup of existing toolchains.
        Expand-Archive -LiteralPath $archive -DestinationPath $destination
    }
    if (-not (Test-Path -LiteralPath $tool) -or
        (Get-FileHash -LiteralPath $tool -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.executableSha256) {
        throw "Pinned executable missing or mismatched: $tool"
    }
}
[pscustomobject]@{
    Dotnet = Join-Path $cacheRoot 'dotnet-10.0.402/dotnet.exe'
    PostgresBin = Join-Path $cacheRoot 'postgresql-18.6/pgsql/bin'
    NuGetPackages = Join-Path $cacheRoot 'nuget'
} | ConvertTo-Json
