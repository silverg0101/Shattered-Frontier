[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][ValidatePattern('^127\.0\.0\.1$')][string]$HostName,
    [Parameter(Mandatory)][ValidateRange(1,65535)][int]$Port,
    [Parameter(Mandatory)][ValidatePattern('^[a-zA-Z_][a-zA-Z_0-9]*$')][string]$Database,
    [Parameter(Mandatory)][ValidatePattern('^[a-zA-Z_][a-zA-Z_0-9]*$')][string]$Username
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$pg = Join-Path $repo 'Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
$pins = Get-Content (Join-Path $repo 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
if ((Get-FileHash (Join-Path $pg 'postgres.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.postgres.executableSha256) { throw 'Pinned PostgreSQL mismatch.' }
if (-not $env:PGPASSWORD) { throw 'Set a synthetic PGPASSWORD in the calling process.' }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Backup destination must be new.' }
function Query([string]$sql) {
    $value = & (Join-Path $pg 'psql.exe') -X -A -t -v ON_ERROR_STOP=1 -h $HostName -p $Port -U $Username -d $Database -c $sql 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Backup query failed: $value" }
    return [string]$value
}
function Counts {
    $value = Query 'SELECT (SELECT count(*) FROM inventory.schema_version), (SELECT count(*) FROM inventory.containers), (SELECT count(*) FROM inventory.entries), (SELECT count(*) FROM inventory.operation_receipts)'
    $parts = $value.Trim().Split('|')
    if ($parts.Count -ne 4) { throw 'Unexpected inventory count response.' }
    return @{ schemaVersionRows=[long]$parts[0]; containers=[long]$parts[1]; entries=[long]$parts[2]; receipts=[long]$parts[3] }
}
$version = (Query 'SELECT version FROM inventory.schema_version WHERE singleton=true').Trim()
if ($version -ne '1') { throw "Unsupported source schema version: $version" }
$before = Counts
[void](New-Item -ItemType Directory -Path $OutputDirectory)
try {
    $dump = Join-Path $OutputDirectory 'inventory.dump'
    & (Join-Path $pg 'pg_dump.exe') -Fc --no-owner --no-acl --schema=inventory -h $HostName -p $Port -U $Username -d $Database -f $dump 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'pg_dump failed.' }
    $after = Counts
    foreach ($key in $before.Keys) { if ($before[$key] -ne $after[$key]) { throw 'Source changed during backup; quiesce writes and retry.' } }
    @{ format=1; schemaVersion=1; dumpSha256=(Get-FileHash $dump -Algorithm SHA256).Hash.ToLowerInvariant(); rowCounts=$before } |
        ConvertTo-Json -Depth 4 | Set-Content (Join-Path $OutputDirectory 'manifest.json')
    Write-Output "Inventory ledger backup complete: $OutputDirectory"
} catch {
    Remove-Item -LiteralPath $OutputDirectory -Recurse -Force
    throw
}
