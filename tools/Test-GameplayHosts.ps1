[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot, [switch]$RecipesOnly, [switch]$AppearanceOnly, [switch]$GatheringOnly)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& (Join-Path $repo 'tools/Assert-InventoryOutboxMigration.ps1') -MigrationPath (Join-Path $repo 'BackendServices/InventoryLedger.Postgres/Schema.Outbox.sql') | Out-Null
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/Persistence/GameplayHosts'))
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $EvidenceRoot)) { throw 'Supply a fresh GameplayHosts evidence child.' }
$pins = Get-Content (Join-Path $repo 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
$dotnet = Join-Path $repo 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
$pg = Join-Path $repo 'Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
if (-not (Test-Path -LiteralPath $dotnet)) { $dotnet = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe' }
if (-not (Test-Path -LiteralPath (Join-Path $pg 'postgres.exe'))) { $pg = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin' }
$nuget = Join-Path $repo 'Artifacts/Persistence/Toolchains/nuget'
if (-not (Test-Path -LiteralPath $nuget)) { $nuget = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/nuget' }
foreach ($path in @( $dotnet,(Join-Path $pg 'postgres.exe'),(Join-Path $pg 'initdb.exe'),(Join-Path $pg 'pg_ctl.exe'),(Join-Path $pg 'psql.exe'))) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file missing: $path" } }
if ((Get-FileHash $dotnet -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.sdk.executableSha256 -or (Get-FileHash (Join-Path $pg 'postgres.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.postgres.executableSha256) { throw 'Pinned toolchain mismatch.' }
$run = [guid]::NewGuid().ToString('N'); $private = Join-Path $EvidenceRoot 'private'; $data = Join-Path $private 'data'
$rng = [Security.Cryptography.RandomNumberGenerator]::Create(); $bytes = [byte[]]::new(32)
$rng.GetBytes($bytes); $adminPassword = [BitConverter]::ToString($bytes).Replace('-','')
$rng.GetBytes($bytes); $servicePassword = [BitConverter]::ToString($bytes).Replace('-',''); $rng.Dispose()
$port = 0; $started = $false; $attempted = $false; $removed = $false; $failure = ''; $token = ''; $hostProcess = $null; $hostProcess2 = $null; $ledgerHost = $null; $currentStep = 'setup'
function Invoke-Checked([string]$Executable,[string[]]$Arguments,[hashtable]$Environment = @{}) {
    $info = [Diagnostics.ProcessStartInfo]::new(); $info.FileName = $Executable; $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true
    $info.Arguments = (($Arguments | ForEach-Object { '"' + $_.Replace('"','\"') + '"' }) -join ' ')
    foreach ($key in @($info.EnvironmentVariables.Keys)) { if ($key -match '^PG|^GAMEPLAY_') { $info.EnvironmentVariables.Remove($key) } }
    foreach ($key in $Environment.Keys) { $info.EnvironmentVariables[$key] = $Environment[$key] }
    $child = [Diagnostics.Process]::new(); $child.StartInfo = $info; [void]$child.Start()
    $out = $child.StandardOutput.ReadToEndAsync(); $err = $child.StandardError.ReadToEndAsync()
    if (-not $child.WaitForExit(180000)) {
        $child.Kill(); $child.WaitForExit(5000) | Out-Null
        $partial = if ($out.Wait(1000)) { $out.Result } else { '' }
        $partial = $partial.Replace($adminPassword,'[REDACTED]').Replace($servicePassword,'[REDACTED]')
        throw "Owned child timeout: $partial"
    }
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
    $env:REHEARSAL_RECEIPT_LEDGER = Join-Path $EvidenceRoot 'receipts.jsonl'
    $account = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    [void](Invoke-Checked 'icacls.exe' @($private,'/inheritance:r','/grant:r',"${account}:(OI)(CI)F"))
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0); $listener.Start(); $port = $listener.LocalEndpoint.Port; $listener.Stop()
    @{run=$run;data=$data;port=$port} | ConvertTo-Json | Set-Content (Join-Path $private 'owner.json')
    $projects = $(if ($RecipesOnly -or $AppearanceOnly -or $GatheringOnly) { @('Progression') } else { @('Rewards','Progression','Housing') }) |
        ForEach-Object { Join-Path $repo "BackendServices/Gameplay.Host.Tests/$_/$_.Host.Tests.csproj" }
    $buildOutput = (Join-Path $private 'build') + [IO.Path]::DirectorySeparatorChar
    foreach ($project in $projects) {
        $currentStep = "restore $project"
        [void](Invoke-Checked $dotnet @('restore',$project,'--locked-mode','--packages',$nuget,'--verbosity','quiet'))
        $currentStep = "build $project"
        [void](Invoke-Checked $dotnet @('build',$project,'--no-restore','--configuration','Release','--verbosity','quiet',"-p:OutDir=$buildOutput"))
    }
    $pwfile = Join-Path $private 'admin-password'; [IO.File]::WriteAllText($pwfile,$adminPassword)
    [void](Invoke-Checked (Join-Path $pg 'initdb.exe') @('-D',$data,'-U','fixture_admin','--pwfile',$pwfile,'--auth','scram-sha-256','--encoding','UTF8','--no-locale'))
    Remove-Item -LiteralPath $pwfile -Force
    [IO.File]::AppendAllText((Join-Path $data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$port`nfsync=on`nsynchronous_commit=on`nfull_page_writes=on`n")
    [IO.File]::WriteAllLines((Join-Path $data 'pg_hba.conf'),@('host all fixture_admin 127.0.0.1/32 scram-sha-256',"host gameplay_$run gameplay_svc 127.0.0.1/32 scram-sha-256",'host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    Cluster start
    [void](Sql "CREATE ROLE gameplay_owner NOLOGIN; CREATE ROLE gameplay_svc LOGIN PASSWORD '$servicePassword'; CREATE DATABASE gameplay_$run;")
    [void](Sql "REVOKE ALL ON DATABASE gameplay_$run FROM PUBLIC; GRANT CONNECT ON DATABASE gameplay_$run TO gameplay_svc; GRANT CREATE ON DATABASE gameplay_$run TO gameplay_owner; REVOKE ALL ON SCHEMA public FROM PUBLIC;" "gameplay_$run")
    $schemas = @('InventoryLedger.Postgres','CharacterState.Postgres','Housing.Postgres','Exchange.Postgres') | ForEach-Object { Get-Content (Join-Path $repo "BackendServices/$_/Schema.sql") -Raw }
    $schemas += Get-Content (Join-Path $repo 'BackendServices/InventoryLedger.Postgres/Schema.Outbox.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/InventoryLedger.Postgres/Schema.GatheringNodes.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/InventoryLedger.Postgres/Schema.DefeatRewards.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/Housing.Postgres/Schema.Gameplay.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/Housing.Postgres/Schema.Geometry.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/Housing.Postgres/Schema.Construction.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.Position.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.Recipes.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.Appearance.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.GatheringSkills.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.Death.sql') -Raw
    [void](Sql ("SET ROLE gameplay_owner;`n" + ($schemas -join "`n")) "gameplay_$run")
    $migrationConnection = "Host=127.0.0.1;Port=$port;Database=gameplay_$run;Username=fixture_admin;Password=$adminPassword;Pooling=false"
    & (Join-Path $repo 'tools/Invoke-SchemaMigrations.ps1') -Connection $migrationConnection -Schemas inventory,character_state,housing,exchange -Role gameplay_owner -Dotnet $dotnet -Nuget $nuget | Out-Null
    [void](Sql 'GRANT USAGE ON SCHEMA inventory,character_state,housing,exchange TO gameplay_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA inventory TO gameplay_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA character_state TO gameplay_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA housing TO gameplay_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA exchange TO gameplay_svc;' "gameplay_$run")
    $connection = "Host=127.0.0.1;Port=$port;Database=gameplay_$run;Username=gameplay_svc;Password=$servicePassword;Pooling=true;Maximum Pool Size=64;Minimum Pool Size=8;Timeout=2;Command Timeout=5;Include Error Detail=false"
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0); $listener.Start(); $hostPort = $listener.LocalEndpoint.Port; $listener.Stop()
    $rng = [Security.Cryptography.RandomNumberGenerator]::Create(); $rng.GetBytes($bytes); $token = [BitConverter]::ToString($bytes).Replace('-',''); $rng.Dispose()
    $env:GAMEPLAY_HOST_PORT = "$hostPort"
    $env:GAMEPLAY_LEDGER_CONNECTION = $connection
    $env:GAMEPLAY_CHARACTER_CONNECTION = $connection
    $env:GAMEPLAY_HOUSING_CONNECTION = $connection
    $env:GAMEPLAY_EXCHANGE_CONNECTION = $connection
    $fixtureContent = Join-Path $private 'content'
    Copy-Item -LiteralPath (Join-Path $repo 'content') -Destination $fixtureContent -Recurse
    $env:GAMEPLAY_CONTENT_ROOT = $fixtureContent
    $env:SF_GAMEPLAY_TOKEN = $token
    $env:GAMEPLAY_FIXTURE_MODE = 'gathering'
    $env:GAMEPLAY_FIXTURE_GATHERING_RESPAWN_SECONDS = '30'
    $env:SF_GAMEPLAY_FAULT_AFTER_SCROLL_CONSUME = 'recipe/scroll-crash'
    $hostDll = Join-Path $buildOutput 'Gameplay.Host.dll'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host.out') -RedirectStandardError (Join-Path $private 'host.err')
    $ready = $false
    for ($i=0; $i -lt 100; $i++) {
        if ($hostProcess.HasExited) { throw 'Gameplay host exited before ready.' }
        if ((Test-Path (Join-Path $private 'host.out')) -and (Get-Content (Join-Path $private 'host.out') -Raw) -match 'GAMEPLAY_HOST_READY') { $ready = $true; break }
        Start-Sleep -Milliseconds 100
    }
    if (-not $ready) { throw 'Gameplay host readiness timeout.' }
    $results = [Collections.Generic.List[string]]::new()
    $journeyDll = Join-Path $buildOutput 'Progression.Host.Tests.dll'
    if (-not ($RecipesOnly -or $AppearanceOnly)) {
        $currentStep = 'gathering old-host capability'
        $results.Add((Invoke-Checked $dotnet @($journeyDll,'gathering-old-host') @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
        }))
        $currentStep = 'gathering growth wire'
        $results.Add((Invoke-Checked $dotnet @($journeyDll,'gathering') @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
        }))
    }
    if ($GatheringOnly) {
        $currentStep = 'gathering Postgres round trip'
        $stateCount = Sql "SELECT count(*) FROM inventory.gathering_node_state" "gameplay_$run"
        $receiptCount = Sql "SELECT count(*) FROM inventory.gathering_harvest_receipts" "gameplay_$run"
        $grantCount = Sql "SELECT count(*) FROM inventory.operation_receipts WHERE operation_id LIKE 'gathering/%'" "gameplay_$run"
        $quantity = Sql "SELECT quantity FROM inventory.entries WHERE container_id='ctn_gathering' AND definition_id='item/iron_ore'" "gameplay_$run"
        if ($stateCount -ne '1' -or $receiptCount -ne '9' -or $grantCount -ne '7' -or $quantity -ne '7') { throw "Gathering state/receipt/grant round trip failed: $stateCount/$receiptCount/$grantCount/$quantity" }
        $results.Add('PASS gathering Postgres node state, nine receipts, seven one-time grants')
        Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null
        Cluster stop; Cluster start
        $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'gather-restart.out') -RedirectStandardError (Join-Path $private 'gather-restart.err')
        for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Gathering restart host exited.' }; if ((Test-Path (Join-Path $private 'gather-restart.out')) -and (Get-Content (Join-Path $private 'gather-restart.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
        $currentStep = 'gathering restart replay'
        $results.Add((Invoke-Checked $dotnet @($journeyDll,'gathering-replay') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
        Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null
        $env:SF_GAMEPLAY_FAULT_AFTER_GATHER_INTENT = 'gathering/fault/1'
        $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'gather-fault.out') -RedirectStandardError (Join-Path $private 'gather-fault.err')
        for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Gathering fault host exited before ready.' }; if ((Test-Path (Join-Path $private 'gather-fault.out')) -and (Get-Content (Join-Path $private 'gather-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
        $currentStep = 'gathering crash after intent'
        $results.Add((Invoke-Checked $dotnet @($journeyDll,'gathering-fault') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
        if (-not $hostProcess.WaitForExit(10000)) { throw 'Gathering injected fault did not stop host.' }
        $pending = Sql "SELECT count(*) FROM inventory.gathering_harvest_receipts WHERE operation_id='gathering/fault/1' AND delivered_at IS NULL" "gameplay_$run"
        $premature = Sql "SELECT count(*) FROM inventory.operation_receipts WHERE operation_id='gathering/fault/1'" "gameplay_$run"
        if ($pending -ne '1' -or $premature -ne '0') { throw "Gathering crash boundary differs: $pending/$premature" }
        Remove-Item Env:SF_GAMEPLAY_FAULT_AFTER_GATHER_INTENT
        Cluster stop; Cluster start
        $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'gather-recover.out') -RedirectStandardError (Join-Path $private 'gather-recover.err')
        for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Gathering recovery host exited.' }; if ((Test-Path (Join-Path $private 'gather-recover.out')) -and (Get-Content (Join-Path $private 'gather-recover.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
        $currentStep = 'gathering crash replay'
        $results.Add((Invoke-Checked $dotnet @($journeyDll,'gathering-fault-replay') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
        $delivered = Sql "SELECT count(*) FROM inventory.gathering_harvest_receipts WHERE operation_id='gathering/fault/1' AND delivered_at IS NOT NULL" "gameplay_$run"
        $outbox = Sql "SELECT count(*) FROM inventory.receipt_outbox WHERE source_operation_id='gathering/fault/1'" "gameplay_$run"
        if ($delivered -ne '1' -or $outbox -ne '1') { throw "Gathering relay/outbox differs: $delivered/$outbox" }
        $results.Add('PASS gathering intent replay entered INT-06 outbox exactly once')
        for ($i=0; $i -lt 300; $i++) { if ((Sql "SELECT count(*) FROM inventory.receipt_outbox WHERE source_operation_id LIKE 'gathering/%' AND delivered_at IS NOT NULL" "gameplay_$run") -eq '8') { break }; Start-Sleep -Milliseconds 100 }
        $followups = Sql "SELECT count(*) FROM character_state.source_receipts WHERE operation_id LIKE 'gathering/%'" "gameplay_$run"
        $parked = Sql "SELECT count(*) FROM inventory.receipt_outbox WHERE source_operation_id LIKE 'gathering/%' AND parked_at IS NOT NULL" "gameplay_$run"
        if ($followups -ne '8' -or $parked -ne '0') {
            $missing = Sql "SELECT string_agg(source_operation_id || ':' || coalesce(last_error_code,'none') || ':' || attempts, ',') FROM inventory.receipt_outbox WHERE source_operation_id LIKE 'gathering/%' AND delivered_at IS NULL" "gameplay_$run"
            throw "Gathering INT-06 follow-up replay differs: $followups/$parked pending=$missing"
        }
        $results.Add('PASS gathering INT-06 progression follow-ups delivered once after replay')
        Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null
        $env:SF_GAMEPLAY_FAULT_AFTER_GATHER_GRANT = 'gathering/fault/2'
        $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'gather-post-grant-fault.out') -RedirectStandardError (Join-Path $private 'gather-post-grant-fault.err')
        for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Post-grant fault host exited before ready.' }; if ((Test-Path (Join-Path $private 'gather-post-grant-fault.out')) -and (Get-Content (Join-Path $private 'gather-post-grant-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
        $currentStep = 'gathering crash after ledger grant'
        $results.Add((Invoke-Checked $dotnet @($journeyDll,'gathering-grant-fault') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
        if (-not $hostProcess.WaitForExit(10000)) { throw 'Post-grant injected fault did not stop host.' }
        $pending = Sql "SELECT count(*) FROM inventory.gathering_harvest_receipts WHERE operation_id='gathering/fault/2' AND delivered_at IS NULL" "gameplay_$run"
        $committed = Sql "SELECT count(*) FROM inventory.operation_receipts WHERE operation_id='gathering/fault/2'" "gameplay_$run"
        if ($pending -ne '1' -or $committed -ne '1') { throw "Post-grant crash boundary differs: $pending/$committed" }
        Remove-Item Env:SF_GAMEPLAY_FAULT_AFTER_GATHER_GRANT
        Cluster stop; Cluster start
        $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'gather-post-grant-recover.out') -RedirectStandardError (Join-Path $private 'gather-post-grant-recover.err')
        for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Post-grant recovery host exited.' }; if ((Test-Path (Join-Path $private 'gather-post-grant-recover.out')) -and (Get-Content (Join-Path $private 'gather-post-grant-recover.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
        $currentStep = 'gathering post-grant replay'
        $results.Add((Invoke-Checked $dotnet @($journeyDll,'gathering-grant-replay') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
        $done = Sql "SELECT count(*) FROM inventory.gathering_harvest_receipts WHERE operation_id='gathering/fault/2' AND delivered_at IS NOT NULL" "gameplay_$run"
        if ($done -ne '1') { throw 'Post-grant replay did not mark intent delivered.' }
    } elseif ($AppearanceOnly) {
        $currentStep = 'appearance wire'
        $results.Add((Invoke-Checked $dotnet @($journeyDll,'appearance') @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection;
            GAMEPLAY_CHARACTER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
        }))
    } else {
    if (-not $RecipesOnly) {
    foreach ($name in @('Rewards','Progression','Housing')) {
        $currentStep = "wire $name"
        $dll = Join-Path $buildOutput "$name.Host.Tests.dll"
        $results.Add((Invoke-Checked $dotnet @($dll,$name.ToLowerInvariant()) @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
        }))
    }
    $currentStep = 'death wire'
    $results.Add((Invoke-Checked $dotnet @($journeyDll,'death') @{
        GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection;
        GAMEPLAY_CHARACTER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
    }))
    $currentStep = 'journey'
    $results.Add((Invoke-Checked $dotnet @($journeyDll,'journey') @{
        GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection;
        GAMEPLAY_CHARACTER_CONNECTION=$connection; GAMEPLAY_CONTENT_ROOT=(Join-Path $repo 'content'); SF_GAMEPLAY_TOKEN=$token
    }))
    }
    $currentStep = 'recipe learning and grades'
    $results.Add((Invoke-Checked $dotnet @($journeyDll,'recipes') @{
        GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection;
        GAMEPLAY_CHARACTER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
    }))
    $currentStep = 'recipe scroll interruption'
    $results.Add((Invoke-Checked $dotnet @($journeyDll,'recipes-crash') @{
        GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection;
        GAMEPLAY_CHARACTER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
    }))
    if (-not $hostProcess.WaitForExit(10000)) { throw 'Recipe fault did not stop the host.' }
    Remove-Item Env:SF_GAMEPLAY_FAULT_AFTER_SCROLL_CONSUME
    $currentStep = 'housing restart persistence'
    Cluster stop
    Cluster start
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-restart.out') -RedirectStandardError (Join-Path $private 'host-restart.err')
    $ready = $false
    for ($i=0; $i -lt 100; $i++) {
        if ($hostProcess.HasExited) { throw 'Gameplay host exited after restart.' }
        if ((Test-Path (Join-Path $private 'host-restart.out')) -and (Get-Content (Join-Path $private 'host-restart.out') -Raw) -match 'GAMEPLAY_HOST_READY') { $ready = $true; break }
        Start-Sleep -Milliseconds 100
    }
    if (-not $ready) { throw 'Gameplay host restart timeout.' }
    $results.Add((Invoke-Checked $dotnet @($journeyDll,'recipes-replay') @{
        GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
    }))
    if (-not $RecipesOnly) {
    foreach ($name in @('Rewards','Progression','Housing')) {
        $dll = Join-Path $buildOutput "$name.Host.Tests.dll"
        $results.Add((Invoke-Checked $dotnet @($dll,($name.ToLowerInvariant() + '-replay')) @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
        }))
    }
    $currentStep = 'housing geometry seed'
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-geometry-seed') @{
        GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
    }))
    $currentStep = 'post-refund crash'
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null
    $env:SF_GAMEPLAY_FAULT_AFTER_REMOVE_GRANT = 'remove/fault'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-fault.out') -RedirectStandardError (Join-Path $private 'host-fault.err')
    for ($i=0; $i -lt 100; $i++) {
        if ($hostProcess.HasExited) { throw 'Fault host exited before ready.' }
        if ((Test-Path (Join-Path $private 'host-fault.out')) -and (Get-Content (Join-Path $private 'host-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }
        Start-Sleep -Milliseconds 100
    }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-fault-seed') @{
        GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
    }))
    $hostProcess.WaitForExit(10000) | Out-Null
    if (-not $hostProcess.HasExited) { throw 'Fault host did not exit.' }
    Cluster stop
    Cluster start
    Remove-Item Env:SF_GAMEPLAY_FAULT_AFTER_REMOVE_GRANT -ErrorAction SilentlyContinue
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-recovered.out') -RedirectStandardError (Join-Path $private 'host-recovered.err')
    $ready = $false
    for ($i=0; $i -lt 100; $i++) {
        if ($hostProcess.HasExited) { throw 'Recovery host exited before ready.' }
        if ((Test-Path (Join-Path $private 'host-recovered.out')) -and (Get-Content (Join-Path $private 'host-recovered.out') -Raw) -match 'GAMEPLAY_HOST_READY') { $ready = $true; break }
        Start-Sleep -Milliseconds 100
    }
    if (-not $ready) { throw 'Recovery host readiness timeout.' }
    $currentStep = 'housing geometry restart read'
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-geometry-check') @{
        GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
    }))
    $currentStep = 'refund recovery and contention'
    foreach ($mode in @('fixture-fault-replay','fixture-stale-concurrent')) {
        $results.Add((Invoke-Checked $dotnet @($hostDll,$mode) @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
        }))
    }
    $currentStep = 'full reward backpack'
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-quest-full') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $pending = Sql "SELECT state FROM character_state.reward_receipts WHERE character_id='chr_quest_full'" "gameplay_$run"
    $premature = Sql "SELECT count(*) FROM inventory.operation_receipts WHERE operation_id LIKE 'quest:chr_quest_full:%'" "gameplay_$run"
    if ($pending -ne 'Pending' -or $premature -ne '0') { throw "Full backpack did not preserve pending intent: $pending/$premature" }
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null
    $questFile = Join-Path $fixtureContent 'quests/greenlands_frontier_founder.json'
    $questOriginal = Get-Content -LiteralPath $questFile -Raw
    $questChanged = $questOriginal.Replace('"revision": 1','"revision": 2').Replace('"definition_id": "item/rough_timber", "quantity": 4','"definition_id": "item/rough_timber", "quantity": 9')
    if ($questChanged -eq $questOriginal) { throw 'Fixture quest content did not change.' }
    [IO.File]::WriteAllText($questFile,$questChanged)
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-changed-content.out') -RedirectStandardError (Join-Path $private 'host-changed-content.err')
    for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Changed-content host exited.' }; if ((Test-Path (Join-Path $private 'host-changed-content.out')) -and (Get-Content (Join-Path $private 'host-changed-content.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-quest-free') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    Start-Sleep -Seconds 9
    $fullState = Sql "SELECT state FROM character_state.reward_receipts WHERE character_id='chr_quest_full'" "gameplay_$run"
    $fullLines = Sql "SELECT string_agg(definition_id || ':' || quantity, ',' ORDER BY definition_id) FROM inventory.entries WHERE container_id='ctn_quest_full'" "gameplay_$run"
    $fullOps = Sql "SELECT count(*) FROM inventory.operation_receipts WHERE operation_id LIKE 'quest:chr_quest_full:%'" "gameplay_$run"
    if ($fullState -ne 'Complete' -or $fullLines -ne 'item/iron_nails:8,item/rough_timber:4,item/starter_tool_repair_kit:1' -or $fullOps -ne '1') { throw "Full backpack recovery failed: $fullState/$fullLines/$fullOps" }
    $results.Add('PASS reward intent granted original item snapshot after content revision and lines changed')
    [IO.File]::WriteAllText($questFile,$questOriginal)
    $currentStep = 'abandoned placement'
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null
    $env:SF_GAMEPLAY_FAULT_AFTER_PLACE_INTENT = 'place/abandon'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-place-fault.out') -RedirectStandardError (Join-Path $private 'host-place-fault.err')
    for ($i=0; $i -lt 100; $i++) { if ((Test-Path (Join-Path $private 'host-place-fault.out')) -and (Get-Content (Join-Path $private 'host-place-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-place-abandon') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $hostProcess.WaitForExit(10000) | Out-Null
    if (-not $hostProcess.HasExited) { throw 'Placement fault host did not exit.' }
    Remove-Item Env:SF_GAMEPLAY_FAULT_AFTER_PLACE_INTENT
    $currentStep = 'killed after placement decision'
    $env:SF_GAMEPLAY_FAULT_AFTER_PLACE_DECISION = 'place/decided'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-decision-fault.out') -RedirectStandardError (Join-Path $private 'host-decision-fault.err')
    for ($i=0; $i -lt 100; $i++) { if ((Test-Path (Join-Path $private 'host-decision-fault.out')) -and (Get-Content (Join-Path $private 'host-decision-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-place-decided') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $hostProcess.WaitForExit(10000) | Out-Null
    if (-not $hostProcess.HasExited) { throw 'Decision fault host did not exit.' }
    Remove-Item Env:SF_GAMEPLAY_FAULT_AFTER_PLACE_DECISION
    $decided = Sql "SELECT state || ':' || decision FROM housing.placements WHERE operation_id='place/decided'" "gameplay_$run"
    if ($decided -ne 'Pending:Commit') { throw "Decision fault left $decided instead of a durable commit decision." }
    $currentStep = 'abandoned reward'
    $env:SF_GAMEPLAY_FAULT_AFTER_QUEST_GRANT = 'quest:chr_quest_abandon:quest/frontier_founder:defeat_boar:reward/frontier_founder_completion'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-quest-fault.out') -RedirectStandardError (Join-Path $private 'host-quest-fault.err')
    for ($i=0; $i -lt 100; $i++) { if ((Test-Path (Join-Path $private 'host-quest-fault.out')) -and (Get-Content (Join-Path $private 'host-quest-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-quest-abandon') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $hostProcess.WaitForExit(10000) | Out-Null
    if (-not $hostProcess.HasExited) { throw 'Reward fault host did not exit.' }
    Cluster stop
    Cluster start
    Remove-Item Env:SF_GAMEPLAY_FAULT_AFTER_QUEST_GRANT
    $currentStep = 'dual-host sweep'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-sweep-a.out') -RedirectStandardError (Join-Path $private 'host-sweep-a.err')
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0); $listener.Start(); $hostPort2 = $listener.LocalEndpoint.Port; $listener.Stop()
    $env:GAMEPLAY_HOST_PORT = "$hostPort2"
    $hostProcess2 = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-sweep-b.out') -RedirectStandardError (Join-Path $private 'host-sweep-b.err')
    $env:GAMEPLAY_HOST_PORT = "$hostPort"
    Start-Sleep -Seconds 9
    if ($hostProcess.HasExited -or $hostProcess2.HasExited) { throw 'Sweep host exited.' }
    $placement = Sql "SELECT state FROM housing.placements WHERE operation_id='place/abandon'" "gameplay_$run"
    $rewardState = Sql "SELECT state FROM character_state.reward_receipts WHERE character_id='chr_quest_abandon'" "gameplay_$run"
    $rewardLines = Sql "SELECT string_agg(definition_id || ':' || quantity, ',' ORDER BY definition_id) FROM inventory.entries WHERE container_id='ctn_quest_abandon'" "gameplay_$run"
    $rewardOps = Sql "SELECT count(*) FROM inventory.operation_receipts WHERE operation_id LIKE 'quest:chr_quest_abandon:%'" "gameplay_$run"
    if ($placement -ne 'Compensated' -or $rewardState -ne 'Complete' -or $rewardLines -ne 'item/iron_nails:8,item/rough_timber:4,item/starter_tool_repair_kit:1' -or $rewardOps -ne '1') { throw "Sweep failed: placement=$placement reward=$rewardState lines=$rewardLines ops=$rewardOps" }
    $sweepLogs = @((Get-Content (Join-Path $private 'host-sweep-a.out') | Where-Object { $_ -match '^GAMEPLAY_SWEEP (placement place/abandon|reward quest:chr_quest_abandon:)' }))
    $sweepLogs += @(Get-Content (Join-Path $private 'host-sweep-b.out') | Where-Object { $_ -match '^GAMEPLAY_SWEEP (placement place/abandon|reward quest:chr_quest_abandon:)' })
    $sweepLogs += @(Get-Content (Join-Path $private 'host-quest-fault.out') | Where-Object { $_ -match '^GAMEPLAY_SWEEP placement place/abandon' })
    if ($sweepLogs.Count -ne 2) { throw "Expected one sweep resolution per abandoned intent, got $($sweepLogs.Count)." }
    $results.Add('PASS dual-host sweeper resolved abandoned placement and reward once after PostgreSQL restart')
    # Kill after commit: the restarted host rolls the decision forward, debiting the hold once and keeping the piece.
    $decided = Sql "SELECT state || ':' || decision FROM housing.placements WHERE operation_id='place/decided'" "gameplay_$run"
    $decidedLines = Sql "SELECT count(*) FROM inventory.entries WHERE container_id IN ('ctn_decided','ctn_hold_chr_decided')" "gameplay_$run"
    $decidedOps = Sql "SELECT string_agg(operation_id, ',' ORDER BY operation_id) FROM inventory.operation_receipts WHERE operation_id LIKE 'place/decided/%'" "gameplay_$run"
    if ($decided -ne 'Committed:Commit' -or $decidedLines -ne '0' -or $decidedOps -ne 'place/decided/consume,place/decided/reserve') { throw "Commit roll-forward failed: $decided lines=$decidedLines ops=$decidedOps" }
    $results.Add('PASS host killed after commit decision rolled forward: reserved, debited once, structure committed')
    Stop-Process -Id $hostProcess2.Id -Force; $hostProcess2.WaitForExit(10000) | Out-Null; $hostProcess2 = $null
    $currentStep = 'exchange wire trade and market'
    foreach ($mode in @('fixture-exchange-seed','fixture-exchange-trade','fixture-exchange-market','fixture-exchange-cancel','fixture-exchange-expire-seed')) {
        $results.Add((Invoke-Checked $dotnet @($hostDll,$mode) @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; GAMEPLAY_EXCHANGE_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    }
    [void](Sql "UPDATE exchange.settlements SET created_at=now()-interval '6 minutes' WHERE operation_id='exchange/offer-expire'" "gameplay_$run")
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-exchange-expire-check') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; GAMEPLAY_EXCHANGE_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    Start-Sleep -Seconds 7
    $expiredState = Sql "SELECT state FROM exchange.settlements WHERE operation_id='exchange/offer-expire'" "gameplay_$run"
    $returnedOre = Sql "SELECT COALESCE((SELECT quantity FROM inventory.entries WHERE container_id='ctn_expire_escrow' AND definition_id='item/iron_ore'),0)" "gameplay_$run"
    if ($expiredState -ne 'cancelled' -or $returnedOre -ne '0') { throw "Trade deadline cancellation failed: $expiredState/$returnedOre" }
    $results.Add('PASS trade offer deadline cancelled and returned escrow')
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null
    $currentStep = 'listing escrow crash recovery'
    $env:SF_GAMEPLAY_FAULT_AFTER_LISTING_ESCROW = 'exchange/list-fault'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-list-fault.out') -RedirectStandardError (Join-Path $private 'host-list-fault.err')
    for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Listing fault host exited before ready.' }; if ((Test-Path (Join-Path $private 'host-list-fault.out')) -and (Get-Content (Join-Path $private 'host-list-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    try { [void](Invoke-Checked $dotnet @($hostDll,'fixture-exchange-fault') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; GAMEPLAY_EXCHANGE_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }); throw 'Listing fault did not fire.' }
    catch { if ($_.Exception.Message -match 'Listing fault did not fire') { throw } }
    $hostProcess.WaitForExit(10000) | Out-Null
    if (-not $hostProcess.HasExited) { throw 'Listing fault host did not exit.' }
    Cluster stop
    Cluster start
    Remove-Item Env:SF_GAMEPLAY_FAULT_AFTER_LISTING_ESCROW
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-list-recovered.out') -RedirectStandardError (Join-Path $private 'host-list-recovered.err')
    for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Listing recovery host exited.' }; if ((Test-Path (Join-Path $private 'host-list-recovered.out')) -and (Get-Content (Join-Path $private 'host-list-recovered.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    Start-Sleep -Seconds 9
    $listingState = Sql "SELECT state FROM exchange.listings WHERE id='lst_fault'" "gameplay_$run"
    $escrowReceipt = Sql "SELECT count(*) FROM inventory.operation_receipts WHERE operation_id='exchange/list-fault/escrow' AND receipt->>'Action'='2'" "gameplay_$run"
    if ($listingState -ne 'active' -or $escrowReceipt -ne '1') { throw "Listing escrow recovery failed: $listingState/$escrowReceipt" }
    $results.Add('PASS listing escrow crash and immediate PostgreSQL restart created one active listing')
    $missingIntent = @{ Command='listing-intent'; Id='lst_missing'; Seller='chr_exchange_seller'; SellerContainer='ctn_chr_exchange_seller'; Escrow='ctn_fault_escrow'; Item='item/iron_ore'; Quantity=1; Price=20; Fee=0; Now=[DateTimeOffset]::UtcNow.ToString('o'); DurationHours=48; CreatedAt=[DateTimeOffset]::UtcNow.AddSeconds(-20).ToString('o') } | ConvertTo-Json -Compress
    $missingReceipt = @{ OperationId='exchange/list-missing'; Accepted=$true; Code='pending'; SubjectId='lst_missing'; Amount=0; Fee=0 } | ConvertTo-Json -Compress
    [void](Sql "INSERT INTO exchange.operations VALUES ('exchange/list-missing','$missingIntent','$missingReceipt'::jsonb)" "gameplay_$run")
    Start-Sleep -Seconds 5
    $missingState = Sql "SELECT receipt->>'Code' FROM exchange.operations WHERE operation_id='exchange/list-missing'" "gameplay_$run"
    $tombstone = Sql "SELECT count(*) FROM inventory.operation_receipts WHERE operation_id='exchange/list-missing/escrow' AND fingerprint='exchange/listing-compensated'" "gameplay_$run"
    if ($missingState -ne 'compensated' -or $tombstone -ne '1') { throw "Missing listing escrow was not fenced: $missingState/$tombstone" }
    $results.Add('PASS absent listing escrow tombstoned at transfer operation key')
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null
    $env:SF_GAMEPLAY_FAULT_AFTER_EXCHANGE_DELIVERY = 'exchange/buy-fault'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-exchange-fault.out') -RedirectStandardError (Join-Path $private 'host-exchange-fault.err')
    for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Exchange fault host exited before ready.' }; if ((Test-Path (Join-Path $private 'host-exchange-fault.out')) -and (Get-Content (Join-Path $private 'host-exchange-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-exchange-fault') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; GAMEPLAY_EXCHANGE_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $hostProcess.WaitForExit(10000) | Out-Null
    if (-not $hostProcess.HasExited) { throw 'Exchange fault host did not exit.' }
    Cluster stop
    Cluster start
    Remove-Item Env:SF_GAMEPLAY_FAULT_AFTER_EXCHANGE_DELIVERY
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-exchange-recovered.out') -RedirectStandardError (Join-Path $private 'host-exchange-recovered.err')
    for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Exchange recovery host exited.' }; if ((Test-Path (Join-Path $private 'host-exchange-recovered.out')) -and (Get-Content (Join-Path $private 'host-exchange-recovered.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    Start-Sleep -Seconds 9
    $saleState = Sql "SELECT state FROM exchange.settlements WHERE operation_id='exchange/buy-fault'" "gameplay_$run"
    $saleReceipts = Sql "SELECT count(*) FROM inventory.operation_receipts WHERE operation_id='exchange/buy-fault/settle'" "gameplay_$run"
    if ($saleState -ne 'committed' -or $saleReceipts -ne '1') { throw "Exchange recovery not exact: $saleState/$saleReceipts" }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-exchange-replay') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; GAMEPLAY_EXCHANGE_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $results.Add('PASS exchange sweeper committed one sale after host kill and immediate PostgreSQL restart')
    $currentStep = 'INT-06 outbox fault/restart'
    $ledgerProject = Join-Path $repo 'BackendServices/InventoryLedger.Host/InventoryLedger.Host.csproj'
    [void](Invoke-Checked $dotnet @('restore',$ledgerProject,'--locked-mode','--packages',(Join-Path $repo 'Artifacts/Persistence/Toolchains/nuget'),'--verbosity','quiet'))
    $ledgerOutput = (Join-Path $private 'ledger-build') + [IO.Path]::DirectorySeparatorChar
    [void](Invoke-Checked $dotnet @('build',$ledgerProject,'--no-restore','--configuration','Release','--verbosity','quiet',"-p:OutDir=$ledgerOutput"))
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0); $listener.Start(); $ledgerPort = $listener.LocalEndpoint.Port; $listener.Stop()
    $env:INVENTORY_LEDGER_PORT = "$ledgerPort"; $env:INVENTORY_LEDGER_CONNECTION = $connection
    $env:SF_INVENTORY_LEDGER_TOKEN = $token; $env:GAMEPLAY_LEDGER_PORT = "$ledgerPort"
    $ledgerHost = Start-Process -FilePath $dotnet -ArgumentList @((Join-Path $ledgerOutput 'InventoryLedger.Host.dll')) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'ledger-host.out') -RedirectStandardError (Join-Path $private 'ledger-host.err')
    for ($i=0; $i -lt 100; $i++) { if ($ledgerHost.HasExited) { throw 'Ledger host exited.' }; if ((Test-Path (Join-Path $private 'ledger-host.out')) -and (Get-Content (Join-Path $private 'ledger-host.out') -Raw) -match 'INVENTORY_LEDGER_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null
    $env:SF_GAMEPLAY_FAULT_BEFORE_OUTBOX_SEND = 'int06/gather/1'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-outbox-fault.out') -RedirectStandardError (Join-Path $private 'host-outbox-fault.err')
    for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Outbox fault host exited before ready.' }; if ((Test-Path (Join-Path $private 'host-outbox-fault.out')) -and (Get-Content (Join-Path $private 'host-outbox-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-outbox-seed') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_PORT="$ledgerPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $hostProcess.WaitForExit(15000) | Out-Null
    if (-not $hostProcess.HasExited) { throw 'Outbox fault did not kill host before send.' }
    $before = Sql "SELECT count(*) FROM character_state.source_receipts WHERE operation_id='int06/gather/1'" "gameplay_$run"
    if ($before -ne '0') { throw 'Outbox sent before fault.' }
    Remove-Item Env:SF_GAMEPLAY_FAULT_BEFORE_OUTBOX_SEND
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-outbox-restart.out') -RedirectStandardError (Join-Path $private 'host-outbox-restart.err')
    for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Outbox recovery host exited.' }; if ((Test-Path (Join-Path $private 'host-outbox-restart.out')) -and (Get-Content (Join-Path $private 'host-outbox-restart.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-outbox-replay') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_PORT="$ledgerPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-outbox-conflict') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_PORT="$ledgerPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-outbox-omit') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_PORT="$ledgerPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    for ($i=0; $i -lt 400; $i++) { if ((Sql "SELECT count(*) FROM inventory.receipt_outbox WHERE source_operation_id='int06/gather/1' AND delivered_at IS NOT NULL" "gameplay_$run") -eq '1') { break }; Start-Sleep -Milliseconds 100 }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-outbox-check') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $status = Invoke-Checked $dotnet @($hostDll,'outbox-status') @{ GAMEPLAY_LEDGER_CONNECTION=$connection }
    if ($status -notmatch 'pending=0 parked=0 batch=32 poll_seconds=2 max_attempts=12') { throw 'Outbox operator counts or controls incorrect.' }
    $results.Add('PASS INT-06 operator counts and effective defaults')
    $effects = Sql "SELECT count(*) FROM character_state.source_receipts WHERE operation_id='int06/gather/1'" "gameplay_$run"
    if ($effects -ne '2') { throw "Outbox produced $effects progression/quest receipts." }
    $results.Add('PASS INT-06 committed intent survived host kill, exact once progression receipt')
    Stop-Process -Id $ledgerHost.Id -Force; $ledgerHost.WaitForExit(10000) | Out-Null
    Cluster stop
    foreach ($name in @('Rewards','Progression','Housing')) {
        $dll = Join-Path $buildOutput "$name.Host.Tests.dll"
        $results.Add((Invoke-Checked $dotnet @($dll,($name.ToLowerInvariant() + '-outage')) @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
        }))
    }
    }
    }
    $results | Set-Content (Join-Path $EvidenceRoot 'results.txt')
}
catch {
    $failure = ($currentStep + ': ' + $_.Exception.Message).Replace($adminPassword,'[REDACTED]').Replace($servicePassword,'[REDACTED]')
    if ($token.Length -gt 0) { $failure = $failure.Replace($token,'[REDACTED]') }
    $hostError = Join-Path $private 'host.err'
    if (Test-Path $hostError) {
        $tail = ((Get-Content $hostError -Tail 12) -join ' ').Replace($adminPassword,'[REDACTED]').Replace($servicePassword,'[REDACTED]')
        if ($token.Length -gt 0) { $tail = $tail.Replace($token,'[REDACTED]') }
        $failure += " Host: $tail"
    }
}
finally {
    foreach ($key in @('SF_GAMEPLAY_TOKEN','SF_GAMEPLAY_FAULT_AFTER_GATHER_INTENT','SF_GAMEPLAY_FAULT_AFTER_GATHER_GRANT','SF_GAMEPLAY_FAULT_AFTER_SCROLL_CONSUME','SF_GAMEPLAY_FAULT_AFTER_REMOVE_GRANT','SF_GAMEPLAY_FAULT_AFTER_PLACE_INTENT','SF_GAMEPLAY_FAULT_AFTER_PLACE_DECISION','SF_GAMEPLAY_FAULT_AFTER_QUEST_GRANT','SF_GAMEPLAY_FAULT_AFTER_LISTING_ESCROW','SF_GAMEPLAY_FAULT_BEFORE_OUTBOX_SEND','GAMEPLAY_LEDGER_CONNECTION','GAMEPLAY_LEDGER_PORT','GAMEPLAY_FIXTURE_MODE','GAMEPLAY_FIXTURE_GATHERING_RESPAWN_SECONDS','INVENTORY_LEDGER_PORT','INVENTORY_LEDGER_CONNECTION','SF_INVENTORY_LEDGER_TOKEN','GAMEPLAY_CHARACTER_CONNECTION','GAMEPLAY_HOUSING_CONNECTION')) {
        Remove-Item "Env:$key" -ErrorAction SilentlyContinue
    }
    try {
        if ($hostProcess -and -not $hostProcess.HasExited) { Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null }
        if ($hostProcess2 -and -not $hostProcess2.HasExited) { Stop-Process -Id $hostProcess2.Id -Force; $hostProcess2.WaitForExit(10000) | Out-Null }
        if ($ledgerHost -and -not $ledgerHost.HasExited) { Stop-Process -Id $ledgerHost.Id -Force; $ledgerHost.WaitForExit(10000) | Out-Null }
        if (Test-Path (Join-Path $private 'owner.json')) {
            Assert-Owned
            if ($started -or ($attempted -and (Test-Path (Join-Path $data 'postmaster.pid')))) { try { [void](Invoke-Checked (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-m','immediate','-w','-t','30','stop')) } catch { if ($failure -eq '') { $failure = $_.Exception.Message } } }
            Assert-Owned
            Remove-Item -LiteralPath $private -Recurse -Force; $removed = $true
        }
    } catch { if ($failure -eq '') { $failure = $_.Exception.Message } }
    @{status=$(if ($failure -eq '') {'pass'} else {'fail'}); privateStorageRemoved=$removed; error=$failure} | ConvertTo-Json | Set-Content (Join-Path $EvidenceRoot 'harness.json')
}
if ($failure -ne '') { throw $failure }
Get-Content (Join-Path $EvidenceRoot 'results.txt')
