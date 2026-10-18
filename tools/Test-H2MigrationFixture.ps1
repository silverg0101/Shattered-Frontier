[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PostgresBin,
    [Parameter(Mandatory)][string]$Dotnet,
    [Parameter(Mandatory)][string]$RunnerDll,
    [Parameter(Mandatory)][string]$Manifest,
    [Parameter(Mandatory)][string]$EvidenceRoot,
    [string]$PartitionProxyDll = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowedRoot = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts\Persistence\C-MIG'))
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($allowedRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'EvidenceRoot must be a new child of Artifacts/Persistence/C-MIG.' }
if (Test-Path -LiteralPath $EvidenceRoot) { throw 'EvidenceRoot already exists; use a fresh run directory.' }
foreach ($path in @($Dotnet, $RunnerDll, $Manifest, (Join-Path $PostgresBin 'initdb.exe'), (Join-Path $PostgresBin 'pg_ctl.exe'), (Join-Path $PostgresBin 'psql.exe'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file missing: $path" }
}
$Dotnet = [IO.Path]::GetFullPath($Dotnet)
$RunnerDll = [IO.Path]::GetFullPath($RunnerDll)
$Manifest = [IO.Path]::GetFullPath($Manifest)
$fixtureId = [guid]::NewGuid()
$db = 'h2mig_' + $fixtureId.ToString('N')
$privateRoot = Join-Path $allowedRoot ('.private-' + $fixtureId.ToString('N'))
$dataDir = Join-Path $privateRoot 'data'
$ownerFile = Join-Path $privateRoot 'owner.json'
$started = $false
$startAttempted = $false
$cleaned = $false
$results = [Collections.Generic.List[object]]::new()
$secrets = @{}
foreach ($role in @('fixture_admin', 'mig_building', 'svc_building', 'ro_building')) { $secrets[$role] = [guid]::NewGuid().ToString('N') + [guid]::NewGuid().ToString('N') }
$processes = [Collections.Generic.List[Diagnostics.Process]]::new()
$utf8 = [Text.UTF8Encoding]::new($false)
function Write-Utf8([string]$Path, [string]$Value) { [IO.File]::WriteAllText($Path, $Value, $utf8) }
function Redact([string]$Value) { foreach ($secret in $secrets.Values) { $Value = $Value.Replace($secret, '[REDACTED]') }; return $Value }
function Quote-Arg([string]$Value) { if ($Value.Contains('"')) { throw 'Double quotes in process arguments are unsupported.' }; return '"' + $Value.TrimEnd('\') + '"' }
function Start-Tool([string]$Exe, [string[]]$Arguments, [hashtable]$Environment = @{}, [bool]$KeepInput = $false) {
    $psi = [Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $Exe
    $psi.Arguments = (($Arguments | ForEach-Object { Quote-Arg $_ }) -join ' ')
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.RedirectStandardInput = $KeepInput
    foreach ($key in $Environment.Keys) { $psi.EnvironmentVariables[$key] = $Environment[$key] }
    $p = [Diagnostics.Process]::new()
    $p.StartInfo = $psi
    [void]$p.Start()
    $processes.Add($p)
    return $p
}
function Finish-Tool([Diagnostics.Process]$Process, [int]$TimeoutSeconds = 90) {
    $stdout = $Process.StandardOutput.ReadToEndAsync()
    $stderr = $Process.StandardError.ReadToEndAsync()
    if (-not $Process.WaitForExit($TimeoutSeconds * 1000)) { $Process.Kill(); $Process.WaitForExit(); throw 'Owned fixture child exceeded its finite timeout.' }
    # Windows postgres may retain pg_ctl's inherited pipe handles. Child exit, not pipe EOF,
    # bounds tool completion; psql/runner output normally completes immediately.
    $stdoutText = if ($stdout.Wait(1000)) { $stdout.Result } else { '[owned child inherited stdout; see private server log]' }
    $stderrText = if ($stderr.Wait(1000)) { $stderr.Result } else { '[owned child inherited stderr; see private server log]' }
    return [pscustomobject]@{ exit = $Process.ExitCode; stdout = (Redact $stdoutText).Trim(); stderr = (Redact $stderrText).Trim() }
}
function Invoke-Tool([string]$Exe, [string[]]$Arguments, [hashtable]$Environment = @{}) { return Finish-Tool (Start-Tool $Exe $Arguments $Environment) }
function Require-Success($Result, [string]$Context) { if ($Result.exit -ne 0) { throw "$Context failed: $($Result.stderr) $($Result.stdout)" }; return $Result.stdout }
function Sql([string]$Query, [string]$Role = 'fixture_admin', [string]$Database = $db) {
    $inputPath = Join-Path $privateRoot ('query-' + [guid]::NewGuid().ToString('N') + '.sql')
    Write-Utf8 $inputPath $Query
    try { return Invoke-Tool (Join-Path $PostgresBin 'psql.exe') @('-X', '-q', '-A', '-t', '-h', '127.0.0.1', '-p', "$port", '-U', $Role, '-d', $Database, '-v', 'ON_ERROR_STOP=1', '-f', $inputPath) @{ PGPASSWORD = $secrets[$Role]; PGCONNECT_TIMEOUT = '5' } }
    finally { Remove-Item -LiteralPath $inputPath -Force }
}
function Query([string]$Statement) { return Require-Success (Sql $Statement) 'Fixture assertion SQL' }
function Record([string]$Case, [string]$Name, [string]$Expected, $Observed, [bool]$Pass) {
    $results.Add([pscustomobject]@{ case = $Case; name = $Name; status = $(if ($Pass) { 'passed' } else { 'failed' }); expected = $Expected; observed = $Observed })
}
function Unproven([string]$Case, [string]$Name, [string]$Reason) { $results.Add([pscustomobject]@{ case = $Case; name = $Name; status = 'unproven'; expected = 'Contract branch requires separate recorded execution'; observed = $Reason }) }
function Start-Runner([string]$Label, [string]$Mode = 'migrate', [string]$ManifestPath = $Manifest, [string]$HookDir = '', [string]$ExpectedId = $fixtureId.ToString(), [int]$ConnectionPort = 0, [string]$TargetDatabase = '', [string]$TargetHost = '127.0.0.1', [string]$ExpectedSystemIdentifier = '') {
    if (-not $ExpectedSystemIdentifier) { $ExpectedSystemIdentifier = $systemId }
    $argsForRunner = @($RunnerDll, $Mode, '--connection-env', 'H2MIG_CONNECTION_STRING', '--manifest', $ManifestPath, '--fixture-id', $ExpectedId, '--system-identifier', $ExpectedSystemIdentifier)
    if ($HookDir) { $argsForRunner += @('--hook-dir', $HookDir) }
    if ($ConnectionPort -eq 0) { $ConnectionPort = $port }
    if (-not $TargetDatabase) { $TargetDatabase = $db }
    $conn = "Host=$TargetHost;Port=$ConnectionPort;Database=$TargetDatabase;Username=mig_building;Password=$($secrets.mig_building);Pooling=false;Multiplexing=false;Timeout=5;Application Name=h2mig-$Label"
    return Start-Tool $Dotnet $argsForRunner @{ H2MIG_CONNECTION_STRING = $conn }
}
function Run-Runner([string]$Label, [string]$Mode = 'migrate', [string]$ManifestPath = $Manifest, [string]$ExpectedId = $fixtureId.ToString()) {
    $r = Finish-Tool (Start-Runner $Label $Mode $ManifestPath '' $ExpectedId)
    Write-Utf8 (Join-Path $EvidenceRoot ($Label + '.json')) ($r | ConvertTo-Json -Depth 10)
    return $r
}
function Reset-Building {
    [void](Query 'DROP SCHEMA building CASCADE; CREATE SCHEMA building AUTHORIZATION own_building; GRANT USAGE ON SCHEMA building TO svc_building,ro_building; ALTER DEFAULT PRIVILEGES FOR ROLE own_building IN SCHEMA building GRANT SELECT,INSERT,UPDATE,DELETE ON TABLES TO svc_building; ALTER DEFAULT PRIVILEGES FOR ROLE own_building IN SCHEMA building GRANT SELECT ON TABLES TO ro_building; ALTER DEFAULT PRIVILEGES FOR ROLE own_building IN SCHEMA building GRANT USAGE ON SEQUENCES TO svc_building;')
}
function Get-MigrationHistory { return Query 'SELECT coalesce(jsonb_agg(to_jsonb(h) ORDER BY id)::text,''[]'') FROM building.schema_migrations h;' }
function Barrier([string]$Hook, [string[]]$MoreHooks = @()) {
    $dir = Join-Path $EvidenceRoot ('barrier-' + [guid]::NewGuid().ToString('N'))
    [void](New-Item -ItemType Directory -Path $dir)
    foreach ($h in @($Hook) + $MoreHooks) { Write-Utf8 (Join-Path $dir ($h + '.arm')) '' }
    return $dir
}
function Wait-Hook([Diagnostics.Process]$Process, [string]$Dir, [string]$Hook) {
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    while (-not (Test-Path -LiteralPath (Join-Path $Dir ($Hook + '.ready')))) {
        if ($Process.HasExited) { $r = Finish-Tool $Process; throw "Runner exited before $Hook barrier: $($r | ConvertTo-Json -Compress)" }
        if ([DateTime]::UtcNow -gt $deadline) { throw "Runner did not reach $Hook barrier." }
        Start-Sleep -Milliseconds 50
    }
}
function Release-Hook([string]$Dir, [string]$Hook) { Write-Utf8 (Join-Path $Dir ($Hook + '.release')) '' }
function Wait-OwnedFile([Diagnostics.Process]$Process, [string]$Path) {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while (-not (Test-Path -LiteralPath $Path)) {
        if ($Process.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw "Owned fixture helper did not acknowledge $([IO.Path]::GetFileName($Path))." }
        Start-Sleep -Milliseconds 25
    }
}
function Hold-Sql([string]$Statement, [string]$Label) {
    $p = Start-Tool (Join-Path $PostgresBin 'psql.exe') @('-X','-q','-A','-t','-h','127.0.0.1','-p',"$port",'-U','fixture_admin','-d',$db,'-v','ON_ERROR_STOP=1') @{ PGPASSWORD = $secrets.fixture_admin; PGAPPNAME = "h2mig-blocker-$Label"; PGCONNECT_TIMEOUT = '5' } $true
    $p.StandardInput.WriteLine("BEGIN; $Statement SELECT 'h2mig-ready';")
    $p.StandardInput.Flush()
    $ready = $p.StandardOutput.ReadLineAsync()
    if (-not $ready.Wait(5000) -or $ready.Result -ne 'h2mig-ready') { throw "Fixture blocker $Label did not acknowledge its held transaction." }
    return $p
}
function Release-Sql([Diagnostics.Process]$Process) {
    $Process.StandardInput.WriteLine('ROLLBACK;')
    $Process.StandardInput.WriteLine('\q')
    $Process.StandardInput.Close()
    [void](Require-Success (Finish-Tool $Process) 'release fixture blocker')
}
function Runner-Pid([string]$Label) {
    $value = Query "SELECT pid FROM pg_stat_activity WHERE datname='$db' AND application_name='h2mig-$Label';"
    if ($value -notmatch '^\d+$') { throw 'Expected exactly one registered runner backend.' }
    return $value
}
try {
    [void](New-Item -ItemType Directory -Path $EvidenceRoot)
    [void](New-Item -ItemType Directory -Path $privateRoot)
    # Block inherited access before any credentials are written. Only this Windows identity has access.
    $acl = [Security.AccessControl.DirectorySecurity]::new()
    $acl.SetAccessRuleProtection($true, $false)
    $acl.SetOwner([Security.Principal.WindowsIdentity]::GetCurrent().User)
    $rule = [Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.WindowsIdentity]::GetCurrent().User, 'FullControl', 'ContainerInherit,ObjectInherit', 'None', 'Allow')
    $acl.AddAccessRule($rule)
    Set-Acl -LiteralPath $privateRoot -AclObject $acl
    Write-Utf8 $ownerFile (@{ id = $fixtureId.ToString(); data = $dataDir; evidence = $EvidenceRoot } | ConvertTo-Json)
    $probe = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
    $probe.Start(); $port = $probe.LocalEndpoint.Port; $probe.Stop()
    if ($port -lt 1024) { throw 'Expected an ephemeral unprivileged port.' }
    $pwfile = Join-Path $privateRoot 'init-password'
    Write-Utf8 $pwfile $secrets.fixture_admin
    [void](Require-Success (Invoke-Tool (Join-Path $PostgresBin 'initdb.exe') @('-D', $dataDir, '-U', 'fixture_admin', '--pwfile', $pwfile, '--auth', 'scram-sha-256', '--encoding', 'UTF8', '--no-locale')) 'initdb')
    Remove-Item -LiteralPath $pwfile -Force
    [IO.File]::AppendAllText((Join-Path $dataDir 'postgresql.conf'), "`nlisten_addresses='127.0.0.1'`nport=$port`nmax_connections=12`nshared_buffers='16MB'`nwork_mem='4MB'`nlog_statement='none'`nlog_min_error_statement='panic'`n", $utf8)
    $startAttempted = $true
    [void](Require-Success (Invoke-Tool (Join-Path $PostgresBin 'pg_ctl.exe') @('-D', $dataDir, '-l', (Join-Path $privateRoot 'postgres.log'), '-w', '-t', '30', 'start')) 'pg_ctl start')
    $started = $true
    [void](Require-Success (Sql "CREATE DATABASE $db;" 'fixture_admin' 'postgres') 'create isolated database')
    $bootstrap = Get-Content -LiteralPath (Join-Path $repo 'BackendServices\fixtures\postgres\bootstrap.sql') -Raw
    $inputSql = "\set migration_password '$($secrets.mig_building)'`n\set runtime_password '$($secrets.svc_building)'`n\set readonly_password '$($secrets.ro_building)'`n\set fixture_id '$fixtureId'`n" + $bootstrap
    [void](Require-Success (Sql $inputSql) 'bootstrap isolated roles')
    $systemId = Query 'SELECT system_identifier FROM fixture_control.identity;'
    Write-Utf8 (Join-Path $EvidenceRoot 'environment.json') (@{ fixture_id = $fixtureId.ToString(); database = $db; host = '127.0.0.1'; port = $port; data_directory = $dataDir; system_identifier = $systemId; postgres_version = (Query 'SELECT version();'); runner_sha256 = (Get-FileHash -LiteralPath $RunnerDll -Algorithm SHA256).Hash; manifest_sha256 = (Get-FileHash -LiteralPath $Manifest -Algorithm SHA256).Hash; started_utc = [DateTime]::UtcNow.ToString('o'); roles = @('own_building','mig_building','svc_building','ro_building','own_world'); limits = @{ max_connections = 12; shared_buffers = '16MB'; work_mem = '4MB' } } | ConvertTo-Json -Depth 8)

    $first = Run-Runner '01-empty'
    $history = Get-MigrationHistory
    $repeat = Run-Runner '01-repeat'
    Record 'C-MIG-01' 'empty/repeat' 'Both exit 0; identical complete history' @{ first = $first; repeat = $repeat; before = $history; after = (Get-MigrationHistory) } ($first.exit -eq 0 -and $repeat.exit -eq 0 -and $history -eq (Get-MigrationHistory))

    [void](Query "UPDATE fixture_control.identity SET id='$( [guid]::NewGuid() )';")
    $badMarker = Run-Runner '10-wrong-marker'
    [void](Query "UPDATE fixture_control.identity SET id='$fixtureId';")
    Record 'C-MIG-10' 'wrong marker' 'Refuse without history mutation' $badMarker ($badMarker.exit -ne 0 -and $history -eq (Get-MigrationHistory))
    [void](Query "UPDATE fixture_control.identity SET system_identifier='0';")
    $forgedSystem = Finish-Tool (Start-Runner -Label '10-forged-system-marker' -ExpectedSystemIdentifier '0')
    [void](Query "UPDATE fixture_control.identity SET system_identifier='$systemId';")
    Record 'C-MIG-10' 'marker cannot forge actual server identity' 'Matching forged marker/expected system identifier still refuses actual server mismatch' $forgedSystem ($forgedSystem.exit -ne 0 -and $history -eq (Get-MigrationHistory))
    $badDatabase = Finish-Tool (Start-Runner -Label '10-wrong-database' -TargetDatabase 'postgres')
    $badHost = Finish-Tool (Start-Runner -Label '10-wrong-host' -TargetHost '192.0.2.1')
    Record 'C-MIG-10' 'wrong database and nonloopback endpoint' 'Both refuse before any connection/mutation' @{ database = $badDatabase; endpoint = $badHost } ($badDatabase.exit -ne 0 -and $badDatabase.stdout -match 'database_identity_mismatch' -and $badHost.exit -ne 0 -and $badHost.stdout -match 'loopback_literal_required' -and $history -eq (Get-MigrationHistory))
    $driftDir = Join-Path $EvidenceRoot 'altered-migration-artifact'
    [void](New-Item -ItemType Directory -Path $driftDir)
    Copy-Item -LiteralPath $Manifest -Destination (Join-Path $driftDir 'manifest.json')
    $manifestDocument = Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json
    foreach ($migration in $manifestDocument.migrations) { Copy-Item -LiteralPath (Join-Path (Split-Path $Manifest) $migration.file) -Destination (Join-Path $driftDir $migration.file) }
    [IO.File]::AppendAllText((Join-Path $driftDir $manifestDocument.migrations[0].file), "`n-- altered raw bytes for fixture-only checksum refusal`n", $utf8)
    $byteDrift = Run-Runner '02-raw-file-byte-drift' 'migrate' (Join-Path $driftDir 'manifest.json')
    Record 'C-MIG-02' 'raw migration byte alteration' 'Refuse altered artifact before pending DDL; committed source untouched' $byteDrift ($byteDrift.exit -ne 0 -and $byteDrift.stdout -match 'file_checksum_drift' -and $history -eq (Get-MigrationHistory))
    Copy-Item -LiteralPath (Join-Path (Split-Path $Manifest) $manifestDocument.migrations[0].file) -Destination (Join-Path $driftDir $manifestDocument.migrations[0].file) -Force
    $manifestDocument.migrations = @($manifestDocument.migrations[1], $manifestDocument.migrations[0], $manifestDocument.migrations[2])
    Write-Utf8 (Join-Path $driftDir 'manifest.json') ($manifestDocument | ConvertTo-Json -Depth 12)
    $reordered = Run-Runner '02-manifest-reordered' 'migrate' (Join-Path $driftDir 'manifest.json')
    Record 'C-MIG-02' 'reordered manifest' 'Refuse out-of-order IDs before connection or DDL' $reordered ($reordered.exit -ne 0 -and $history -eq (Get-MigrationHistory))
    $check = Run-Runner '08-supported-check' 'check'
    Record 'C-MIG-08' 'current readiness' 'Current fixture artifact accepts current schema without migrations' $check ($check.exit -eq 0)
    [void](Query 'ALTER TABLE building.fixture_items DROP CONSTRAINT fixture_items_pkey;')
    $missingPk = Run-Runner '02-missing-primary-key' 'check'
    Record 'C-MIG-02' 'completed primary key drift' 'Readiness refuses missing required primary key' $missingPk ($missingPk.exit -ne 0)
    [void](Query 'ALTER TABLE building.fixture_items ADD CONSTRAINT fixture_items_pkey PRIMARY KEY(id);')
    [void](Query 'DROP INDEX building.fixture_items_label_idx; CREATE INDEX fixture_items_label_idx ON building.fixture_items(note);')
    $wrongIndex = Run-Runner '02-wrong-index' 'migrate'
    Record 'C-MIG-02' 'completed wrong index definition' 'Refuse same-name wrong column without repair' $wrongIndex ($wrongIndex.exit -ne 0 -and (Query "SELECT pg_get_indexdef('building.fixture_items_label_idx'::regclass);") -match '\(note\)')
    [void](Query 'DROP INDEX building.fixture_items_label_idx; CREATE INDEX fixture_items_label_idx ON building.fixture_items(label);')

    [void](Require-Success (Sql "INSERT INTO building.fixture_items(label) VALUES('old-writer');" 'svc_building') 'synthetic old writer')
    $oldRead = Sql "SELECT label FROM building.fixture_items WHERE label='old-writer';" 'svc_building'
    $newWrite = Sql "INSERT INTO building.fixture_items(label,note) VALUES('new-writer','expanded'); SELECT note FROM building.fixture_items WHERE label='new-writer';" 'svc_building'
    Record 'C-MIG-08' 'synthetic N-1/new read-write' 'Old projection and new nullable column operations both succeed' @{ old = $oldRead; new = $newWrite; artifact = (Get-FileHash -LiteralPath $RunnerDll).Hash; schema_history = (Get-MigrationHistory) } ($oldRead.exit -eq 0 -and $oldRead.stdout -match 'old-writer' -and $newWrite.exit -eq 0 -and $newWrite.stdout -match 'expanded')

    $privilegeCases = @(
        @('runtime DDL','svc_building','CREATE TABLE building.forbidden(id int);'),
        @('runtime history mutation','svc_building','DELETE FROM building.schema_migrations;'),
        @('runtime owner assumption','svc_building','SET ROLE own_building;'),
        @('readonly write','ro_building',"INSERT INTO building.fixture_items(label) VALUES('forbidden');"),
        @('readonly sequence','ro_building',"SELECT nextval('building.fixture_items_id_seq');")
    )
    foreach ($case in $privilegeCases) { $r = Sql $case[2] $case[1]; Record 'C-MIG-09' $case[0] 'Privilege denied' $r ($r.exit -ne 0 -and $r.stderr -match 'permission denied') }
    foreach ($role in @('mig_building','svc_building','ro_building')) {
        foreach ($q in @('SELECT * FROM world.sentinel;', "UPDATE world.sentinel SET value='forbidden';", 'CREATE TABLE world.forbidden(id int);')) {
            $r = Sql $q $role; Record 'C-MIG-09' "$role cross-domain $q" 'Privilege denied; sentinel unchanged' $r ($r.exit -ne 0 -and $r.stderr -match 'permission denied')
        }
    }
    $roRead = Sql 'SELECT count(*) FROM building.fixture_items; SELECT count(*) FROM building.schema_migrations;' 'ro_building'
    Record 'C-MIG-09' 'readonly permitted reads' 'Synthetic data and history reads succeed' $roRead ($roRead.exit -eq 0)
    [void](Query 'SET ROLE own_building; CREATE TABLE building.fixture_grants_probe(id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,label text NOT NULL); RESET ROLE;')
    $newRuntime = Sql "INSERT INTO building.fixture_grants_probe(label) VALUES('new-object'); SELECT label FROM building.fixture_grants_probe;" 'svc_building'
    $newReadonly = Sql 'SELECT label FROM building.fixture_grants_probe;' 'ro_building'
    $newDeny = Sql "INSERT INTO building.fixture_grants_probe(label) VALUES('forbidden');" 'ro_building'
    Record 'C-MIG-09' 'new object inherited grants' 'Runtime DML/sequence and readonly SELECT allowed; readonly INSERT denied' @{ runtime = $newRuntime; readonly = $newReadonly; denied = $newDeny } ($newRuntime.exit -eq 0 -and $newReadonly.exit -eq 0 -and $newDeny.exit -ne 0)
    $sentinel = Query 'SELECT value FROM world.sentinel WHERE id=1;'
    Record 'C-MIG-09' 'sentinel unchanged' 'unchanged' $sentinel ($sentinel -eq 'unchanged')

    # Mutate only disposable history, restoring each explicit predecessor checkpoint.
    [void](Query "ALTER TABLE building.schema_migrations DISABLE TRIGGER USER; UPDATE building.schema_migrations SET sha256=repeat('0',64) WHERE id=(SELECT min(id) FROM building.schema_migrations); ALTER TABLE building.schema_migrations ENABLE TRIGGER USER;")
    $driftBefore = Get-MigrationHistory; $drift = Run-Runner '02-checksum-drift'
    Record 'C-MIG-02' 'checksum drift' 'Refuse and preserve altered history' $drift ($drift.exit -ne 0 -and $driftBefore -eq (Get-MigrationHistory))
    # Return to the exact recorded checkpoint rather than blessing changed checksums.
    [void](Query "ALTER TABLE building.schema_migrations DISABLE TRIGGER USER; TRUNCATE building.schema_migrations; INSERT INTO building.schema_migrations SELECT * FROM jsonb_populate_recordset(NULL::building.schema_migrations, '$($history.Replace("'", "''"))'::jsonb); UPDATE building.schema_migrations SET id='9999_unknown' WHERE id=(SELECT max(id) FROM building.schema_migrations); ALTER TABLE building.schema_migrations ENABLE TRIGGER USER;")
    $unknownBefore = Get-MigrationHistory; $unknown = Run-Runner '02-unknown'
    Record 'C-MIG-02' 'unknown history ID' 'Refuse and preserve history' $unknown ($unknown.exit -ne 0 -and $unknownBefore -eq (Get-MigrationHistory))
    [void](Query "ALTER TABLE building.schema_migrations DISABLE TRIGGER USER; TRUNCATE building.schema_migrations; INSERT INTO building.schema_migrations SELECT * FROM jsonb_populate_recordset(NULL::building.schema_migrations, '$($history.Replace("'", "''"))'::jsonb); DELETE FROM building.schema_migrations WHERE id=(SELECT min(id) FROM building.schema_migrations); ALTER TABLE building.schema_migrations ENABLE TRIGGER USER;")
    $gapBefore = Get-MigrationHistory; $gap = Run-Runner '02-gap'
    Record 'C-MIG-02' 'applied prefix gap' 'Refuse and preserve history' $gap ($gap.exit -ne 0 -and $gapBefore -eq (Get-MigrationHistory))
    [void](Query "ALTER TABLE building.schema_migrations DISABLE TRIGGER USER; TRUNCATE building.schema_migrations; INSERT INTO building.schema_migrations SELECT * FROM jsonb_populate_recordset(NULL::building.schema_migrations, '$($history.Replace("'", "''"))'::jsonb); UPDATE building.schema_migrations SET result_version=99 WHERE id='0003'; ALTER TABLE building.schema_migrations ENABLE TRIGGER USER;")
    $unsupportedBefore = Get-MigrationHistory
    $unsupportedMigrate = Run-Runner '02-unsupported-source'
    $unsupportedCheck = Run-Runner '08-unsupported-readiness' 'check'
    Record 'C-MIG-02' 'unsupported source schema version' 'Refuse unsupported99 without mutation' $unsupportedMigrate ($unsupportedMigrate.exit -ne 0 -and $unsupportedBefore -eq (Get-MigrationHistory))
    Record 'C-MIG-08' 'unsupported schema readiness' 'Check refuses unsupported99 without automatic migration' @{ check = $unsupportedCheck; artifact = (Get-FileHash -LiteralPath $RunnerDll).Hash; schema = 99 } ($unsupportedCheck.exit -ne 0 -and $unsupportedBefore -eq (Get-MigrationHistory))

    foreach ($hook in @('transaction_before_commit','transaction_after_commit')) {
        Reset-Building
        $dir = Barrier $hook
        $p = Start-Runner "03-$hook" 'migrate' $Manifest $dir
        Wait-Hook $p $dir $hook
        $before = Query "SELECT to_regclass('building.fixture_items') IS NOT NULL;"
        # Kill exactly the registered runner's database backend, never a name-matched arbitrary session.
        $pidText = Query "SELECT pid FROM pg_stat_activity WHERE datname='$db' AND application_name='h2mig-03-$hook';"
        if ($pidText -notmatch '^\d+$') { throw 'Expected exactly one registered runner backend.' }
        [void](Query "SELECT pg_terminate_backend($pidText);")
        Release-Hook $dir $hook
        $interrupted = Finish-Tool $p
        $recovered = Run-Runner "03-retry-$hook"
        $expectedVisible = if ($hook -eq 'transaction_before_commit') { 'f' } else { 't' }
        Record 'C-MIG-03' $hook 'Before commit no fixture table; after commit table exists; restart converges once' @{ table_visible_at_barrier = $before; backend_pid = $pidText; interrupted = $interrupted; recovered = $recovered; history = (Get-MigrationHistory) } ($before -eq $expectedVisible -and $recovered.exit -eq 0)
    }
    foreach ($hook in @('session_locked','transaction_before_commit','index_running')) {
        Reset-Building
        $dir = Barrier $hook
        $p = Start-Runner "04-$hook" 'migrate' $Manifest $dir
        Wait-Hook $p $dir $hook
        $locks = Query "SELECT jsonb_agg(jsonb_build_object('pid',pid,'mode',mode,'granted',granted)) FROM pg_locks WHERE locktype='advisory';"
        $other = Run-Runner "04-competing-$hook"
        Release-Hook $dir $hook
        $completed = Finish-Tool $p
        Record 'C-MIG-04' $hook 'B exits busy 10 while A retains advisory lock; A completes' @{ other = $other; a = $completed; locks = $locks } ($other.exit -eq 10 -and $completed.exit -eq 0 -and $locks -match 'ExclusiveLock')
    }
    foreach ($hook in @('index_running','index_created')) {
        Reset-Building
        $dir = Barrier $hook
        $p = Start-Runner "06-$hook" 'migrate' $Manifest $dir
        Wait-Hook $p $dir $hook
        $catalog = Query "SELECT coalesce(jsonb_agg(jsonb_build_object('name',c.relname,'valid',i.indisvalid,'ready',i.indisready,'definition',pg_get_indexdef(c.oid)))::text,'[]') FROM pg_class c JOIN pg_index i ON i.indexrelid=c.oid JOIN pg_namespace n ON n.oid=c.relnamespace WHERE n.nspname='building' AND c.relname='fixture_items_label_idx';"
        $pidText = Query "SELECT pid FROM pg_stat_activity WHERE datname='$db' AND application_name='h2mig-06-$hook';"
        if ($pidText -notmatch '^\d+$') { throw 'Expected exactly one registered runner backend.' }
        [void](Query "SELECT pg_terminate_backend($pidText);")
        Release-Hook $dir $hook
        $interrupted = Finish-Tool $p
        $recovered = Run-Runner "06-retry-$hook"
        Record 'C-MIG-06' $hook 'Actual catalog captured; restart reaches one valid declared index' @{ catalog_at_crash = $catalog; interrupted = $interrupted; retry = $recovered; history = (Get-MigrationHistory) } ($recovered.exit -eq 0 -and (Query "SELECT count(*) FROM pg_index i JOIN pg_class c ON c.oid=i.indexrelid JOIN pg_namespace n ON n.oid=c.relnamespace WHERE n.nspname='building' AND c.relname='fixture_items_label_idx' AND i.indisvalid;") -eq '1')
    }
    Reset-Building
    $dir = Barrier 'index_running'
    $p = Start-Runner '07-lock-timeout' 'migrate' $Manifest $dir
    Wait-Hook $p $dir 'index_running'
    $blocker = Hold-Sql 'LOCK TABLE building.fixture_items IN ACCESS EXCLUSIVE MODE;' 'lock-timeout'
    Release-Hook $dir 'index_running'
    $timeout = Finish-Tool $p
    $timeoutState = Query "SELECT jsonb_build_object('index_absent',to_regclass('building.fixture_items_label_idx') IS NULL,'failed_attempt',(SELECT count(*) FROM building.schema_migration_attempts WHERE migration_id='0003' AND state='failed'),'errors',(SELECT jsonb_agg(jsonb_build_object('state',state,'sqlstate',sqlstate,'detail',detail)) FROM building.schema_migration_attempts WHERE migration_id='0003'))::text;"
    Release-Sql $blocker
    $timeoutRetry = Run-Runner '07-lock-timeout-retry'
    Record 'C-MIG-07' 'actual DDL lock timeout before index creation' 'Failure SQLSTATE55P03, durable failed attempt, absent index, then successful retry' @{ failed = $timeout; catalog = $timeoutState; retry = $timeoutRetry } ($timeout.exit -ne 0 -and $timeoutState -match '55P03' -and $timeoutState -match '"index_absent": true' -and $timeoutRetry.exit -eq 0)

    Reset-Building
    $dir = Barrier 'index_running'
    $p = Start-Runner '06-invalid-index' 'migrate' $Manifest $dir
    Wait-Hook $p $dir 'index_running'
    $blocker = Hold-Sql "INSERT INTO building.fixture_items(label) VALUES('uncommitted-blocker');" 'invalid-index'
    $backendId = Runner-Pid '06-invalid-index'
    Release-Hook $dir 'index_running'
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        $invalid = Query "SELECT count(*) FROM pg_index i JOIN pg_class c ON c.oid=i.indexrelid JOIN pg_namespace n ON n.oid=c.relnamespace WHERE n.nspname='building' AND c.relname='fixture_items_label_idx' AND NOT i.indisvalid;"
        if ($invalid -eq '1') { break }
        if ($p.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw 'Concurrent index did not produce the required actual invalid-index branch.' }
        Start-Sleep -Milliseconds 25
    } while ($true)
    $progress = Query "SELECT coalesce(jsonb_agg(to_jsonb(p))::text,'[]') FROM pg_stat_progress_create_index p WHERE pid=$backendId;"
    [void](Query "SELECT pg_cancel_backend($backendId);")
    $cancelled = Finish-Tool $p
    Release-Sql $blocker
    $invalidAfter = Query "SELECT indisvalid::text FROM pg_index WHERE indexrelid='building.fixture_items_label_idx'::regclass;"
    Record 'C-MIG-07' 'actual statement cancellation' 'Cancellation57014 preserves actual invalid concurrent index and failed attempt' @{ runner = $cancelled; progress = $progress; index_valid_after_cancel = $invalidAfter; attempts = (Query "SELECT jsonb_agg(to_jsonb(a)) FROM building.schema_migration_attempts a WHERE migration_id='0003';") } ($cancelled.exit -ne 0 -and $invalidAfter -eq 'false' -and $cancelled.stdout -match '57014')
    foreach ($cleanupHook in @('index_cleanup_pending','index_dropped')) {
        $dir = Barrier $cleanupHook
        $label = "06-$cleanupHook"
        $p = Start-Runner $label 'migrate' $Manifest $dir
        Wait-Hook $p $dir $cleanupHook
        $cleanupCatalog = Query "SELECT jsonb_build_object('index_exists',to_regclass('building.fixture_items_label_idx') IS NOT NULL,'attempts',(SELECT jsonb_agg(to_jsonb(a) ORDER BY event_seq) FROM building.schema_migration_attempts a WHERE migration_id='0003'))::text;"
        $backendId = Runner-Pid $label
        [void](Query "SELECT pg_terminate_backend($backendId);")
        Release-Hook $dir $cleanupHook
        $stopped = Finish-Tool $p
        Record 'C-MIG-06' $cleanupHook 'Owned invalid-index cleanup crash captured at named boundary' @{ catalog = $cleanupCatalog; interrupted = $stopped } ($stopped.exit -ne 0)
    }
    $invalidRetry = Run-Runner '06-invalid-index-final-retry'
    Record 'C-MIG-06' 'invalid index recovery converges' 'One valid index, one completed step, preserved failure attempts' @{ retry = $invalidRetry; history = (Get-MigrationHistory); valid_indexes = (Query "SELECT count(*) FROM pg_index WHERE indexrelid='building.fixture_items_label_idx'::regclass AND indisvalid;") } ($invalidRetry.exit -eq 0 -and (Query "SELECT count(*) FROM building.schema_migrations WHERE id='0003';") -eq '1')
    if ($PartitionProxyDll) {
        Reset-Building
        $proxyDir = Join-Path $EvidenceRoot 'partition-proxy'
        [void](New-Item -ItemType Directory -Path $proxyDir)
        $proxy = Start-Tool $Dotnet @($PartitionProxyDll, "$port", $proxyDir)
        Wait-OwnedFile $proxy (Join-Path $proxyDir 'ready.json')
        $proxyPort = (Get-Content -LiteralPath (Join-Path $proxyDir 'ready.json') -Raw | ConvertFrom-Json).port
        $dir = Barrier 'index_running'
        $p = Start-Runner -Label '05-partitioned-runner' -HookDir $dir -ConnectionPort $proxyPort
        Wait-Hook $p $dir 'index_running'
        $backendId = Runner-Pid '05-partitioned-runner'
        Write-Utf8 (Join-Path $proxyDir 'drop') ''
        Wait-OwnedFile $proxy (Join-Path $proxyDir 'partitioned')
        Release-Hook $dir 'index_running'
        $disconnected = Finish-Tool $p
        $heldLock = Query "SELECT count(*) FROM pg_locks WHERE pid=$backendId AND locktype='advisory' AND granted;"
        $competing = Run-Runner '05-competing-during-partition'
        Write-Utf8 (Join-Path $proxyDir 'stop') ''
        $proxyEnded = Finish-Tool $proxy
        $deadline = [DateTime]::UtcNow.AddSeconds(10)
        do {
            $remainingBackend = Query "SELECT count(*) FROM pg_stat_activity WHERE pid=$backendId;"
            if ($remainingBackend -eq '0') { break }
            if ([DateTime]::UtcNow -gt $deadline) { throw 'Retained partition backend did not close after owned proxy stop.' }
            Start-Sleep -Milliseconds 25
        } while ($true)
        $afterPartition = Run-Runner '05-after-partition'
        Record 'C-MIG-05' 'client disconnect with retained server session' 'A fails; backend retains lock; Bbusy until upstream is closed then Bsuccess' @{ backend_pid = $backendId; a = $disconnected; held_locks = $heldLock; b = $competing; proxy_exit = $proxyEnded.exit; after_close = $afterPartition } ($disconnected.exit -ne 0 -and [int]$heldLock -gt 0 -and $competing.exit -eq 10 -and $afterPartition.exit -eq 0)
    }
    else { Unproven 'C-MIG-05' 'client partition with retained backend' 'Supply -PartitionProxyDll to execute actual loopback partition branch.' }
    Write-Utf8 (Join-Path $EvidenceRoot 'catalog-final.json') (Query "SELECT jsonb_build_object('history',(SELECT jsonb_agg(to_jsonb(h) ORDER BY id) FROM building.schema_migrations h),'attempts',(SELECT jsonb_agg(to_jsonb(a) ORDER BY event_seq) FROM building.schema_migration_attempts a),'sentinel',(SELECT jsonb_agg(to_jsonb(s)) FROM world.sentinel s))::text;")
}
catch {
    $results.Add([pscustomobject]@{ case = 'HARNESS'; name = 'execution'; status = 'failed'; expected = 'Finite isolated execution'; observed = (Redact ($_.Exception.Message + ' ' + $_.ScriptStackTrace)) })
}
finally {
    foreach ($p in $processes) { if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit() } }
    try {
        $owner = Get-Content -LiteralPath $ownerFile -Raw | ConvertFrom-Json
        $resolvedPrivate = [IO.Path]::GetFullPath($privateRoot)
        $resolvedData = [IO.Path]::GetFullPath($dataDir)
        if ($resolvedPrivate -ne (Join-Path $allowedRoot ('.private-' + $fixtureId.ToString('N'))) -or $resolvedData -ne (Join-Path $resolvedPrivate 'data') -or $owner.id -ne $fixtureId.ToString() -or $owner.data -ne $resolvedData) { throw 'Cleanup owner/path mismatch; preserve resources.' }
        if ((Get-Item -LiteralPath $resolvedPrivate).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Cleanup refuses reparse point.' }
        if ($startAttempted) {
            $pidPath = Join-Path $resolvedData 'postmaster.pid'
            if (Test-Path -LiteralPath $pidPath) {
                $pidLines = Get-Content -LiteralPath $pidPath -TotalCount 4
                if ($pidLines.Count -lt 2 -or [IO.Path]::GetFullPath($pidLines[1]) -ne $resolvedData -or $pidLines[0] -notmatch '^\d+$') { throw 'Postmaster identity mismatch; preserve resources.' }
                $server = Get-CimInstance Win32_Process -Filter "ProcessId=$($pidLines[0])"
                if ($server -and ([IO.Path]::GetFullPath($server.ExecutablePath) -ne [IO.Path]::GetFullPath((Join-Path $PostgresBin 'postgres.exe')) -or $server.CommandLine.Replace('/','\') -notlike "*$resolvedData*")) { throw 'Postmaster executable/data mismatch; preserve resources.' }
                if ($server) { [void](Require-Success (Invoke-Tool (Join-Path $PostgresBin 'pg_ctl.exe') @('-D', $resolvedData, '-m', 'fast', '-w', '-t', '30', 'stop')) 'owned instance stop') }
                if (Get-CimInstance Win32_Process -Filter "ProcessId=$($pidLines[0])") { throw 'Postmaster still alive; preserve resources.' }
            }
            $status = Invoke-Tool (Join-Path $PostgresBin 'pg_ctl.exe') @('-D', $resolvedData, 'status')
            if ($status.exit -ne 3) { throw 'Cannot prove owned server stopped; preserve resources.' }
            $started = $false
        }
        # Both absolute targets and ownership marker are verified immediately before this bounded recursive removal.
        Remove-Item -LiteralPath $resolvedPrivate -Recurse -Force
        $cleaned = -not (Test-Path -LiteralPath $resolvedPrivate)
        Record 'C-MIG-10' 'owned cleanup' 'Owned cluster stopped and private storage removed; evidence retained' @{ removed = $cleaned; evidence = $EvidenceRoot } $cleaned
    }
    catch { Record 'C-MIG-10' 'owned cleanup' 'Safe owned teardown' (Redact $_.Exception.Message) $false }
    if (Test-Path -LiteralPath $EvidenceRoot) {
        Write-Utf8 (Join-Path $EvidenceRoot 'results.json') ($results | ConvertTo-Json -Depth 16)
        Write-Utf8 (Join-Path $EvidenceRoot 'cleanup.json') (@{ fixture_id = $fixtureId.ToString(); stopped = (-not $started); private_storage_removed = $cleaned; remaining_data_directory = $(if ($cleaned) { $null } else { $dataDir }); completed_utc = [DateTime]::UtcNow.ToString('o') } | ConvertTo-Json)
    }
}
$failed = @($results | Where-Object status -eq 'failed').Count
$unproven = @($results | Where-Object status -eq 'unproven').Count
Write-Output "C-MIG fixture: $failed failed assertions; $unproven unproven branches. Evidence: $EvidenceRoot"
if ($failed -gt 0) { exit 1 }
if ($unproven -gt 0) { exit 2 }
exit 0
