[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot, [ValidateRange(5,30)][int]$UndecidedDeadlineSeconds = 15)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowedRoot = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/Persistence/AuthorityDurable'))
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($allowedRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $EvidenceRoot)) { throw 'Supply a fresh child of Artifacts/Persistence/AuthorityDurable.' }
$pins = Get-Content -LiteralPath (Join-Path $repo 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
$dotnet = Join-Path $repo 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
$pgBin = Join-Path $repo 'Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
$project = Join-Path $repo 'BackendServices/fixtures/AuthorityDurable/AuthorityDurable.csproj'
$ddl = Join-Path $repo 'BackendServices/fixtures/AuthorityDurable/Schema.sql'
$buildRoot = Join-Path $EvidenceRoot 'build'
$dll = Join-Path $buildRoot 'bin/AuthorityDurable/release/AuthorityDurable.dll'
foreach ($path in @($dotnet,$project,$ddl,(Join-Path $pgBin 'postgres.exe'),(Join-Path $pgBin 'initdb.exe'),(Join-Path $pgBin 'pg_ctl.exe'),(Join-Path $pgBin 'psql.exe'))) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file missing: $path" } }
if ((Get-FileHash -LiteralPath $dotnet -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.sdk.executableSha256 -or (Get-FileHash -LiteralPath (Join-Path $pgBin 'postgres.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.postgres.executableSha256) { throw 'Pinned toolchain identity mismatch; use Prepare-H2Toolchains.' }
$fixtureId = [guid]::NewGuid().ToString()
$tokenId = ([guid]$fixtureId).ToString('N')
$privateRoot = Join-Path $allowedRoot ('.private-' + $tokenId)
$dataDir = Join-Path $privateRoot 'data'
$ownerFile = Join-Path $privateRoot 'owner.json'
$controlRoot = Join-Path $privateRoot 'control'
$utf8 = [Text.UTF8Encoding]::new($false)
$children = [Collections.Generic.List[Diagnostics.Process]]::new()
$secrets = @{}
$secrets.fixture_admin = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
$owners = @('coordinator','material','terrain')
foreach ($owner in $owners) { foreach ($prefix in @('mig','svc','ro')) { $secrets["${prefix}_ad_$owner"] = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32)) } }
$started = $false; $startAttempted = $false; $removed = $false; $systemId = ''; $failure = ''; $port = 0
$driver = $null; $restartEvents = [Collections.Generic.List[object]]::new(); $outageEvents = [Collections.Generic.List[object]]::new(); $roleResults = [Collections.Generic.List[object]]::new()
function Write-Json([string]$Path,$Value) {
    $temporaryPath = $Path + '.tmp-' + [guid]::NewGuid().ToString('N')
    try { [IO.File]::WriteAllText($temporaryPath,($Value | ConvertTo-Json -Depth 20),$utf8); [IO.File]::Move($temporaryPath,$Path,$true) }
    finally { if (Test-Path -LiteralPath $temporaryPath) { Remove-Item -LiteralPath $temporaryPath } }
}
function Redact([string]$Value) { foreach ($secret in $secrets.Values) { $Value = $Value.Replace($secret,'[REDACTED]') }; return $Value }
function Start-Owned([string]$Exe,[string[]]$Arguments,[hashtable]$Environment = @{}) {
    $info = [Diagnostics.ProcessStartInfo]::new(); $info.FileName = $Exe; $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true; $info.WorkingDirectory = Join-Path $repo 'BackendServices'
    foreach ($arg in $Arguments) { $info.ArgumentList.Add($arg) }
    foreach ($key in @($info.Environment.Keys)) { if ($key -match '^(PG|AD_|H2MIG_)') { [void]$info.Environment.Remove($key) } }
    $info.Environment['DOTNET_CLI_TELEMETRY_OPTOUT'] = '1'; $info.Environment['DOTNET_GENERATE_ASPNET_CERTIFICATE'] = 'false'
    foreach ($key in $Environment.Keys) { $info.Environment[$key] = $Environment[$key] }
    $child = [Diagnostics.Process]::new(); $child.StartInfo = $info; [void]$child.Start(); $children.Add($child); return $child
}
function Finish-Owned([Diagnostics.Process]$Child,[int]$Seconds = 60) {
    $stdout = $Child.StandardOutput.ReadToEndAsync(); $stderr = $Child.StandardError.ReadToEndAsync()
    if (-not $Child.WaitForExit($Seconds * 1000)) { $Child.Kill($true); $Child.WaitForExit(); throw 'Owned child exceeded finite timeout.' }
    $outText = if ($stdout.Wait(1000)) { $stdout.Result } else { '[server inherited pipe; see private log]' }
    $errText = if ($stderr.Wait(1000)) { $stderr.Result } else { '[server inherited pipe; see private log]' }
    return @{ exit = $Child.ExitCode; stdout = (Redact $outText).Trim(); stderr = (Redact $errText).Trim() }
}
function Invoke-Owned([string]$Exe,[string[]]$Arguments,[hashtable]$Environment = @{}) { return Finish-Owned (Start-Owned $Exe $Arguments $Environment) }
function Require($Result,[string]$What) { if ($Result.exit -ne 0) { throw "$What failed: $($Result.stderr) $($Result.stdout)" }; return $Result.stdout }
function Sql([string]$Query,[string]$Database = 'postgres',[string]$Role = 'fixture_admin') {
    $queryFile = Join-Path $privateRoot ('sql-' + [guid]::NewGuid().ToString('N') + '.sql')
    [IO.File]::WriteAllText($queryFile,$Query,$utf8)
    try { return Invoke-Owned (Join-Path $pgBin 'psql.exe') @('-X','-q','-A','-t','-h','127.0.0.1','-p',"$port",'-U',$Role,'-d',$Database,'-v','ON_ERROR_STOP=1','-f',$queryFile) @{ PGPASSWORD = $secrets[$Role]; PGCONNECT_TIMEOUT = '5' } }
    finally { Remove-Item -LiteralPath $queryFile -Force }
}
function Assert-OwnedServer {
    $owner = Get-Content -LiteralPath $ownerFile -Raw | ConvertFrom-Json
    $absolutePrivate = [IO.Path]::GetFullPath($privateRoot); $absoluteData = [IO.Path]::GetFullPath($dataDir)
    if ($absolutePrivate -ne (Join-Path $allowedRoot ('.private-' + $tokenId)) -or $absoluteData -ne (Join-Path $absolutePrivate 'data') -or $owner.fixtureId -ne $fixtureId -or $owner.dataDirectory -ne $absoluteData -or $owner.port -ne $port) { throw 'Owned path/marker mismatch; preserve resources.' }
    foreach ($path in @($allowedRoot,$absolutePrivate,$absoluteData)) { if ((Test-Path -LiteralPath $path) -and ((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Refusing reparse point in owned path.' } }
    $pidFile = Join-Path $dataDir 'postmaster.pid'
    if (Test-Path -LiteralPath $pidFile) {
        $lines = @(Get-Content -LiteralPath $pidFile -TotalCount 4)
        if ($lines.Count -lt 4 -or $lines[0] -notmatch '^\d+$' -or [IO.Path]::GetFullPath($lines[1]) -ne $absoluteData -or [int]$lines[3] -ne $port) { throw 'Owned postmaster identity mismatch.' }
        $server = Get-CimInstance Win32_Process -Filter "ProcessId=$($lines[0])"
        if ($server -and ([IO.Path]::GetFullPath($server.ExecutablePath) -ne [IO.Path]::GetFullPath((Join-Path $pgBin 'postgres.exe')) -or $server.CommandLine.Replace('/','\') -notlike "*$absoluteData*")) { throw 'Owned postmaster executable/data mismatch.' }
    }
}
function Start-Cluster {
    $script:startAttempted = $true
    [void](Require (Invoke-Owned (Join-Path $pgBin 'pg_ctl.exe') @('-D',$dataDir,'-l',(Join-Path $privateRoot 'postgres.log'),'-w','-t','30','start')) 'owned cluster start')
    $script:started = $true; Assert-OwnedServer
}
try {
    [void](New-Item -ItemType Directory -Path $EvidenceRoot)
    [void](New-Item -ItemType Directory -Path $privateRoot)
    # Restrict the private run tree before writing any credentials; no machine trust store or ACL is changed.
    $account = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    $acl = Invoke-Owned 'icacls.exe' @($privateRoot,'/inheritance:r','/grant:r',"${account}:(OI)(CI)F")
    [void](Require $acl 'private directory ACL')
    [void](New-Item -ItemType Directory -Path $controlRoot)
    Write-Json $ownerFile @{ fixtureId=$fixtureId; dataDirectory=$dataDir; privateDirectory=$privateRoot; port=$port }
    $restore = Invoke-Owned $dotnet @('restore',$project,'--locked-mode','--packages',(Join-Path $repo 'Artifacts/Persistence/Toolchains/nuget'),'--artifacts-path',$buildRoot,'--verbosity','quiet')
    Write-Json (Join-Path $EvidenceRoot 'restore.json') $restore; [void](Require $restore 'locked restore')
    $build = Invoke-Owned $dotnet @('build',$project,'--no-restore','--configuration','Release','--artifacts-path',$buildRoot,'--verbosity','quiet')
    Write-Json (Join-Path $EvidenceRoot 'build.json') $build; [void](Require $build 'focused build')
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0); $listener.Start(); $port = $listener.LocalEndpoint.Port; $listener.Stop()
    if ($port -lt 1024) { throw 'Expected ephemeral unprivileged port.' }
    Write-Json $ownerFile @{ fixtureId=$fixtureId; dataDirectory=$dataDir; privateDirectory=$privateRoot; port=$port }
    $passwordFile = Join-Path $privateRoot 'admin-password'
    [IO.File]::WriteAllText($passwordFile,$secrets.fixture_admin,$utf8)
    [void](Require (Invoke-Owned (Join-Path $pgBin 'initdb.exe') @('-D',$dataDir,'-U','fixture_admin','--pwfile',$passwordFile,'--auth','scram-sha-256','--encoding','UTF8','--no-locale')) 'initdb')
    Remove-Item -LiteralPath $passwordFile -Force
    [IO.File]::AppendAllText((Join-Path $dataDir 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$port`nmax_connections=32`nshared_buffers='16MB'`nwork_mem='4MB'`nsynchronous_commit=on`nfsync=on`nfull_page_writes=on`nlog_statement='none'`nlog_min_error_statement='panic'`n",$utf8)
    $hba = [Collections.Generic.List[string]]::new(); $hba.Add('host all fixture_admin 127.0.0.1/32 scram-sha-256')
    foreach ($owner in $owners) { foreach ($prefix in @('mig','svc','ro')) { $hba.Add("host ad_${tokenId}_$owner ${prefix}_ad_$owner 127.0.0.1/32 scram-sha-256") } }
    $hba.Add('host all all 127.0.0.1/32 reject'); $hba.Add('host all all ::1/128 reject')
    [IO.File]::WriteAllLines((Join-Path $dataDir 'pg_hba.conf'),$hba,$utf8)
    Start-Cluster
    $systemId = (Require (Sql 'SELECT system_identifier::text FROM pg_control_system();') 'actual cluster identity').Trim()
    $ddlText = [IO.File]::ReadAllText($ddl); $config = @{ fixtureId=$fixtureId; systemIdentifier=$systemId; controlRoot=$controlRoot; allowedRoot=$allowedRoot; undecidedDeadlineSeconds=$UndecidedDeadlineSeconds; dotnet=$dotnet; ddlSha256=(Get-FileHash -LiteralPath $ddl -Algorithm SHA256).Hash.ToLowerInvariant() }
    foreach ($owner in $owners) {
        $database = "ad_${tokenId}_$owner"; $ownRole = "own_ad_$owner"; $migRole = "mig_ad_$owner"; $svcRole = "svc_ad_$owner"; $roRole = "ro_ad_$owner"
        $rolesSql = "CREATE ROLE $ownRole NOLOGIN; CREATE ROLE $migRole LOGIN NOINHERIT PASSWORD '$($secrets[$migRole])'; GRANT $ownRole TO $migRole; CREATE ROLE $svcRole LOGIN PASSWORD '$($secrets[$svcRole])'; CREATE ROLE $roRole LOGIN PASSWORD '$($secrets[$roRole])'; CREATE DATABASE $database;"
        [void](Require (Sql $rolesSql) 'create fixture roles/database')
        [void](Require (Sql "REVOKE ALL ON DATABASE $database FROM PUBLIC; GRANT CONNECT ON DATABASE $database TO $migRole,$svcRole,$roRole; REVOKE ALL ON SCHEMA public FROM PUBLIC; CREATE SCHEMA fixture AUTHORIZATION $ownRole;" $database) 'database isolation')
        [void](Require (Sql ("SET ROLE $ownRole;`n" + $ddlText) $database $migRole) 'apply fixture DDL as migration owner')
        $grants = @"
INSERT INTO fixture.identity(fixture_id) VALUES ('$fixtureId');
GRANT USAGE ON SCHEMA fixture TO $svcRole,$roRole;
GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA fixture TO $svcRole;
GRANT USAGE,SELECT ON ALL SEQUENCES IN SCHEMA fixture TO $svcRole;
GRANT SELECT ON ALL TABLES IN SCHEMA fixture TO $roRole;
REVOKE INSERT,UPDATE,DELETE ON fixture.identity FROM $svcRole;
CREATE SCHEMA fixture_control AUTHORIZATION fixture_admin;
CREATE FUNCTION fixture_control.system_identifier() RETURNS text LANGUAGE sql SECURITY DEFINER SET search_path=pg_catalog AS 'SELECT system_identifier::text FROM pg_catalog.pg_control_system()';
REVOKE ALL ON SCHEMA fixture_control FROM PUBLIC;
REVOKE ALL ON FUNCTION fixture_control.system_identifier() FROM PUBLIC;
GRANT USAGE ON SCHEMA fixture_control TO $svcRole,$roRole;
GRANT EXECUTE ON FUNCTION fixture_control.system_identifier() TO $svcRole,$roRole;
"@
        [void](Require (Sql $grants $database) 'grant restricted runtime/read roles')
        $config[$owner] = "Host=127.0.0.1;Port=$port;Database=$database;Username=$svcRole;Password=$($secrets[$svcRole]);Pooling=true;Maximum Pool Size=64;Minimum Pool Size=8;Timeout=5;Command Timeout=10;Application Name=h2-authority-$owner;Include Error Detail=false"
        $config["${owner}ReadOnly"] = "Host=127.0.0.1;Port=$port;Database=$database;Username=$roRole;Password=$($secrets[$roRole]);Pooling=true;Maximum Pool Size=64;Minimum Pool Size=8;Timeout=5;Command Timeout=10;Include Error Detail=false"
        foreach ($check in @(@('runtime DDL denied',$svcRole,'CREATE TABLE fixture.forbidden(id int);'),@('identity marker protected',$svcRole,"UPDATE fixture.identity SET fixture_id='$fixtureId';"),@('readonly DML denied',$roRole,"DELETE FROM fixture.identity;"),@('runtime cannot assume owner',$svcRole,"SET ROLE $ownRole;"))) {
            $denial = Sql $check[2] $database $check[1]
            $pass = $denial.exit -ne 0 -and $denial.stderr -match 'permission denied|must be able to SET ROLE'
            $roleResults.Add(@{ owner=$owner; name=$check[0]; passed=$pass; observed=$denial })
            if (-not $pass) { throw "Role denial failed: $owner $($check[0])" }
        }
        $otherDatabase = "ad_${tokenId}_" + $(if ($owner -eq 'terrain') { 'coordinator' } else { 'terrain' })
        $denial = Sql 'SELECT 1;' $otherDatabase $svcRole
        $pass = $denial.exit -ne 0 -and $denial.stderr -match 'pg_hba.conf rejects connection|permission denied for database'
        $roleResults.Add(@{ owner=$owner; name='cross-owner database denied'; passed=$pass; observed=$denial })
        if (-not $pass) { throw 'Cross-owner database connection unexpectedly allowed.' }
    }
    Write-Json (Join-Path $EvidenceRoot 'roles.json') $roleResults
    $privateConfig = Join-Path $privateRoot 'driver-config.json'; Write-Json $privateConfig $config
    $sources = @{}; Get-ChildItem -LiteralPath (Split-Path $project) -File | Where-Object Extension -in @('.cs','.csproj','.sql','.json') | ForEach-Object { $sources[$_.Name] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    $native = @{}; foreach ($name in @('postgres.exe','pg_ctl.exe','initdb.exe','psql.exe')) { $native[$name] = (Get-FileHash -LiteralPath (Join-Path $pgBin $name) -Algorithm SHA256).Hash.ToLowerInvariant() }
    $settings = Require (Sql "SELECT json_build_object('version',version(),'fsync',current_setting('fsync'),'synchronous_commit',current_setting('synchronous_commit'),'full_page_writes',current_setting('full_page_writes'))::text;") 'durability settings'
    Write-Json (Join-Path $EvidenceRoot 'environment.json') @{ fixtureId=$fixtureId; systemIdentifier=$systemId; host='127.0.0.1'; port=$port; databases=@($owners | ForEach-Object { "ad_${tokenId}_$_" }); sourceCommit=(git -C $repo rev-parse HEAD); sourceHashes=$sources; binarySha256=(Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant(); sdkSha256=$pins.sdk.executableSha256; nativeHashes=$native; settings=($settings | ConvertFrom-Json); limits=@{ maxConnections=32; sharedBuffersMiB=16; workMemMiB=4; driverSeconds=600; restartSeconds=30 }; authorization='trusted synthetic lookup stub'; externalFence='synthetic proof stub'; stage='S' }
    $driver = Start-Owned $dotnet @($dll,'driver',$privateConfig,(Join-Path $EvidenceRoot 'results.json'))
    $driverOut = $driver.StandardOutput.ReadToEndAsync(); $driverErr = $driver.StandardError.ReadToEndAsync(); $deadline = [DateTime]::UtcNow.AddSeconds(600); $handled = @{}; $handledOutages = @{}
    while (-not $driver.HasExited) {
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Driver exceeded 600-second bounded run.' }
        $requestPath = Join-Path $controlRoot 'restart.request'
        if (Test-Path -LiteralPath $requestPath) {
            try { $request = Get-Content -LiteralPath $requestPath -Raw | ConvertFrom-Json } catch { Start-Sleep -Milliseconds 30; continue }
            if ($request.fixtureId -ne $fixtureId -or $request.requestId -notmatch '^[A-Za-z0-9_-]{1,64}$') { throw 'Restart control identity mismatch.' }
            if (-not $handled.ContainsKey($request.requestId)) {
                if ($handled.Count -ge 2) { throw 'Fixture exceeded bounded PostgreSQL restart count.' }
                Assert-OwnedServer
                [void](Require (Invoke-Owned (Join-Path $pgBin 'pg_ctl.exe') @('-D',$dataDir,'-m','immediate','-w','-t','30','stop')) 'owned immediate stop')
                $started = $false; Start-Cluster
                $afterId = (Require (Sql 'SELECT system_identifier::text FROM pg_control_system();') 'restarted cluster identity').Trim()
                if ($afterId -ne $systemId) { throw 'Cluster identity changed during restart.' }
                $handled[$request.requestId] = $true
                $entry = @{ fixtureId=$fixtureId; requestId=$request.requestId; mode='immediate'; systemIdentifier=$afterId; completedUtc=[DateTime]::UtcNow.ToString('o') }
                $restartEvents.Add($entry); Write-Json (Join-Path $controlRoot 'restart.complete') $entry
            }
        }
        $outagePath = Join-Path $controlRoot 'owner-outage.request'
        if (Test-Path -LiteralPath $outagePath) {
            try { $request = Get-Content -LiteralPath $outagePath -Raw | ConvertFrom-Json } catch { Start-Sleep -Milliseconds 30; continue }
            if ($request.fixtureId -ne $fixtureId -or $request.requestId -notmatch '^[A-Za-z0-9_-]{1,64}$' -or $request.owner -notin @('material','terrain') -or $request.offline -isnot [bool]) { throw 'Owner outage control identity mismatch.' }
            if (-not $handledOutages.ContainsKey($request.requestId)) {
                if ($handledOutages.Count -ge 4) { throw 'Fixture exceeded bounded owner-outage transition count.' }
                Assert-OwnedServer
                $outageDatabase = "ad_${tokenId}_$($request.owner)"
                $allowConnections = if ($request.offline) { 'false' } else { 'true' }
                [void](Require (Sql "ALTER DATABASE $outageDatabase ALLOW_CONNECTIONS $allowConnections;") 'owned database admission transition')
                if ($request.offline) { [void](Require (Sql "SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname='$outageDatabase' AND backend_type='client backend' AND pid<>pg_backend_pid();") 'owned database backend termination') }
                $handledOutages[$request.requestId] = $true
                $entry = @{ fixtureId=$fixtureId; requestId=$request.requestId; owner=$request.owner; offline=$request.offline; completedUtc=[DateTime]::UtcNow.ToString('o') }
                $outageEvents.Add($entry); Write-Json (Join-Path $controlRoot 'owner-outage.complete') $entry
            }
        }
        Start-Sleep -Milliseconds 50
    }
    Write-Json (Join-Path $EvidenceRoot 'driver-output.json') @{ exit=$driver.ExitCode; stdout=(Redact $driverOut.GetAwaiter().GetResult()); stderr=(Redact $driverErr.GetAwaiter().GetResult()) }
    if ($driver.ExitCode -ne 0) { throw 'Durable authority driver failed; inspect sanitized evidence.' }
    $result = Get-Content -LiteralPath (Join-Path $EvidenceRoot 'results.json') -Raw | ConvertFrom-Json
    if (-not $result.passed) { throw 'Driver did not report a passing applicable Stage S result.' }
}
catch { $failure = Redact ($_.Exception.Message + ' ' + $_.ScriptStackTrace) }
finally {
    foreach ($child in $children) { if (-not $child.HasExited) { $child.Kill($true); $child.WaitForExit() } }
    try {
        if (Test-Path -LiteralPath $ownerFile) {
            Assert-OwnedServer
            if ($startAttempted) {
                $status = Invoke-Owned (Join-Path $pgBin 'pg_ctl.exe') @('-D',$dataDir,'status')
                if ($status.exit -eq 0) { [void](Require (Invoke-Owned (Join-Path $pgBin 'pg_ctl.exe') @('-D',$dataDir,'-m','fast','-w','-t','30','stop')) 'owned final stop') }
                $status = Invoke-Owned (Join-Path $pgBin 'pg_ctl.exe') @('-D',$dataDir,'status')
                if ($status.exit -ne 3) { throw 'Cannot prove owned PostgreSQL stopped; preserve resources.' }
            }
            $started = $false
            # Resolved absolute targets and marker are verified immediately before this single-shell removal.
            Assert-OwnedServer
            if (@(Get-ChildItem -LiteralPath $privateRoot -Recurse -Attributes ReparsePoint -ErrorAction Stop).Count) { throw 'Owned tree contains a reparse point; preserve resources.' }
            Remove-Item -LiteralPath ([IO.Path]::GetFullPath($privateRoot)) -Recurse -Force
            $removed = -not (Test-Path -LiteralPath $privateRoot)
        }
    }
    catch { $failure += ' Cleanup: ' + (Redact $_.Exception.Message) }
    if (Test-Path -LiteralPath $EvidenceRoot) {
        Write-Json (Join-Path $EvidenceRoot 'roles.json') $roleResults
        Write-Json (Join-Path $EvidenceRoot 'restart-events.json') $restartEvents
        Write-Json (Join-Path $EvidenceRoot 'owner-outage-events.json') $outageEvents
        Write-Json (Join-Path $EvidenceRoot 'cleanup.json') @{ fixtureId=$fixtureId; stopped=(-not $started); privateStorageRemoved=$removed; remainingPrivateDirectory=$(if ($removed) { $null } else { $privateRoot }); completedUtc=[DateTime]::UtcNow.ToString('o') }
        Write-Json (Join-Path $EvidenceRoot 'harness.json') @{ passed=($failure -eq '' -and $removed); failure=$failure; stage='S'; roleAssertions=$roleResults.Count; restartCount=$restartEvents.Count }
    }
}
if ($failure -ne '' -or -not $removed) { Write-Output "Durable authority fixture failed: $failure Evidence: $EvidenceRoot"; exit 1 }
Write-Output "Durable authority Stage S passed; $($roleResults.Count) role assertions; $($restartEvents.Count) PostgreSQL restart(s); owned private storage removed. Evidence: $EvidenceRoot"
