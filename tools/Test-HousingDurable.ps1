[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/Persistence/HousingDurable'))
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $EvidenceRoot)) { throw 'Supply a fresh HousingDurable evidence child.' }
$pins = Get-Content (Join-Path $repo 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
$dotnet = Join-Path $repo 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
$pg = Join-Path $repo 'Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
$project = Join-Path $repo 'BackendServices/Housing.Postgres.Tests/Housing.Postgres.Tests.csproj'
$dll = Join-Path $EvidenceRoot 'private/build/Housing.Postgres.Tests.dll'
$backupTool = Join-Path $repo 'tools/Backup-ServiceStore.ps1'; $restoreTool = Join-Path $repo 'tools/Restore-ServiceStore.ps1'
$backup = Join-Path $EvidenceRoot 'backup'; $restoredInstance = Join-Path $EvidenceRoot 'restored-instance'
foreach ($path in @($dotnet,$project,(Join-Path $pg 'postgres.exe'),(Join-Path $pg 'initdb.exe'),(Join-Path $pg 'pg_ctl.exe'),(Join-Path $pg 'psql.exe'))) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file missing: $path" } }
if ((Get-FileHash $dotnet -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.sdk.executableSha256 -or (Get-FileHash (Join-Path $pg 'postgres.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.postgres.executableSha256) { throw 'Pinned toolchain mismatch.' }
$run = [guid]::NewGuid().ToString('N'); $private = Join-Path $EvidenceRoot 'private'; $data = Join-Path $private 'data'
$rng = [Security.Cryptography.RandomNumberGenerator]::Create(); $bytes = [byte[]]::new(32)
$rng.GetBytes($bytes); $adminPassword = [BitConverter]::ToString($bytes).Replace('-','')
$rng.GetBytes($bytes); $servicePassword = [BitConverter]::ToString($bytes).Replace('-',''); $rng.Dispose()
$port = 0; $started = $false; $attempted = $false; $removed = $false; $failure = ''
function Invoke-Checked([string]$Executable,[string[]]$Arguments,[hashtable]$Environment = @{}) {
    $info = [Diagnostics.ProcessStartInfo]::new(); $info.FileName = $Executable; $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true
    $info.Arguments = (($Arguments | ForEach-Object { '"' + $_.Replace('"','\"') + '"' }) -join ' ')
    foreach ($key in @($info.EnvironmentVariables.Keys)) { if ($key -match '^PG|^HOUSING_') { $info.EnvironmentVariables.Remove($key) } }
    foreach ($key in $Environment.Keys) { $info.EnvironmentVariables[$key] = $Environment[$key] }
    $child = [Diagnostics.Process]::new(); $child.StartInfo = $info; [void]$child.Start()
    $out = $child.StandardOutput.ReadToEndAsync(); $err = $child.StandardError.ReadToEndAsync()
    if (-not $child.WaitForExit(90000)) { $child.Kill(); throw 'Owned child timeout.' }
    $stdout = if ($out.Wait(1000)) { $out.Result } else { '[output pipe held]' }
    $stderr = if ($err.Wait(1000)) { $err.Result } else { '[error pipe held]' }
    foreach ($secret in @($adminPassword,$servicePassword)) { $stdout = $stdout.Replace($secret,'[REDACTED]'); $stderr = $stderr.Replace($secret,'[REDACTED]') }
    if ($child.ExitCode -ne 0) { throw "$Executable exited $($child.ExitCode): $stderr $stdout" }
    return $stdout.Trim()
}
function Sql([string]$Query,[string]$Database='postgres') {
    $file = Join-Path $private ('sql-' + [guid]::NewGuid().ToString('N') + '.sql')
    [IO.File]::WriteAllText($file,$Query)
    try { return Invoke-Checked (Join-Path $pg 'psql.exe') @('-X','-q','-A','-t','-h','127.0.0.1','-p',"$port",'-U','fixture_admin','-d',$Database,'-v','ON_ERROR_STOP=1','-f',$file) @{ PGPASSWORD=$adminPassword; PGCONNECT_TIMEOUT='5' } }
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
    [void](Invoke-Checked $dotnet @('restore',$project,'--locked-mode','--packages',(Join-Path $repo 'Artifacts/Persistence/Toolchains/nuget'),'--verbosity','quiet'))
    $buildPath = (Join-Path $private 'build') + [IO.Path]::DirectorySeparatorChar
    [void](Invoke-Checked $dotnet @('build',$project,'--no-restore','--configuration','Release','--verbosity','quiet',"-p:OutDir=$buildPath"))
    $pwfile = Join-Path $private 'admin-password'; [IO.File]::WriteAllText($pwfile,$adminPassword)
    [void](Invoke-Checked (Join-Path $pg 'initdb.exe') @('-D',$data,'-U','fixture_admin','--pwfile',$pwfile,'--auth','scram-sha-256','--encoding','UTF8','--no-locale'))
    Remove-Item -LiteralPath $pwfile -Force
    [IO.File]::AppendAllText((Join-Path $data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$port`nfsync=on`nsynchronous_commit=on`nfull_page_writes=on`n")
    [IO.File]::WriteAllLines((Join-Path $data 'pg_hba.conf'),@('host all fixture_admin 127.0.0.1/32 scram-sha-256',"host housing_$run housing_svc 127.0.0.1/32 scram-sha-256",'host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    Cluster start
    [void](Sql "CREATE ROLE housing_owner NOLOGIN; CREATE ROLE housing_svc LOGIN PASSWORD '$servicePassword'; CREATE DATABASE housing_$run;")
    [void](Sql "REVOKE ALL ON DATABASE housing_$run FROM PUBLIC; GRANT CONNECT ON DATABASE housing_$run TO housing_svc; GRANT CREATE ON DATABASE housing_$run TO housing_owner; REVOKE ALL ON SCHEMA public FROM PUBLIC; CREATE SCHEMA housing AUTHORIZATION housing_owner; CREATE SCHEMA inventory AUTHORIZATION housing_owner;" "housing_$run")
    [void](Sql ("SET ROLE housing_owner;`n" + (Get-Content (Join-Path $repo 'BackendServices/Housing.Postgres/Schema.sql') -Raw) + "`n" + (Get-Content (Join-Path $repo 'BackendServices/InventoryLedger.Postgres/Schema.sql') -Raw) + "`n" + (Get-Content (Join-Path $repo 'BackendServices/Housing.Postgres/Schema.Gameplay.sql') -Raw) + "`n" + (Get-Content (Join-Path $repo 'BackendServices/Housing.Postgres/Schema.Geometry.sql') -Raw)) "housing_$run")
    [void](Sql 'GRANT USAGE ON SCHEMA housing,inventory TO housing_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA housing TO housing_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA inventory TO housing_svc;' "housing_$run")
    $connection = "Host=127.0.0.1;Port=$port;Database=housing_$run;Username=housing_svc;Password=$servicePassword;Pooling=true;Maximum Pool Size=64;Minimum Pool Size=8;Timeout=2;Command Timeout=5;Include Error Detail=false"
    $results = [Collections.Generic.List[string]]::new()
    $results.Add((Invoke-Checked $dotnet @($dll,'seed') @{ HOUSING_CONNECTION=$connection }))
    Cluster stop; Cluster start
    $results.Add((Invoke-Checked $dotnet @($dll,'restart') @{ HOUSING_CONNECTION=$connection }))
    $results.Add((Invoke-Checked $dotnet @($dll,'concurrent') @{ HOUSING_CONNECTION=$connection }))
    Cluster stop
    $results.Add((Invoke-Checked $dotnet @($dll,'outage') @{ HOUSING_CONNECTION=$connection }))
    Cluster start
    [void](Sql "INSERT INTO housing.claim_geometry VALUES ('clm_test',0,0,40),('clm_time',100,100,40)" "housing_$run")
    [void](Sql "INSERT INTO housing.removals VALUES ('remove/window','fixture','one','Pending',4,1,5,NULL); UPDATE housing.placements SET created_at=clock_timestamp()-interval '2 hours' WHERE operation_id='one'" "housing_$run")
    try { [void](Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$backupTool,'-Store','Housing','-OutputDirectory',$backup,'-HostName','127.0.0.1','-Port',"$port",'-Database',"housing_$run",'-Username','housing_svc') @{ PGPASSWORD=$servicePassword }); throw 'Aged pending removal was accepted.' }
    catch { if ($_.Exception.Message -notmatch 'exceeds recovery window') { throw }; $results.Add('PASS aged pending removal refused') }
    [void](Sql "UPDATE housing.placements SET created_at=clock_timestamp() WHERE operation_id='one'" "housing_$run")
    # The contention fixture leaves one pending reservation to exercise the recovery window.
    if ((Sql "SELECT count(*) FROM housing.placements WHERE state='Pending' AND created_at IS NOT NULL" "housing_$run").Trim() -ne '1') { throw 'Expected one timestamped pending placement.' }
    [void](Sql "UPDATE housing.placements SET created_at=clock_timestamp()-interval '2 hours' WHERE state='Pending'" "housing_$run")
    try { [void](Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$backupTool,'-Store','Housing','-OutputDirectory',$backup,'-HostName','127.0.0.1','-Port',"$port",'-Database',"housing_$run",'-Username','housing_svc') @{ PGPASSWORD=$servicePassword }); throw 'Aged pending placement was accepted.' }
    catch { if ($_.Exception.Message -notmatch 'exceeds recovery window') { throw }; $results.Add('PASS aged pending placement refused') }
    [void](Sql "UPDATE housing.placements SET created_at=clock_timestamp()-interval '120 seconds' WHERE state='Pending'" "housing_$run")
    if ((Sql 'SELECT version FROM housing.schema_version WHERE singleton=true' "housing_$run").Trim() -ne '5') { throw 'Housing schema changed before backup.' }
    $results.Add((Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$backupTool,'-Store','Housing','-OutputDirectory',$backup,'-HostName','127.0.0.1','-Port',"$port",'-Database',"housing_$run",'-Username','housing_svc') @{ PGPASSWORD=$servicePassword }))
    $backupRevision = (Sql "SELECT revision FROM housing.claims WHERE id='clm_test'" "housing_$run").Trim()
    [void](Sql "UPDATE housing.claims SET revision=revision+1 WHERE id='clm_test'" "housing_$run")
    if ((Sql "SELECT revision FROM housing.claims WHERE id='clm_test'" "housing_$run").Trim() -ne "$([long]$backupRevision + 1)") { throw 'Post-backup write failed.' }
    $corrupt = Join-Path $EvidenceRoot 'corrupt-backup'; [void](New-Item -ItemType Directory -Path $corrupt)
    Copy-Item (Join-Path $backup 'manifest.json') $corrupt
    $corruptDump = Join-Path $corrupt 'housing.dump'; Copy-Item (Join-Path $backup 'housing.dump') $corruptDump
    $stream = [IO.File]::Open($corruptDump,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite)
    try { [void]$stream.Seek(64,[IO.SeekOrigin]::Begin); $byte=$stream.ReadByte(); [void]$stream.Seek(64,[IO.SeekOrigin]::Begin); $stream.WriteByte([byte]($byte -bxor 1)) } finally { $stream.Dispose() }
    try { [void](Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$restoreTool,'-Store','Housing','-BackupDirectory',$corrupt,'-InstanceRoot',$restoredInstance)); throw 'Corrupt backup was accepted.' }
    catch { if ($_.Exception.Message -notmatch 'SHA256 mismatch') { throw }; $results.Add('PASS flipped-byte backup refused') }
    try { [void](Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$restoreTool,'-Store','Housing','-BackupDirectory',$backup,'-InstanceRoot',$restoredInstance,'-PendingPlacementRecoveryWindowSeconds','60')); throw 'Aged restored placement was accepted.' }
    catch { if ($_.Exception.Message -notmatch 'Restored housing consistency check failed') { throw }; $results.Add('PASS aged restored placement refused') }
    $results.Add((Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$restoreTool,'-Store','Housing','-BackupDirectory',$backup,'-InstanceRoot',$restoredInstance)))
    $restored = Get-Content (Join-Path $restoredInstance 'connection.json') -Raw | ConvertFrom-Json
    $restoredRevision = (Invoke-Checked (Join-Path $pg 'psql.exe') @('-X','-A','-t','-h','127.0.0.1','-p',"$($restored.port)",'-U',$restored.username,'-d',$restored.database,'-c',"SELECT revision FROM housing.claims WHERE id='clm_test'") @{ PGPASSWORD=$restored.password }).Trim()
    if ($restoredRevision -ne $backupRevision) { throw 'Restored housing revision differs from backup point.' }
    $results.Add('PASS restored housing state equals backup point')
    [void](Sql 'UPDATE housing.schema_version SET version=6' "housing_$run")
    $results.Add((Invoke-Checked $dotnet @($dll,'newer') @{ HOUSING_CONNECTION=$connection }))
    try { [void](Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$backupTool,'-Store','Housing','-OutputDirectory',(Join-Path $EvidenceRoot 'newer-backup'),'-HostName','127.0.0.1','-Port',"$port",'-Database',"housing_$run",'-Username','housing_svc') @{ PGPASSWORD=$servicePassword }); throw 'Newer Housing schema backup was accepted.' }
    catch { if ($_.Exception.Message -notmatch 'Unsupported source schema version') { throw }; $results.Add('PASS newer Housing schema backup refused') }
    $results | Set-Content (Join-Path $EvidenceRoot 'results.txt')
}
catch { $failure = $_.Exception.Message.Replace($adminPassword,'[REDACTED]').Replace($servicePassword,'[REDACTED]') }
finally {
    try {
        if (Test-Path (Join-Path $restoredInstance 'owner.json')) { [void](Invoke-Checked 'pwsh.exe' @('-NoProfile','-File',$restoreTool,'-Store','Housing','-InstanceRoot',$restoredInstance,'-Cleanup')) }
        if (Test-Path (Join-Path $private 'owner.json')) {
            Assert-Owned
            if ($started -or $attempted) { try { [void](Invoke-Checked (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-m','immediate','-w','-t','30','stop')) } catch { if ($failure -eq '') { $failure = $_.Exception.Message } } }
            Assert-Owned
            Remove-Item -LiteralPath $private -Recurse -Force; $removed = $true
        }
    } catch { if ($failure -eq '') { $failure = $_.Exception.Message } }
    @{status=$(if ($failure -eq '') {'pass'} else {'fail'}); privateStorageRemoved=$removed; error=$failure} | ConvertTo-Json | Set-Content (Join-Path $EvidenceRoot 'harness.json')
}
if ($failure -ne '') { throw $failure }
Get-Content (Join-Path $EvidenceRoot 'results.txt')
