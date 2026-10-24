[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Connection,
    [Parameter(Mandatory)][string[]]$Schemas,
    [string]$Role,
    [string]$Dotnet,
    [string]$Nuget
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $Dotnet) { $Dotnet = Join-Path $repo 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe' }
if (-not $Nuget) { $Nuget = Join-Path $repo 'Artifacts/Persistence/Toolchains/nuget' }
$project = Join-Path $repo 'BackendServices/SchemaMigration.Tool/SchemaMigration.Tool.csproj'
& $Dotnet restore $project --locked-mode --packages $Nuget --verbosity quiet
if ($LASTEXITCODE -ne 0) { throw 'Schema migration tool restore failed.' }
& $Dotnet build $project --no-restore --configuration Release --verbosity quiet
if ($LASTEXITCODE -ne 0) { throw 'Schema migration tool build failed.' }
$previous = [Environment]::GetEnvironmentVariable('SF_MIGRATION_CONNECTION')
try {
    $env:SF_MIGRATION_CONNECTION = $Connection
    $arguments = @()
    if ($Role) { $arguments += @('--role', $Role) }
    $arguments += $Schemas
    & $Dotnet (Join-Path $repo 'BackendServices/SchemaMigration.Tool/bin/Release/net10.0/SchemaMigration.Tool.dll') @arguments
    if ($LASTEXITCODE -ne 0) { throw 'Schema migration failed.' }
}
finally { [Environment]::SetEnvironmentVariable('SF_MIGRATION_CONNECTION', $previous) }
