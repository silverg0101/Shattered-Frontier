[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('InventoryLedger','CharacterState','Housing','Exchange','TerrainDelta')][string]$Store,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][ValidatePattern('^127\.0\.0\.1$')][string]$HostName,
    [Parameter(Mandatory)][ValidateRange(1,65535)][int]$Port,
    [Parameter(Mandatory)][ValidatePattern('^[a-zA-Z_][a-zA-Z_0-9]*$')][string]$Database,
    [Parameter(Mandatory)][ValidatePattern('^[a-zA-Z_][a-zA-Z_0-9]*$')][string]$Username,
    [ValidateRange(60,86400)][int]$PendingPlacementRecoveryWindowSeconds = 3600,
    [ValidateRange(60,86400)][int]$PendingSettlementRecoveryWindowSeconds = 3600
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function FileSha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
    finally { $stream.Dispose(); $hasher.Dispose() }
}
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$pg = Join-Path $repo 'Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
$pins = Get-Content (Join-Path $repo 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
if ((FileSha256 (Join-Path $pg 'postgres.exe')) -ne $pins.postgres.executableSha256) { throw 'Pinned PostgreSQL mismatch.' }
if (-not $env:PGPASSWORD) { throw 'Set a synthetic PGPASSWORD in the calling process.' }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Backup destination must be new.' }
$tables = switch ($Store) {
    InventoryLedger { @('inventory.schema_version','inventory.containers','inventory.entries','inventory.operation_receipts','inventory.receipt_outbox') }
    CharacterState { @('character_state.schema_version','character_state.progression','character_state.quests','character_state.source_receipts','character_state.reward_receipts','character_state.positions','character_state.recipe_intents') }
    Housing { @('housing.schema_version','housing.claims','housing.claim_operations','housing.claim_structure_revisions','housing.placements','housing.claim_geometry','housing.removals','inventory.schema_version','inventory.containers','inventory.entries','inventory.operation_receipts','inventory.receipt_outbox') }
    Exchange { @('exchange.schema_version','exchange.wallets','exchange.wallet_entries','exchange.operations','exchange.listings','exchange.settlements') }
    TerrainDelta { @('terrain_delta.schema_version','terrain_delta.regions','terrain_delta.operations') }
}
$schemas = if ($Store -eq 'Housing') { @('housing','inventory') } elseif ($Store -eq 'CharacterState') { @('character_state') } elseif ($Store -eq 'Exchange') { @('exchange') } elseif ($Store -eq 'TerrainDelta') { @('terrain_delta') } else { @('inventory') }
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
    $expectedVersion = if ($schema -eq 'housing') { '6' } elseif ($schema -eq 'character_state') { '4' } elseif ($schema -eq 'exchange') { '3' } elseif ($schema -eq 'inventory') { '2' } else { '1' }
    if ($version -ne $expectedVersion) { throw "Unsupported source schema version: $schema=$version" }
}
if ($Store -eq 'Housing') {
    $stale = (Query "SELECT (SELECT count(*) FROM housing.placements WHERE state='Pending' AND created_at < clock_timestamp() - make_interval(secs => $PendingPlacementRecoveryWindowSeconds)) + (SELECT count(*) FROM housing.removals r JOIN housing.placements p ON p.operation_id=r.placement_operation_id WHERE r.state='Pending' AND p.created_at < clock_timestamp() - make_interval(secs => $PendingPlacementRecoveryWindowSeconds))").Trim()
    if ($stale -ne '0') { throw 'Housing pending action exceeds recovery window.' }
}
if ($Store -eq 'Exchange') {
    $stale = (Query "SELECT (SELECT count(*) FROM exchange.settlements WHERE state='pending' AND created_at < clock_timestamp() - make_interval(secs => $PendingSettlementRecoveryWindowSeconds)) + (SELECT count(*) FROM exchange.operations WHERE receipt->>'Code'='pending' AND fingerprint::jsonb->>'Command'='listing-intent' AND (fingerprint::jsonb->>'CreatedAt')::timestamptz < clock_timestamp() - make_interval(secs => $PendingSettlementRecoveryWindowSeconds))").Trim()
    if ($stale -ne '0') { throw 'Exchange pending settlement exceeds recovery window.' }
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
    $dumpHash = FileSha256 $dump
    @{ format=2; store=$Store; schemaVersion=$(if ($Store -eq 'Housing') { 5 } elseif ($Store -eq 'CharacterState') { 4 } elseif ($Store -eq 'Exchange') { 3 } else { 2 }); dumpSha256=$dumpHash; tables=$before } |
        ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDirectory 'manifest.json')
    Write-Output "$Store backup complete: $OutputDirectory"
} catch {
    Remove-Item -LiteralPath $OutputDirectory -Recurse -Force
    throw
}
