[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('InventoryLedger','CharacterState','Housing')][string]$Store,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][ValidatePattern('^127\.0\.0\.1$')][string]$HostName,
    [Parameter(Mandatory)][ValidateRange(1,65535)][int]$Port,
    [Parameter(Mandatory)][ValidatePattern('^[a-zA-Z_][a-zA-Z_0-9]*$')][string]$Database,
    [Parameter(Mandatory)][ValidatePattern('^[a-zA-Z_][a-zA-Z_0-9]*$')][string]$Username,
    [ValidateRange(60,86400)][int]$PendingPlacementRecoveryWindowSeconds = 3600
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
$tables = switch ($Store) {
    InventoryLedger { @('inventory.schema_version','inventory.containers','inventory.entries','inventory.operation_receipts') }
    CharacterState { @('character_state.schema_version','character_state.progression','character_state.quests','character_state.source_receipts','character_state.reward_receipts') }
    Housing { @('housing.schema_version','housing.claims','housing.claim_operations','housing.claim_structure_revisions','housing.placements','inventory.schema_version','inventory.containers','inventory.entries','inventory.operation_receipts') }
}
$schemas = if ($Store -eq 'Housing') { @('housing','inventory') } elseif ($Store -eq 'CharacterState') { @('character_state') } else { @('inventory') }
function Query([string]$sql) {
    $value = & (Join-Path $pg 'psql.exe') -X -A -t -v ON_ERROR_STOP=1 -h $HostName -p $Port -U $Username -d $Database -c $sql 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Backup query failed: $value" }
    return [string]$value
}
function Snapshot {
    $result = [ordered]@{}
    foreach ($table in $tables) {
        $line = (Query "SELECT count(*),md5(coalesce(string_agg(row_to_json(t)::text, '|' ORDER BY row_to_json(t)::text),'')) FROM $table t").Trim().Split('|')
        if ($line.Count -ne 2) { throw "Invalid snapshot for $table" }
        $result[$table] = @{ count=[long]$line[0]; digest=$line[1] }
    }
    return $result
}
foreach ($schema in $schemas) {
    $version = (Query "SELECT version FROM $schema.schema_version WHERE singleton=true").Trim()
    $expectedVersion = if ($schema -eq 'housing') { '3' } else { '1' }
    if ($version -ne $expectedVersion) { throw "Unsupported source schema version: $schema=$version" }
}
if ($Store -eq 'Housing') {
    $stale = (Query "SELECT count(*) FROM housing.placements WHERE state='Pending' AND created_at < clock_timestamp() - make_interval(secs => $PendingPlacementRecoveryWindowSeconds)").Trim()
    if ($stale -ne '0') { throw 'Housing pending placement exceeds recovery window.' }
}
$before = Snapshot
[void](New-Item -ItemType Directory -Path $OutputDirectory)
try {
    $dump = Join-Path $OutputDirectory "$($Store.ToLowerInvariant()).dump"
    $schemaArgs = @($schemas | ForEach-Object { "--schema=$_" })
    & (Join-Path $pg 'pg_dump.exe') -Fc --no-owner --no-acl @schemaArgs -h $HostName -p $Port -U $Username -d $Database -f $dump 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'pg_dump failed.' }
    $after = Snapshot
    if (($before | ConvertTo-Json -Depth 5 -Compress) -cne ($after | ConvertTo-Json -Depth 5 -Compress)) { throw 'Source changed during backup; quiesce writes and retry.' }
    @{ format=2; store=$Store; schemaVersion=$(if ($Store -eq 'Housing') { 3 } else { 1 }); dumpSha256=(Get-FileHash $dump -Algorithm SHA256).Hash.ToLowerInvariant(); tables=$before } |
        ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDirectory 'manifest.json')
    Write-Output "$Store backup complete: $OutputDirectory"
} catch {
    Remove-Item -LiteralPath $OutputDirectory -Recurse -Force
    throw
}
