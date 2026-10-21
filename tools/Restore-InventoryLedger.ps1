[CmdletBinding()]
param(
    [string]$BackupDirectory,
    [Parameter(Mandatory)][string]$InstanceRoot,
    [switch]$Cleanup
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/Persistence/InventoryLedgerDurable'))
$InstanceRoot = [IO.Path]::GetFullPath($InstanceRoot)
if (-not $InstanceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $InstanceRoot -eq $allowed) { throw 'Instance root must be an owned InventoryLedgerDurable child.' }
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
    if ($marker.data -ne $data -or $marker.tool -ne 'Restore-InventoryLedger.ps1') { throw 'Restore owner marker mismatch.' }
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
$dump = Join-Path $BackupDirectory 'inventory.dump'
if ($manifest.format -ne 1 -or $manifest.schemaVersion -ne 1 -or $manifest.dumpSha256 -notmatch '^[a-f0-9]{64}$' -or
    (Get-FileHash $dump -Algorithm SHA256).Hash.ToLowerInvariant() -cne $manifest.dumpSha256) { throw 'Backup manifest or SHA256 mismatch.' }
foreach ($name in @('schemaVersionRows','containers','entries','receipts')) { if ($manifest.rowCounts.$name -isnot [long] -and $manifest.rowCounts.$name -isnot [int]) { throw 'Invalid manifest row count.' } }
if ($manifest.rowCounts.schemaVersionRows -ne 1) { throw 'Invalid schema marker count.' }
if (Test-Path -LiteralPath $InstanceRoot) { throw 'Restore target must be new.' }
$rng = [Security.Cryptography.RandomNumberGenerator]::Create(); $bytes = [byte[]]::new(32)
$rng.GetBytes($bytes); $adminPassword = [BitConverter]::ToString($bytes).Replace('-','')
$rng.GetBytes($bytes); $servicePassword = [BitConverter]::ToString($bytes).Replace('-',''); $rng.Dispose()
$listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0); $listener.Start(); $port = $listener.LocalEndpoint.Port; $listener.Stop()
$database = 'ledger_restore_' + [guid]::NewGuid().ToString('N')
$oldPassword = $env:PGPASSWORD
try {
    [void](New-Item -ItemType Directory -Path $InstanceRoot)
    $account = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    & icacls.exe $InstanceRoot /inheritance:r /grant:r "${account}:(OI)(CI)F" *> $null
    if ($LASTEXITCODE -ne 0) { throw 'Could not restrict restore directory.' }
    @{tool='Restore-InventoryLedger.ps1';data=$data;port=$port} | ConvertTo-Json | Set-Content $markerPath
    $pwfile = Join-Path $InstanceRoot 'admin-password'; [IO.File]::WriteAllText($pwfile,$adminPassword)
    & (Join-Path $pg 'initdb.exe') -D $data -U restore_admin --pwfile=$pwfile --auth=scram-sha-256 --encoding=UTF8 --no-locale *> $null
    if ($LASTEXITCODE -ne 0) { throw 'initdb failed.' }
    Remove-Item $pwfile -Force
    [IO.File]::AppendAllText((Join-Path $data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$port`nfsync=on`nsynchronous_commit=on`n")
    [IO.File]::WriteAllLines((Join-Path $data 'pg_hba.conf'),@('host all restore_admin 127.0.0.1/32 scram-sha-256',"host $database ledger_svc 127.0.0.1/32 scram-sha-256",'host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    [void](Assert-Owned)
    if ((Invoke-Control @('-D',$data,'-l',(Join-Path $InstanceRoot 'postgres.log'),'-w','-t','30','start')) -ne 0) { throw 'Restore cluster failed to start.' }
    $env:PGPASSWORD = $adminPassword
    function Sql([string]$query,[string]$db='postgres') {
        $output = & (Join-Path $pg 'psql.exe') -X -A -t -v ON_ERROR_STOP=1 -h 127.0.0.1 -p $port -U restore_admin -d $db -c $query 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Restore SQL failed: $output" }
        return [string]$output
    }
    [void](Sql "CREATE ROLE ledger_owner NOLOGIN; CREATE ROLE ledger_svc LOGIN PASSWORD '$servicePassword';")
    [void](Sql "CREATE DATABASE $database OWNER ledger_owner")
    [void](Sql "REVOKE ALL ON DATABASE $database FROM PUBLIC; GRANT CONNECT ON DATABASE $database TO ledger_svc" $database)
    & (Join-Path $pg 'pg_restore.exe') --exit-on-error --no-owner --no-acl --role=ledger_owner -h 127.0.0.1 -p $port -U restore_admin -d $database $dump *> $null
    if ($LASTEXITCODE -ne 0) { throw 'pg_restore failed.' }
    [void](Sql 'GRANT USAGE ON SCHEMA inventory TO ledger_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA inventory TO ledger_svc' $database)
    $version = (Sql 'SELECT version FROM inventory.schema_version WHERE singleton=true' $database).Trim()
    if ($version -ne '1') { throw 'Restored schema version mismatch.' }
    $counts = (Sql 'SELECT (SELECT count(*) FROM inventory.schema_version), (SELECT count(*) FROM inventory.containers), (SELECT count(*) FROM inventory.entries), (SELECT count(*) FROM inventory.operation_receipts)' $database).Trim().Split('|')
    $expected = @($manifest.rowCounts.schemaVersionRows,$manifest.rowCounts.containers,$manifest.rowCounts.entries,$manifest.rowCounts.receipts)
    for ($i=0; $i -lt 4; $i++) { if ([long]$counts[$i] -ne [long]$expected[$i]) { throw 'Restored row counts mismatch.' } }
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
    @{host='127.0.0.1';port=$port;database=$database;username='ledger_svc';password=$servicePassword} |
        ConvertTo-Json | Set-Content (Join-Path $InstanceRoot 'connection.json')
    Write-Output "Inventory ledger restore verified: $InstanceRoot"
} catch {
    $errorMessage = $_.Exception.Message.Replace($adminPassword,'[REDACTED]').Replace($servicePassword,'[REDACTED]')
    if (Test-Path $markerPath) { try { Remove-Owned } catch { $errorMessage += ' Cleanup failed: ' + $_.Exception.Message } }
    throw $errorMessage
} finally { $env:PGPASSWORD = $oldPassword }
