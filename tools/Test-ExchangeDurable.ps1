[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/Persistence/ExchangeDurable'))
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $EvidenceRoot)) { throw 'Supply a fresh ExchangeDurable evidence child.' }
$pins = Get-Content (Join-Path $repo 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
$dotnet = Join-Path $repo 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
$pg = Join-Path $repo 'Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
$project = Join-Path $repo 'BackendServices/Exchange.Postgres.Tests/Exchange.Postgres.Tests.csproj'
$schema = Join-Path $repo 'BackendServices/Exchange.Postgres/Schema.sql'
$dll = Join-Path $EvidenceRoot 'private/build/Exchange.Postgres.Tests.dll'
$inventorySchema = Join-Path $repo 'BackendServices/InventoryLedger.Postgres/Schema.sql'
& (Join-Path $repo 'tools/Assert-InventoryOutboxMigration.ps1') -MigrationPath (Join-Path $repo 'BackendServices/InventoryLedger.Postgres/Schema.Outbox.sql') | Out-Null
$backupTool = Join-Path $repo 'tools/Backup-ServiceStore.ps1'; $restoreTool = Join-Path $repo 'tools/Restore-ServiceStore.ps1'
$backup = Join-Path $EvidenceRoot 'backup'; $restoredInstance = Join-Path $EvidenceRoot 'restored-instance'
foreach ($path in @($dotnet,$project,$schema,(Join-Path $pg 'postgres.exe'),(Join-Path $pg 'initdb.exe'),(Join-Path $pg 'pg_ctl.exe'),(Join-Path $pg 'psql.exe'))) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file missing: $path" } }
if ((Get-FileHash $dotnet -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.sdk.executableSha256 -or (Get-FileHash (Join-Path $pg 'postgres.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.postgres.executableSha256) { throw 'Pinned toolchain mismatch.' }
$run = [guid]::NewGuid().ToString('N'); $private = Join-Path $EvidenceRoot 'private'; $data = Join-Path $private 'data'
$rng = [Security.Cryptography.RandomNumberGenerator]::Create()
$bytes = [byte[]]::new(32); $rng.GetBytes($bytes); $adminPassword = [BitConverter]::ToString($bytes).Replace('-','')
$rng.GetBytes($bytes); $servicePassword = [BitConverter]::ToString($bytes).Replace('-',''); $rng.Dispose()
$rng = [Security.Cryptography.RandomNumberGenerator]::Create(); $rng.GetBytes($bytes); $ledgerPassword = [BitConverter]::ToString($bytes).Replace('-',''); $rng.Dispose()
$started = $false; $attempted = $false; $removed = $false; $failure = ''; $port = 0
function Invoke-Checked([string]$Executable,[string[]]$Arguments,[hashtable]$Environment = @{}) {
    $info = [Diagnostics.ProcessStartInfo]::new(); $info.FileName = $Executable; $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true
    $info.Arguments = (($Arguments | ForEach-Object { '"' + $_.Replace('"','\"') + '"' }) -join ' ')
    foreach ($key in @($info.EnvironmentVariables.Keys)) { if ($key -match '^PG') { $info.EnvironmentVariables.Remove($key) } }
    foreach ($key in $Environment.Keys) { $info.EnvironmentVariables[$key] = $Environment[$key] }
    $child = [Diagnostics.Process]::new(); $child.StartInfo = $info; [void]$child.Start()
    $out = $child.StandardOutput.ReadToEndAsync(); $err = $child.StandardError.ReadToEndAsync()
    if (-not $child.WaitForExit(90000)) { $child.Kill(); throw 'Owned child timeout.' }
    $stdout = if ($out.Wait(1000)) { $out.Result } else { '[server retained output pipe]' }
    $stderr = if ($err.Wait(1000)) { $err.Result } else { '[server retained error pipe]' }
    foreach ($secret in @($adminPassword,$servicePassword,$ledgerPassword)) { $stdout = $stdout.Replace($secret,'[REDACTED]'); $stderr = $stderr.Replace($secret,'[REDACTED]') }
    if ($child.ExitCode -ne 0) { throw "$Executable exited $($child.ExitCode): $stderr $stdout" }
    return $stdout.Trim()
}
function Sql([string]$Query,[string]$Database='postgres',[string]$Role='fixture_admin',[string]$Password=$adminPassword) {
    $file = Join-Path $private ('sql-' + [guid]::NewGuid().ToString('N') + '.sql')
    [IO.File]::WriteAllText($file,$Query)
    try { return Invoke-Checked (Join-Path $pg 'psql.exe') @('-X','-q','-A','-t','-h','127.0.0.1','-p',"$port",'-U',$Role,'-d',$Database,'-v','ON_ERROR_STOP=1','-f',$file) @{ PGPASSWORD=$Password; PGCONNECT_TIMEOUT='5' } }
    finally { Remove-Item -LiteralPath $file -Force }
}
function Assert-Owned {
    if ([IO.Path]::GetFullPath($private) -ne [IO.Path]::GetFullPath((Join-Path $EvidenceRoot 'private')) -or -not $EvidenceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Owned path mismatch.' }
    $marker = Get-Content (Join-Path $private 'owner.json') -Raw | ConvertFrom-Json
    if ($marker.run -ne $run -or $marker.data -ne $data -or $marker.port -ne $port) { throw 'Owned marker mismatch.' }
    foreach ($path in @($EvidenceRoot,$private,$data)) { if ((Test-Path $path) -and ((Get-Item $path).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Owned path is a reparse point.' } }
    $pidFile = Join-Path $data 'postmaster.pid'
    if (Test-Path $pidFile) {
        $lines = @(Get-Content $pidFile -TotalCount 4)
        if ($lines.Count -lt 4 -or [IO.Path]::GetFullPath($lines[1]) -ne $data -or [int]$lines[3] -ne $port) { throw 'Postmaster identity mismatch.' }
        $process = Get-CimInstance Win32_Process -Filter "ProcessId=$($lines[0])"
        if ($process -and ([IO.Path]::GetFullPath($process.ExecutablePath) -ne [IO.Path]::GetFullPath((Join-Path $pg 'postgres.exe')) -or $process.CommandLine.Replace('/','\') -notlike "*$data*")) { throw 'Postmaster process mismatch.' }
    }
}
function Cluster([string]$Action) {
    Assert-Owned
    if ($Action -eq 'start') { $script:attempted = $true; [void](Invoke-Checked (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-l',(Join-Path $private 'postgres.log'),'-w','-t','30','start')); $script:started = $true; Assert-Owned }
    else { [void](Invoke-Checked (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-m','immediate','-w','-t','30','stop')); $script:started = $false }
}
try {
    [void](New-Item -ItemType Directory -Path $private -Force)
    $account = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    [void](Invoke-Checked 'icacls.exe' @($private,'/inheritance:r','/grant:r',"${account}:(OI)(CI)F"))
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0); $listener.Start(); $port = $listener.LocalEndpoint.Port; $listener.Stop()
    @{run=$run;data=$data;port=$port} | ConvertTo-Json | Set-Content (Join-Path $private 'owner.json')
    $buildPath = (Join-Path $private 'build') + [IO.Path]::DirectorySeparatorChar
    [void](Invoke-Checked $dotnet @('restore',$project,'--locked-mode','--packages',(Join-Path $repo 'Artifacts/Persistence/Toolchains/nuget'),'--verbosity','quiet'))
    [void](Invoke-Checked $dotnet @('build',$project,'--no-restore','--configuration','Release','--verbosity','quiet',"-p:OutDir=$buildPath"))
    $pwfile = Join-Path $private 'admin-password'; [IO.File]::WriteAllText($pwfile,$adminPassword)
    [void](Invoke-Checked (Join-Path $pg 'initdb.exe') @('-D',$data,'-U','fixture_admin','--pwfile',$pwfile,'--auth','scram-sha-256','--encoding','UTF8','--no-locale'))
    Remove-Item -LiteralPath $pwfile -Force
    [IO.File]::AppendAllText((Join-Path $data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$port`nfsync=on`nsynchronous_commit=on`nfull_page_writes=on`n")
    [IO.File]::WriteAllLines((Join-Path $data 'pg_hba.conf'),@('host all fixture_admin 127.0.0.1/32 scram-sha-256',"host exchange_$run exchange_svc 127.0.0.1/32 scram-sha-256","host exchange_$run fixture_ledger 127.0.0.1/32 scram-sha-256",'host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    Cluster start
    [void](Sql "CREATE ROLE exchange_owner NOLOGIN; CREATE ROLE exchange_svc LOGIN PASSWORD '$servicePassword'; CREATE ROLE fixture_ledger LOGIN PASSWORD '$ledgerPassword'; CREATE DATABASE exchange_$run;")
    [void](Sql "REVOKE ALL ON DATABASE exchange_$run FROM PUBLIC; GRANT CONNECT ON DATABASE exchange_$run TO exchange_svc,fixture_ledger; GRANT CREATE ON DATABASE exchange_$run TO exchange_owner; REVOKE ALL ON SCHEMA public FROM PUBLIC; CREATE SCHEMA inventory AUTHORIZATION exchange_owner; CREATE SCHEMA exchange AUTHORIZATION exchange_owner;" "exchange_$run")
    [void](Sql ("SET ROLE exchange_owner;`n" + (Get-Content $inventorySchema -Raw) + "`n" + (Get-Content (Join-Path $repo 'BackendServices/InventoryLedger.Postgres/Schema.Outbox.sql') -Raw) + "`n" + (Get-Content $schema -Raw)) "exchange_$run")
    $migrationConnection = "Host=127.0.0.1;Port=$port;Database=exchange_$run;Username=fixture_admin;Password=$adminPassword;Pooling=false"
    & (Join-Path $repo 'tools/Invoke-SchemaMigrations.ps1') -Connection $migrationConnection -Schemas inventory,exchange -Role exchange_owner -Dotnet $dotnet -Nuget (Join-Path $repo 'Artifacts/Persistence/Toolchains/nuget') | Out-Null
    [void](Sql 'GRANT USAGE ON SCHEMA exchange TO exchange_svc; GRANT SELECT ON exchange.schema_version TO exchange_svc; GRANT SELECT,INSERT,UPDATE ON exchange.wallets,exchange.wallet_entries,exchange.operations,exchange.listings,exchange.settlements TO exchange_svc; GRANT USAGE ON SCHEMA inventory TO fixture_ledger; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA inventory TO fixture_ledger;' "exchange_$run")
    $connection = "Host=127.0.0.1;Port=$port;Database=exchange_$run;Username=exchange_svc;Password=$servicePassword;Pooling=true;Maximum Pool Size=64;Minimum Pool Size=8;Timeout=2;Command Timeout=5;Include Error Detail=false"
    $ledgerConnection = "Host=127.0.0.1;Port=$port;Database=exchange_$run;Username=fixture_ledger;Password=$ledgerPassword;Pooling=true;Maximum Pool Size=64;Minimum Pool Size=8;Timeout=2;Command Timeout=5;Include Error Detail=false"
    $results = [Collections.Generic.List[string]]::new()
    foreach ($phase in @('seed','restart','recover','tombstone','tombstone-recover','concurrent','outage')) {
        if ($phase -eq 'restart' -or $phase -eq 'recover' -or $phase -eq 'tombstone-recover' -or $phase -eq 'outage') { Cluster stop }
        if ($phase -eq 'restart' -or $phase -eq 'recover' -or $phase -eq 'tombstone-recover') { Cluster start }
        $results.Add((Invoke-Checked $dotnet @($dll,$phase) @{ EXCHANGE_CONNECTION=$connection; INVENTORY_CONNECTION=$ledgerConnection }))
    }
    Cluster start
    [void](Sql "INSERT INTO exchange.settlements(operation_id,fingerprint,kind,subject_id,amount,fee,deliveries,state,receipt,created_at) VALUES ('backup_pending','fixture','trade','trd_backup',0,0,'[]','pending','{}',clock_timestamp()-interval '2 hours')" "exchange_$run")
    try { [void](Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$backupTool,'-Store','Exchange','-OutputDirectory',$backup,'-HostName','127.0.0.1','-Port',"$port",'-Database',"exchange_$run",'-Username','exchange_svc') @{ PGPASSWORD=$servicePassword }); throw 'Aged pending settlement was accepted.' }
    catch { if ($_.Exception.Message -notmatch 'exceeds recovery window') { throw }; $results.Add('PASS aged pending settlement refused') }
    [void](Sql "UPDATE exchange.settlements SET created_at=clock_timestamp() WHERE operation_id='backup_pending'" "exchange_$run")
    $results.Add((Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$backupTool,'-Store','Exchange','-OutputDirectory',$backup,'-HostName','127.0.0.1','-Port',"$port",'-Database',"exchange_$run",'-Username','exchange_svc') @{ PGPASSWORD=$servicePassword }))
    $backupWallets = (Sql 'SELECT count(*),sum(balance) FROM exchange.wallets' "exchange_$run").Trim()
    [void](Sql "INSERT INTO exchange.wallets(owner_id,balance,revision) VALUES ('chr_after_backup',7,1); INSERT INTO exchange.wallet_entries(owner_id,sequence,operation_id,delta,flow_class,code) VALUES ('chr_after_backup',1,'after_backup',7,'faucet','gm_grant')" "exchange_$run")
    if ((Sql 'SELECT count(*),sum(balance) FROM exchange.wallets' "exchange_$run").Trim() -eq $backupWallets) { throw 'Post-backup write failed.' }
    $corrupt = Join-Path $EvidenceRoot 'corrupt-backup'; [void](New-Item -ItemType Directory -Path $corrupt)
    Copy-Item (Join-Path $backup 'manifest.json') $corrupt
    $corruptDump = Join-Path $corrupt 'exchange.dump'; Copy-Item (Join-Path $backup 'exchange.dump') $corruptDump
    $stream = [IO.File]::Open($corruptDump,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite)
    try { [void]$stream.Seek(64,[IO.SeekOrigin]::Begin); $byte=$stream.ReadByte(); [void]$stream.Seek(64,[IO.SeekOrigin]::Begin); $stream.WriteByte([byte]($byte -bxor 1)) } finally { $stream.Dispose() }
    try { [void](Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$restoreTool,'-Store','Exchange','-BackupDirectory',$corrupt,'-InstanceRoot',$restoredInstance)); throw 'Corrupt backup was accepted.' }
    catch { if ($_.Exception.Message -notmatch 'SHA256 mismatch') { throw }; $results.Add('PASS flipped-byte backup refused') }
    $results.Add((Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$restoreTool,'-Store','Exchange','-BackupDirectory',$backup,'-InstanceRoot',$restoredInstance)))
    $restored = Get-Content (Join-Path $restoredInstance 'connection.json') -Raw | ConvertFrom-Json
    $restoredWallets = (Invoke-Checked (Join-Path $pg 'psql.exe') @('-X','-A','-t','-h','127.0.0.1','-p',"$($restored.port)",'-U',$restored.username,'-d',$restored.database,'-c','SELECT count(*),sum(balance) FROM exchange.wallets') @{ PGPASSWORD=$restored.password }).Trim()
    if ($restoredWallets -ne $backupWallets) { throw 'Restored exchange wallets differ from backup point.' }
    $results.Add('PASS restored exchange tables equal backup point')
    [void](Sql 'UPDATE exchange.schema_version SET version=4' "exchange_$run")
    $results.Add((Invoke-Checked $dotnet @($dll,'newer-schema') @{ EXCHANGE_CONNECTION=$connection; INVENTORY_CONNECTION=$ledgerConnection }))
    $markerVersion = Sql 'SELECT version FROM exchange.schema_version WHERE singleton=true' "exchange_$run"
    if ($markerVersion.Trim() -ne '4') { throw 'Newer schema marker changed unexpectedly.' }
    try { [void](Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$backupTool,'-Store','Exchange','-OutputDirectory',(Join-Path $EvidenceRoot 'newer-backup'),'-HostName','127.0.0.1','-Port',"$port",'-Database',"exchange_$run",'-Username','exchange_svc') @{ PGPASSWORD=$servicePassword }); throw 'Newer source schema was accepted.' }
    catch { if ($_.Exception.Message -notmatch 'Unsupported source schema version') { throw }; $results.Add('PASS newer source schema refused') }
    $results | Set-Content (Join-Path $EvidenceRoot 'results.txt')
}
catch { $failure = $_.Exception.Message.Replace($adminPassword,'[REDACTED]').Replace($servicePassword,'[REDACTED]').Replace($ledgerPassword,'[REDACTED]') }
finally {
    try {
        if (Test-Path (Join-Path $restoredInstance 'owner.json')) { [void](Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$restoreTool,'-Store','Exchange','-InstanceRoot',$restoredInstance,'-Cleanup')) }
        if (Test-Path (Join-Path $private 'owner.json')) {
            Assert-Owned
            if ($attempted) {
                $status = & (Join-Path $pg 'pg_ctl.exe') -D $data status 2>$null
                if ($LASTEXITCODE -eq 0) { Cluster stop }
            }
            Assert-Owned
            if (@(Get-ChildItem -LiteralPath $private -Recurse -Attributes ReparsePoint).Count) { throw 'Owned tree contains a reparse point.' }
            Remove-Item -LiteralPath ([IO.Path]::GetFullPath($private)) -Recurse -Force
            $removed = -not (Test-Path $private)
        }
    } catch { $failure += ' Cleanup: ' + $_.Exception.Message }
    @{passed=($failure -eq '' -and $removed);failure=$failure;privateStorageRemoved=$removed;run=$run} | ConvertTo-Json | Set-Content (Join-Path $EvidenceRoot 'harness.json')
}
if ($failure -ne '' -or -not $removed) { throw "Exchange durable fixture failed: $failure Evidence: $EvidenceRoot" }
Write-Output "Exchange durable fixture passed; owned private storage removed. Evidence: $EvidenceRoot"
