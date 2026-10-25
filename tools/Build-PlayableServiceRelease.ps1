[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$Dotnet = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe',
    [string]$Packages = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/nuget'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Release output must be a new directory.' }
$revision = (& git -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $revision -notmatch '^[a-f0-9]{40}$') { throw 'Cannot determine release revision.' }
if (& git -C $repo status --porcelain --untracked-files=normal) { throw 'Commit the complete release candidate before building.' }
if ((& $Dotnet --version).Trim() -ne '10.0.402') { throw 'Pinned .NET SDK 10.0.402 required.' }
New-Item -ItemType Directory -Path $output | Out-Null
foreach ($hostProject in @('Gameplay.Host', 'InventoryLedger.Host', 'SchemaMigration.Tool')) {
    $project = Join-Path $repo "BackendServices/$hostProject/$hostProject.csproj"
    & $Dotnet restore $project --locked-mode --packages $Packages --verbosity quiet
    if ($LASTEXITCODE) { throw "Locked restore failed: $hostProject" }
    & $Dotnet publish $project --no-restore --configuration Release --no-self-contained -p:UseAppHost=false --output (Join-Path $output $hostProject) --verbosity quiet
    if ($LASTEXITCODE) { throw "Publish failed: $hostProject" }
}
Copy-Item -LiteralPath (Join-Path $repo 'content') -Destination (Join-Path $output 'content') -Recurse
$schema = New-Item -ItemType Directory -Path (Join-Path $output 'base-schema')
foreach ($store in @('InventoryLedger', 'CharacterState', 'Housing', 'Exchange')) {
    Copy-Item -LiteralPath (Join-Path $repo "BackendServices/$store.Postgres/Schema.sql") -Destination (Join-Path $schema "$store.sql")
}
Copy-Item -LiteralPath (Join-Path $repo 'ops/steam-server/Services.Dockerfile') -Destination (Join-Path $output 'Dockerfile')
[IO.File]::WriteAllText((Join-Path $output 'release.json'), (@{ revision=$revision; sdk='10.0.402'; runtime='10.0.13'; authentication='private-development-bearers'; zone='zone/greenlands_silverbrook' } | ConvertTo-Json) + "`n")
$rows = Get-ChildItem -LiteralPath $output -File -Recurse | Sort-Object FullName | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($output, $_.FullName).Replace('\','/')
    '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $relative
}
[IO.File]::WriteAllText((Join-Path $output 'SHA256SUMS'), ($rows -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
Write-Output "SERVICE_RELEASE_READY revision=$revision files=$($rows.Count)"
