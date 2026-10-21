[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/Persistence/GameplayHosts'))
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $EvidenceRoot)) { throw 'Supply a fresh GameplayHosts evidence child.' }
$pins = Get-Content (Join-Path $repo 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
$dotnet = Join-Path $repo 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
$pg = Join-Path $repo 'Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
foreach ($path in @( $dotnet,(Join-Path $pg 'postgres.exe'),(Join-Path $pg 'initdb.exe'),(Join-Path $pg 'pg_ctl.exe'),(Join-Path $pg 'psql.exe'))) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file missing: $path" } }
if ((Get-FileHash $dotnet -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.sdk.executableSha256 -or (Get-FileHash (Join-Path $pg 'postgres.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.postgres.executableSha256) { throw 'Pinned toolchain mismatch.' }
$run = [guid]::NewGuid().ToString('N'); $private = Join-Path $EvidenceRoot 'private'; $data = Join-Path $private 'data'
$rng = [Security.Cryptography.RandomNumberGenerator]::Create(); $bytes = [byte[]]::new(32)
$rng.GetBytes($bytes); $adminPassword = [BitConverter]::ToString($bytes).Replace('-','')
$rng.GetBytes($bytes); $servicePassword = [BitConverter]::ToString($bytes).Replace('-',''); $rng.Dispose()
$port = 0; $started = $false; $attempted = $false; $removed = $false; $failure = ''; $token = ''; $hostProcess = $null; $hostProcess2 = $null; $currentStep = 'setup'
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
    $projects = @('Rewards','Progression','Housing') | ForEach-Object { Join-Path $repo "BackendServices/Gameplay.Host.Tests/$_/$_.Host.Tests.csproj" }
    $buildOutput = (Join-Path $private 'build') + [IO.Path]::DirectorySeparatorChar
    foreach ($project in $projects) {
        $currentStep = "restore $project"
        [void](Invoke-Checked $dotnet @('restore',$project,'--locked-mode','--packages',(Join-Path $repo 'Artifacts/Persistence/Toolchains/nuget'),'--verbosity','quiet'))
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
    $schemas += Get-Content (Join-Path $repo 'BackendServices/Housing.Postgres/Schema.Gameplay.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/Housing.Postgres/Schema.Geometry.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.Position.sql') -Raw
    [void](Sql ("SET ROLE gameplay_owner;`n" + ($schemas -join "`n")) "gameplay_$run")
    [void](Sql 'GRANT USAGE ON SCHEMA inventory,character_state,housing,exchange TO gameplay_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA inventory TO gameplay_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA character_state TO gameplay_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA housing TO gameplay_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA exchange TO gameplay_svc;' "gameplay_$run")
    $connection = "Host=127.0.0.1;Port=$port;Database=gameplay_$run;Username=gameplay_svc;Password=$servicePassword;Pooling=false;Timeout=2;Command Timeout=5;Include Error Detail=false"
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
    foreach ($name in @('Rewards','Progression','Housing')) {
        $currentStep = "wire $name"
        $dll = Join-Path $buildOutput "$name.Host.Tests.dll"
        $results.Add((Invoke-Checked $dotnet @($dll,$name.ToLowerInvariant()) @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
        }))
    }
    $journeyDll = Join-Path $buildOutput 'Progression.Host.Tests.dll'
    $currentStep = 'journey'
    $results.Add((Invoke-Checked $dotnet @($journeyDll,'journey') @{
        GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection;
        GAMEPLAY_CONTENT_ROOT=(Join-Path $repo 'content'); SF_GAMEPLAY_TOKEN=$token
    }))
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null
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
    Cluster stop
    foreach ($name in @('Rewards','Progression','Housing')) {
        $dll = Join-Path $buildOutput "$name.Host.Tests.dll"
        $results.Add((Invoke-Checked $dotnet @($dll,($name.ToLowerInvariant() + '-outage')) @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
        }))
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
    foreach ($key in @('SF_GAMEPLAY_TOKEN','SF_GAMEPLAY_FAULT_AFTER_REMOVE_GRANT','SF_GAMEPLAY_FAULT_AFTER_PLACE_INTENT','SF_GAMEPLAY_FAULT_AFTER_QUEST_GRANT','SF_GAMEPLAY_FAULT_AFTER_LISTING_ESCROW','GAMEPLAY_LEDGER_CONNECTION','GAMEPLAY_CHARACTER_CONNECTION','GAMEPLAY_HOUSING_CONNECTION')) {
        Remove-Item "Env:$key" -ErrorAction SilentlyContinue
    }
    try {
        if ($hostProcess -and -not $hostProcess.HasExited) { Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null }
        if ($hostProcess2 -and -not $hostProcess2.HasExited) { Stop-Process -Id $hostProcess2.Id -Force; $hostProcess2.WaitForExit(10000) | Out-Null }
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

