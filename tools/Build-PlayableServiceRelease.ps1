[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$SnapshotDirectory,
    [string]$RepositoryRoot = (Join-Path $PSScriptRoot '..'),
    [string]$Dotnet = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe',
    [string]$Packages = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/nuget'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((& git -C $RepositoryRoot rev-parse --show-toplevel).Trim())
if ($LASTEXITCODE) { throw 'RepositoryRoot must identify a Git checkout.' }
function Get-SafeReleasePath([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $cursor = $full
    while ($cursor) {
        if ((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Release paths may not traverse reparse points.'
        }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
    return $full
}
function Test-ReleasePathOverlap([string]$Left, [string]$Right) {
    $comparison = [StringComparison]::OrdinalIgnoreCase
    return $Left.Equals($Right, $comparison) -or $Left.StartsWith($Right + [IO.Path]::DirectorySeparatorChar, $comparison) -or $Right.StartsWith($Left + [IO.Path]::DirectorySeparatorChar, $comparison)
}
$repo = Get-SafeReleasePath $repo
$output = Get-SafeReleasePath $OutputDirectory
if (-not $SnapshotDirectory) { $SnapshotDirectory = "$output.source" }
$snapshot = Get-SafeReleasePath $SnapshotDirectory
if ((Test-ReleasePathOverlap $repo $output) -or (Test-ReleasePathOverlap $repo $snapshot) -or (Test-ReleasePathOverlap $output $snapshot)) {
    throw 'Checkout, output and snapshot paths must not overlap.'
}
if ((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $snapshot)) { throw 'Release output and snapshot must be new directories.' }
$revision = (& git -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $revision -notmatch '^[a-f0-9]{40}$') { throw 'Cannot determine release revision.' }
if (& git -C $repo status --porcelain --untracked-files=normal) { throw 'Commit the complete release candidate before building.' }
if ((& $Dotnet --version).Trim() -ne '10.0.402') { throw 'Pinned .NET SDK 10.0.402 required.' }
# Snapshot all required tracked source, locks, proto and content. Never build or
# recursively copy from the live checkout, where ignored files can contain secrets.
$scope = @('BackendServices', 'proto', 'content', 'ops/steam-server/Services.Dockerfile',
    'ops/steam-server/playable-runtime.py', 'ops/steam-server/playable-entrypoint.sh')
$rootInputs = & git -C $repo ls-tree --name-only $revision
if ($LASTEXITCODE) { throw 'Cannot inspect frozen source tree.' }
$scope += @($rootInputs | Where-Object { $_ -match '^(global\.json|nuget\.config|Directory\.(Build\.(props|targets)|Packages\.props))$' })
$tree = & git -C $repo ls-tree -r $revision -- @scope
if ($LASTEXITCODE -or ($tree | Where-Object { $_ -match '^(120000|160000) ' })) { throw 'Snapshot source must contain only tracked regular files.' }
New-Item -ItemType Directory -Path $snapshot | Out-Null
$archive = Join-Path $snapshot 'source.tar'
& git -C $repo archive --format=tar "--output=$archive" $revision -- @scope
if ($LASTEXITCODE) { throw 'Tracked source archive failed.' }
& tar -xf $archive -C $snapshot
if ($LASTEXITCODE) { throw 'Tracked source archive extraction failed.' }
Remove-Item -LiteralPath $archive
New-Item -ItemType Directory -Path $output | Out-Null
Push-Location (Join-Path $snapshot 'BackendServices')
try { foreach ($hostProject in @('Gameplay.Host', 'InventoryLedger.Host', 'SchemaMigration.Tool')) {
    $project = Join-Path $snapshot "BackendServices/$hostProject/$hostProject.csproj"
    & $Dotnet restore $project --locked-mode --packages $Packages --verbosity quiet
    if ($LASTEXITCODE) { throw "Locked restore failed: $hostProject" }
    & $Dotnet publish $project --no-restore --configuration Release --no-self-contained -p:UseAppHost=false --output (Join-Path $output $hostProject) --verbosity quiet
    if ($LASTEXITCODE) { throw "Publish failed: $hostProject" }
} } finally { Pop-Location }
Copy-Item -LiteralPath (Join-Path $snapshot 'content') -Destination (Join-Path $output 'content') -Recurse
$schema = New-Item -ItemType Directory -Path (Join-Path $output 'base-schema')
foreach ($store in @('InventoryLedger', 'CharacterState', 'Housing', 'Exchange')) {
    Copy-Item -LiteralPath (Join-Path $snapshot "BackendServices/$store.Postgres/Schema.sql") -Destination (Join-Path $schema "$store.sql")
}
Copy-Item -LiteralPath (Join-Path $snapshot 'ops/steam-server/Services.Dockerfile') -Destination (Join-Path $output 'Dockerfile')
$runtime = New-Item -ItemType Directory -Path (Join-Path $output 'Runtime')
foreach ($name in @('playable-runtime.py', 'playable-entrypoint.sh')) {
    Copy-Item -LiteralPath (Join-Path $snapshot "ops/steam-server/$name") -Destination (Join-Path $runtime $name)
}
[IO.File]::WriteAllText((Join-Path $output 'release.json'), (@{ revision=$revision; sdk='10.0.402'; runtime='10.0.13'; authentication='private-development-bearers'; zone='zone/greenlands_silverbrook' } | ConvertTo-Json) + "`n")
$rows = Get-ChildItem -LiteralPath $output -File -Recurse | Sort-Object FullName | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($output, $_.FullName).Replace('\','/')
    '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $relative
}
[IO.File]::WriteAllText((Join-Path $output 'SHA256SUMS'), ($rows -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
Write-Output "SERVICE_RELEASE_READY revision=$revision files=$($rows.Count)"
