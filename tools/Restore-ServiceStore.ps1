[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('InventoryLedger','CharacterState','Housing','Exchange','TerrainDelta')][string]$Store,
    [string]$BackupDirectory,
    [Parameter(Mandatory)][string]$InstanceRoot,
    [switch]$Cleanup,
    [ValidateRange(60,86400)][int]$PendingPlacementRecoveryWindowSeconds = 3600,
    [ValidateRange(60,86400)][int]$PendingSettlementRecoveryWindowSeconds = 3600
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowed = [IO.Path]::GetFullPath((Join-Path $repo ('Artifacts/Persistence/' + $Store + 'Durable')))
$InstanceRoot = [IO.Path]::GetFullPath($InstanceRoot)
if (-not $InstanceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $InstanceRoot -eq $allowed) { throw 'Instance root must be an owned durable-store child.' }
$pg = Join-Path $repo 'Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
$pins = Get-Content (Join-Path $repo 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
if ((Get-FileHash (Join-Path $pg 'postgres.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.postgres.executableSha256) { throw 'Pinned PostgreSQL mismatch.' }
$data = Join-Path $InstanceRoot 'data'
$markerPath = Join-Path $InstanceRoot 'owner.json'
function Invoke-Control([string[]]$arguments) {
    $info = [Diagnostics.ProcessStartInfo]::new(); $info.FileName = Join-Path $pg 'pg_ctl.exe'
    $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true
    $info.Arguments = (($arguments | ForEach-Object { '"' + $_.Replace('"','\"') + '"' }) -join ' ')
    $child = [Diagnostics.Process]::new(); $child.StartInfo = $info; [void]$child.Start()
    $stdout = $child.StandardOutput.ReadToEndAsync(); $stderr = $child.StandardError.ReadToEndAsync()
    if (-not $child.WaitForExit(45000)) { $child.Kill(); throw 'Owned PostgreSQL control timeout.' }
    [void]$stdout.Wait(1000); [void]$stderr.Wait(1000)
    return $child.ExitCode
}
function Assert-Owned {
    if (-not (Test-Path -LiteralPath $markerPath)) { throw 'Missing restore owner marker.' }
    $marker = Get-Content $markerPath -Raw | ConvertFrom-Json
    if ($marker.data -ne $data -or $marker.tool -ne 'Restore-ServiceStore.ps1' -or $marker.store -ne $Store) { throw 'Restore owner marker mismatch.' }
    foreach ($path in @($InstanceRoot,$data)) { if ((Get-Item $path).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse point in owned path.' } }
    $pidPath = Join-Path $data 'postmaster.pid'
    if (Test-Path $pidPath) {
        $lines = @(Get-Content $pidPath -TotalCount 4)
        if ($lines.Count -lt 4 -or [IO.Path]::GetFullPath($lines[1]) -ne $data -or [int]$lines[3] -ne [int]$marker.port) { throw 'Postmaster identity mismatch.' }
        $process = Get-CimInstance Win32_Process -Filter "ProcessId=$($lines[0])"
        if ($process -and ([IO.Path]::GetFullPath($process.ExecutablePath) -ne [IO.Path]::GetFullPath((Join-Path $pg 'postgres.exe')) -or $process.CommandLine.Replace('/','\') -notlike "*$data*")) { throw 'Postmaster process mismatch.' }
    }
    return $marker
}
function Remove-Owned {
    $marker = Assert-Owned
    if ((Invoke-Control @('-D',$data,'status')) -eq 0) {
        if ((Invoke-Control @('-D',$data,'-m','immediate','-w','-t','30','stop')) -ne 0) { throw 'Could not stop owned restore cluster.' }
    }
    [void](Assert-Owned)
    if (@(Get-ChildItem -LiteralPath $InstanceRoot -Recurse -Attributes ReparsePoint).Count) { throw 'Owned tree contains a reparse point.' }
    Remove-Item -LiteralPath $InstanceRoot -Recurse -Force
}
if ($Cleanup) { Remove-Owned; Write-Output 'Owned restore instance removed.'; return }
if (-not $BackupDirectory) { throw 'Supply BackupDirectory.' }
$BackupDirectory = [IO.Path]::GetFullPath($BackupDirectory)
$manifest = Get-Content (Join-Path $BackupDirectory 'manifest.json') -Raw | ConvertFrom-Json
$dump = Join-Path $BackupDirectory "$($Store.ToLowerInvariant()).dump"
if ($manifest.format -ne 2 -or $manifest.store -cne $Store -or $manifest.schemaVersion -ne $(if ($Store -eq 'Housing') { 5 } elseif ($Store -eq 'Exchange' -or $Store -eq 'CharacterState') { 3 } else { 2 }) -or $manifest.dumpSha256 -notmatch '^[a-f0-9]{64}$' -or
    (Get-FileHash $dump -Algorithm SHA256).Hash.ToLowerInvariant() -cne $manifest.dumpSha256) { throw 'Backup manifest or SHA256 mismatch.' }
$tables = switch ($Store) {
    InventoryLedger { @('inventory.schema_version','inventory.containers','inventory.entries','inventory.operation_receipts','inventory.receipt_outbox') }
    CharacterState { @('character_state.schema_version','character_state.progression','character_state.quests','character_state.source_receipts','character_state.reward_receipts','character_state.positions') }
    Housing { @('housing.schema_version','housing.claims','housing.claim_operations','housing.claim_structure_revisions','housing.placements','housing.claim_geometry','housing.removals','inventory.schema_version','inventory.containers','inventory.entries','inventory.operation_receipts','inventory.receipt_outbox') }
    Exchange { @('exchange.schema_version','exchange.wallets','exchange.wallet_entries','exchange.operations','exchange.listings','exchange.settlements') }
    TerrainDelta { @('terrain_delta.schema_version','terrain_delta.regions','terrain_delta.operations') }
}
$schemas = if ($Store -eq 'Housing') { @('housing','inventory') } elseif ($Store -eq 'CharacterState') { @('character_state') } elseif ($Store -eq 'Exchange') { @('exchange') } elseif ($Store -eq 'TerrainDelta') { @('terrain_delta') } else { @('inventory') }
$serviceRole = switch ($Store) { InventoryLedger { 'ledger_svc' } CharacterState { 'state_svc' } Housing { 'housing_svc' } Exchange { 'exchange_svc' } TerrainDelta { 'terrain_delta_svc' } }
if (@($manifest.tables.PSObject.Properties).Count -ne $tables.Count) { throw 'Invalid manifest table set.' }
foreach ($table in $tables) {
    $entry = $manifest.tables.PSObject.Properties[$table].Value
    if ($null -eq $entry -or $entry.count -lt 0 -or $entry.digest -notmatch '^[a-f0-9]{32}$') { throw 'Invalid manifest table snapshot.' }
}
foreach ($schema in $schemas) { if ($manifest.tables.PSObject.Properties["$schema.schema_version"].Value.count -ne 1) { throw 'Invalid schema marker count.' } }
if (Test-Path -LiteralPath $InstanceRoot) { throw 'Restore target must be new.' }
$rng = [Security.Cryptography.RandomNumberGenerator]::Create(); $bytes = [byte[]]::new(32)
$rng.GetBytes($bytes); $adminPassword = [BitConverter]::ToString($bytes).Replace('-','')
$rng.GetBytes($bytes); $servicePassword = [BitConverter]::ToString($bytes).Replace('-',''); $rng.Dispose()
$listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0); $listener.Start(); $port = $listener.LocalEndpoint.Port; $listener.Stop()
$database = 'store_restore_' + [guid]::NewGuid().ToString('N')
$oldPassword = $env:PGPASSWORD
try {
    [void](New-Item -ItemType Directory -Path $InstanceRoot)
    $account = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    & icacls.exe $InstanceRoot /inheritance:r /grant:r "${account}:(OI)(CI)F" *> $null
    if ($LASTEXITCODE -ne 0) { throw 'Could not restrict restore directory.' }
    @{tool='Restore-ServiceStore.ps1';store=$Store;data=$data;port=$port} | ConvertTo-Json | Set-Content $markerPath
    $pwfile = Join-Path $InstanceRoot 'admin-password'; [IO.File]::WriteAllText($pwfile,$adminPassword)
    & (Join-Path $pg 'initdb.exe') -D $data -U restore_admin --pwfile=$pwfile --auth=scram-sha-256 --encoding=UTF8 --no-locale *> $null
    if ($LASTEXITCODE -ne 0) { throw 'initdb failed.' }
    Remove-Item $pwfile -Force
    [IO.File]::AppendAllText((Join-Path $data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$port`nfsync=on`nsynchronous_commit=on`n")
    [IO.File]::WriteAllLines((Join-Path $data 'pg_hba.conf'),@('host all restore_admin 127.0.0.1/32 scram-sha-256',"host $database $serviceRole 127.0.0.1/32 scram-sha-256",'host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    [void](Assert-Owned)
    if ((Invoke-Control @('-D',$data,'-l',(Join-Path $InstanceRoot 'postgres.log'),'-w','-t','30','start')) -ne 0) { throw 'Restore cluster failed to start.' }
    $env:PGPASSWORD = $adminPassword
    function Sql([string]$query,[string]$db='postgres') {
        $output = & (Join-Path $pg 'psql.exe') -X -A -t -v ON_ERROR_STOP=1 -h 127.0.0.1 -p $port -U restore_admin -d $db -c $query 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Restore SQL failed: $output" }
        return [string]$output
    }
    [void](Sql "CREATE ROLE store_owner NOLOGIN; CREATE ROLE $serviceRole LOGIN PASSWORD '$servicePassword';")
    [void](Sql "CREATE DATABASE $database OWNER store_owner")
    [void](Sql "REVOKE ALL ON DATABASE $database FROM PUBLIC; GRANT CONNECT ON DATABASE $database TO $serviceRole" $database)
    & (Join-Path $pg 'pg_restore.exe') --exit-on-error --no-owner --no-acl --role=store_owner -h 127.0.0.1 -p $port -U restore_admin -d $database $dump *> $null
    if ($LASTEXITCODE -ne 0) { throw 'pg_restore failed.' }
    foreach ($schema in $schemas) {
        $grant = if ($Store -eq 'Exchange') { "GRANT SELECT ON $schema.schema_version TO $serviceRole; GRANT SELECT,INSERT,UPDATE ON exchange.wallets,exchange.wallet_entries,exchange.operations,exchange.listings,exchange.settlements TO $serviceRole" } else { "GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA $schema TO $serviceRole" }
        [void](Sql "GRANT USAGE ON SCHEMA $schema TO $serviceRole; $grant" $database)
        $version = (Sql "SELECT version FROM $schema.schema_version WHERE singleton=true" $database).Trim()
        $expectedVersion = if ($schema -eq 'housing') { '5' } elseif ($schema -eq 'exchange' -or $schema -eq 'character_state') { '3' } elseif ($schema -eq 'inventory') { '2' } else { '1' }
        if ($version -ne $expectedVersion) { throw 'Restored schema version mismatch.' }
    }
    foreach ($table in $tables) {
        $line = (Sql "SELECT count(*),md5(coalesce(string_agg(row_to_json(t)::text, '|' ORDER BY row_to_json(t)::text),'')) FROM $table t" $database).Trim().Split('|')
        $entry = $manifest.tables.PSObject.Properties[$table].Value
        if ($line.Count -ne 2 -or [long]$line[0] -ne [long]$entry.count -or $line[1] -cne $entry.digest) { throw "Restored table mismatch: $table" }
    }
    if ($Store -eq 'InventoryLedger') {
        $bad = Sql @'
WITH changes AS (
 SELECT (r.receipt->>'OperationId') AS operation_id,
        e->>'ContainerId' AS container_id,
        (e->>'Before')::bigint AS before_revision,
        (e->>'After')::bigint AS after_revision
 FROM inventory.operation_receipts r CROSS JOIN LATERAL jsonb_array_elements(r.receipt->'Revisions') e
), ordered AS (
 SELECT changes.*, row_number() OVER (PARTITION BY container_id ORDER BY after_revision) AS sequence
 FROM changes
)
SELECT count(*) FROM (
 SELECT 1 FROM inventory.entries e LEFT JOIN inventory.containers c ON c.id=e.container_id
 WHERE e.quantity < 0 OR c.id IS NULL
 UNION ALL
 SELECT 1 FROM ordered o LEFT JOIN inventory.containers c ON c.id=o.container_id
 WHERE c.id IS NULL OR o.before_revision < 0 OR o.after_revision <> o.before_revision+1
    OR o.after_revision <> o.sequence OR o.after_revision > c.revision
 UNION ALL
 SELECT 1 FROM inventory.containers c WHERE c.revision < 0 OR c.revision <> (SELECT count(*) FROM changes x WHERE x.container_id=c.id)
 UNION ALL
 SELECT 1 FROM inventory.operation_receipts r
 WHERE r.receipt->>'OperationId' IS DISTINCT FROM r.operation_id
    OR jsonb_typeof(r.receipt->'Revisions') IS DISTINCT FROM 'array'
    OR jsonb_array_length(r.receipt->'Revisions') = 0
) invalid
'@ $database
        if ($bad.Trim() -ne '0') { throw 'Restored ledger consistency check failed.' }
    }
    if ($Store -eq 'CharacterState') {
        $bad = Sql @'
SELECT count(*) FROM (
 SELECT 1 FROM character_state.progression p WHERE
  (p.state->>'LifetimeXp')::bigint < 0 OR
  EXISTS (SELECT 1 FROM jsonb_each_text(p.state->'Skills') s WHERE s.value::integer NOT BETWEEN 0 AND 1000) OR
  (SELECT coalesce(sum(s.value::integer),0) FROM jsonb_each_text(p.state->'Skills') s) > 7200
 UNION ALL
 SELECT 1 FROM character_state.quests q WHERE (q.state->>'StepIndex')::integer < 0 OR
  q.revision < (q.state->>'StepIndex')::integer OR
  (q.state->>'LastOccurredAt')::timestamptz < (q.state->>'AcceptedAt')::timestamptz OR
  EXISTS (SELECT 1 FROM jsonb_each_text(q.state->'Counts') c WHERE c.value::integer < 0)
 UNION ALL
 SELECT 1 FROM character_state.source_receipts r WHERE
  (r.domain='progression' AND NOT EXISTS (SELECT 1 FROM character_state.progression p WHERE p.character_id=r.character_id)) OR
  (r.domain='quest' AND NOT EXISTS (SELECT 1 FROM character_state.quests q WHERE q.character_id=r.character_id))
 UNION ALL
 SELECT 1 FROM character_state.reward_receipts r WHERE NOT EXISTS
  (SELECT 1 FROM character_state.quests q WHERE q.character_id=r.character_id AND q.quest_id=r.quest_id)
  OR (r.state='Pending' AND (r.content_revision <= 0 OR jsonb_typeof(r.item_lines) IS DISTINCT FROM 'array'
   OR jsonb_array_length(r.item_lines)=0))
 UNION ALL
 SELECT 1 FROM character_state.positions p WHERE p.revision < 1 OR
  p.zone_id !~ '^[a-z0-9_]{1,64}$' OR
  p.x NOT BETWEEN -10000000 AND 10000000 OR
  p.y NOT BETWEEN -10000000 AND 10000000 OR
  p.z NOT BETWEEN -10000000 AND 10000000 OR
  p.yaw NOT BETWEEN -360 AND 360
) invalid
'@ $database
        if ($bad.Trim() -ne '0') { throw 'Restored character consistency check failed.' }
    }
    if ($Store -eq 'Housing') {
        $bad = Sql @"
SELECT count(*) FROM (
 SELECT 1 FROM housing.placements p WHERE p.shape_key !~ '^[a-z0-9._-]+$' OR
  length(p.shape_key) NOT BETWEEN 1 AND 128 OR p.orientation_index NOT BETWEEN 0 AND 23
 UNION ALL
 SELECT 1 FROM housing.placements p WHERE p.state='Pending' AND
  p.created_at < clock_timestamp() - make_interval(secs => $PendingPlacementRecoveryWindowSeconds)
 UNION ALL
 SELECT 1 FROM housing.placements p WHERE p.state='Committed' AND (
  p.consume_receipt IS NULL OR p.consume_receipt->>'OperationId' IS DISTINCT FROM p.operation_id || '/consume' OR
  p.consume_receipt->>'Action' IS DISTINCT FROM '1' OR
  NOT EXISTS (SELECT 1 FROM jsonb_array_elements(p.consume_receipt->'Changes') c WHERE
    c->>'ContainerId'=p.container_id AND c->>'DefinitionId'=p.material_id AND
    (c->>'Delta')::bigint = -p.material_quantity) OR
  NOT EXISTS (SELECT 1 FROM inventory.operation_receipts r WHERE r.operation_id=p.operation_id || '/consume'
    AND r.receipt=p.consume_receipt))
 UNION ALL
 SELECT 1 FROM housing.claims c WHERE NOT EXISTS (SELECT 1 FROM housing.claim_geometry g WHERE g.claim_id=c.id)
 UNION ALL
 SELECT 1 FROM housing.placements a JOIN housing.placements b ON a.claim_id=b.claim_id AND
  a.cell_x=b.cell_x AND a.cell_y=b.cell_y AND a.cell_z=b.cell_z AND a.operation_id < b.operation_id
  WHERE a.state IN ('Pending','Committed') AND b.state IN ('Pending','Committed')
 UNION ALL
 SELECT 1 FROM housing.removals r JOIN housing.placements p ON p.operation_id=r.placement_operation_id WHERE
  (r.state='Pending' AND p.created_at < clock_timestamp() - make_interval(secs => $PendingPlacementRecoveryWindowSeconds)) OR
  (r.state='Committed' AND (p.state<>'Removed' OR (r.refund_quantity=0 AND r.refund_receipt IS NOT NULL) OR (r.refund_quantity>0 AND (
    r.refund_receipt IS NULL OR r.refund_receipt->>'OperationId' IS DISTINCT FROM r.operation_id || '/refund' OR
    r.refund_receipt->>'Action' IS DISTINCT FROM '0' OR
    NOT EXISTS (SELECT 1 FROM jsonb_array_elements(r.refund_receipt->'Changes') c WHERE
      c->>'ContainerId'=p.container_id AND c->>'DefinitionId'=p.material_id AND (c->>'Delta')::bigint=r.refund_quantity) OR
    NOT EXISTS (SELECT 1 FROM inventory.operation_receipts ir WHERE ir.operation_id=r.operation_id || '/refund' AND ir.receipt=r.refund_receipt))))) OR
  (r.state='Compensated' AND (p.state<>'Committed' OR r.refund_receipt IS NOT NULL OR r.refund_quantity=0 OR
    NOT EXISTS (SELECT 1 FROM inventory.operation_receipts ir WHERE ir.operation_id=r.operation_id || '/refund' AND
      ir.fingerprint='housing/compensated' AND ir.receipt->>'OperationId'=ir.operation_id AND
      ir.receipt->>'Action'='0' AND ir.receipt->'Changes'='[]'::jsonb AND ir.receipt->'Revisions'='[]'::jsonb)))
) invalid
"@ $database
        if ($bad.Trim() -ne '0') { throw 'Restored housing consistency check failed.' }
    }
    if ($Store -eq 'Exchange') {
        $bad = Sql @"
SELECT count(*) FROM (
 SELECT 1 FROM exchange.wallets w WHERE w.balance < 0 OR w.reserved < 0 OR w.reserved > w.balance OR
  w.balance::numeric IS DISTINCT FROM (SELECT coalesce(sum(e.delta::numeric),0) FROM exchange.wallet_entries e WHERE e.owner_id=w.owner_id) OR
  w.revision IS DISTINCT FROM (SELECT count(*) FROM exchange.wallet_entries e WHERE e.owner_id=w.owner_id)
 UNION ALL
 SELECT 1 FROM exchange.settlements s WHERE s.state='pending' AND
  s.created_at < clock_timestamp() - make_interval(secs => $PendingSettlementRecoveryWindowSeconds)
 UNION ALL
 SELECT 1 FROM exchange.operations o WHERE o.fingerprint::jsonb->>'Command'='listing-intent' AND (
  (o.receipt->>'Code'='pending' AND (o.fingerprint::jsonb->>'CreatedAt')::timestamptz < clock_timestamp() - make_interval(secs => $PendingSettlementRecoveryWindowSeconds)) OR
  (o.receipt->>'Code'='ok' AND NOT EXISTS (SELECT 1 FROM exchange.listings l WHERE l.id=o.receipt->>'SubjectId')))
 UNION ALL
 SELECT 1 FROM exchange.settlements s WHERE s.kind='listing' AND s.state='committed' AND (
  NOT EXISTS (SELECT 1 FROM exchange.listings l WHERE l.id=s.subject_id AND l.state='sold') OR
  (SELECT count(*) FROM exchange.wallet_entries e WHERE e.operation_id=s.operation_id AND e.owner_id=s.payer_id AND e.flow_class='transfer' AND e.code='market_purchase' AND e.delta=-s.amount) <> 1 OR
  (SELECT count(*) FROM exchange.wallet_entries e WHERE e.operation_id=s.operation_id AND e.owner_id=s.payee_id AND e.flow_class='transfer' AND e.code='market_sale' AND e.delta=s.amount) <> 1 OR
  (SELECT count(*) FROM exchange.wallet_entries e WHERE e.operation_id=s.operation_id AND e.owner_id=s.payee_id AND e.flow_class='sink' AND e.code='market_transaction_fee' AND e.delta=-s.fee) <> CASE WHEN s.fee=0 THEN 0 ELSE 1 END OR
  (SELECT coalesce(sum(-e.delta::numeric),0) FROM exchange.wallet_entries e WHERE e.operation_id=s.operation_id AND e.owner_id=s.payer_id AND e.code='market_purchase') <>
  ((SELECT coalesce(sum(e.delta::numeric),0) FROM exchange.wallet_entries e WHERE e.operation_id=s.operation_id AND e.owner_id=s.payee_id AND e.code IN ('market_sale','market_transaction_fee')) + s.fee))
 UNION ALL
 SELECT 1 FROM exchange.listings l WHERE
  (l.state='sold' AND (l.settlement_id IS NOT NULL OR (SELECT count(*) FROM exchange.settlements s WHERE s.kind='listing' AND s.subject_id=l.id AND s.state='committed') <> 1)) OR
  (l.state='active' AND EXISTS (SELECT 1 FROM exchange.settlements s WHERE s.kind='listing' AND s.subject_id=l.id AND s.state='committed')) OR
  (l.state='pending' AND NOT EXISTS (SELECT 1 FROM exchange.settlements s WHERE s.operation_id=l.settlement_id AND s.kind='listing' AND s.subject_id=l.id AND s.state='pending'))
) invalid
"@ $database
        if ($bad.Trim() -ne '0') { throw 'Restored exchange consistency check failed.' }
    }
    @{host='127.0.0.1';port=$port;database=$database;username=$serviceRole;password=$servicePassword} |
        ConvertTo-Json | Set-Content (Join-Path $InstanceRoot 'connection.json')
    Write-Output "$Store restore verified: $InstanceRoot"
} catch {
    $errorMessage = $_.Exception.Message.Replace($adminPassword,'[REDACTED]').Replace($servicePassword,'[REDACTED]')
    if (Test-Path $markerPath) { try { Remove-Owned } catch { $errorMessage += ' Cleanup failed: ' + $_.Exception.Message } }
    throw $errorMessage
} finally { $env:PGPASSWORD = $oldPassword }
