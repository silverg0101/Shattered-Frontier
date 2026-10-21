[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& (Join-Path $repo 'tools/Assert-InventoryOutboxMigration.ps1') -MigrationPath (Join-Path $repo 'BackendServices/InventoryLedger.Postgres/Schema.Outbox.sql') | Out-Null
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/Persistence/RestoreRehearsal'))
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $EvidenceRoot)) { throw 'Supply a fresh RestoreRehearsal evidence child.' }
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
$restoreData = Join-Path $private 'restore-data'; $restoreStarted = $false; $primaryPort = 0
$receiptFile = Join-Path $EvidenceRoot 'client-receipts.jsonl'
$hostDll = ''
function Invoke-Checked([string]$Executable,[string[]]$Arguments,[hashtable]$Environment = @{}) {
    $info = [Diagnostics.ProcessStartInfo]::new(); $info.FileName = $Executable; $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true
    $info.Arguments = (($Arguments | ForEach-Object { '"' + $_.Replace('"','\"') + '"' }) -join ' ')
    foreach ($key in @($info.EnvironmentVariables.Keys)) { if ($key -match '^PG|^GAMEPLAY_') { $info.EnvironmentVariables.Remove($key) } }
    foreach ($key in $Environment.Keys) { $info.EnvironmentVariables[$key] = $Environment[$key] }
    if ($Arguments.Count -gt 0 -and $Arguments[0] -eq $hostDll -and $Arguments.Count -gt 1 -and $Arguments[1] -like 'fixture-*') { $info.EnvironmentVariables['REHEARSAL_RECEIPT_LEDGER'] = $receiptFile }
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
function TableSnapshot([string]$Database) {
    $tables = @('inventory.schema_version','inventory.containers','inventory.entries','inventory.operation_receipts','inventory.receipt_outbox',
        'character_state.schema_version','character_state.progression','character_state.quests','character_state.source_receipts','character_state.reward_receipts','character_state.positions',
        'housing.schema_version','housing.claims','housing.claim_operations','housing.claim_structure_revisions','housing.placements','housing.claim_geometry','housing.removals',
        'exchange.schema_version','exchange.wallets','exchange.wallet_entries','exchange.operations','exchange.listings','exchange.settlements',
        'terrain_delta.schema_version','terrain_delta.regions','terrain_delta.operations')
    $snapshot = [ordered]@{}
    foreach ($table in $tables) {
        $snapshot[$table] = (Sql "SELECT count(*),md5(coalesce(string_agg(row_to_json(t)::text, '|' ORDER BY row_to_json(t)::text),'')) FROM $table t" $Database).Trim()
    }
    return $snapshot
}
function Assert-Receipts([string]$Database,[string]$ReceiptFile,[int]$BackupCount = -1) {
    $receipts = @(Get-Content -LiteralPath $ReceiptFile | ForEach-Object { $_ | ConvertFrom-Json })
    if ($receipts.Count -eq 0 -or ($BackupCount -ge 0 -and $receipts.Count -le $BackupCount)) { throw 'Client receipt ledger is empty or lacks a post-backup acknowledgement.' }
    $seen = @{}
    for ($i = 0; $i -lt $receipts.Count; $i++) {
        $receipt = $receipts[$i]
        $id = [string]$receipt.operationId
        if ($id -notmatch '^[a-zA-Z0-9_:/.-]+$' -or $receipt.receiptId -notmatch '^[a-zA-Z0-9_:/.-]+$' -or -not $receipt.acknowledgedAtUtc -or -not $receipt.rpc) { throw "Invalid client receipt ledger row $i." }
        if ($BackupCount -ge 0 -and $i -ge $BackupCount) { continue }
        $target = switch ([string]$receipt.rpc) {
            ApplyReceipt {'character_state.source_receipts'} ClaimPlot {'housing.claim_operations'}
            Place {'housing.placements'} Remove {'housing.removals'}
            AcceptTrade {'exchange.settlements'}
            { $_ -in @('OfferTrade','ListItem','BuyListing','CancelTrade','CancelListing') } {'exchange.operations'}
            'Exchange.Credit' {'exchange.wallet_entries'}
            default { throw "Unmapped receipt RPC: $($receipt.rpc)" }
        }
        if ($receipt.rpc -eq 'AcceptTrade') { $id = [string]$receipt.receiptId }
        $key = "$target/$id"
        if ($seen.ContainsKey($key)) { continue }
        $seen[$key] = $true
        $count = (Sql "SELECT count(*) FROM $target WHERE operation_id='$id'" $Database).Trim()
        if ($target -eq 'character_state.source_receipts') {
            $duplicates = (Sql "SELECT count(*) FROM (SELECT domain FROM $target WHERE operation_id='$id' GROUP BY domain HAVING count(*)<>1) d" $Database).Trim()
            if ($duplicates -ne '0' -or [long]$count -lt 1 -or [long]$count -gt 2) { throw "Acknowledged progression receipt $id is inconsistent." }
        } elseif ([long]$count -ne 1) { throw "Acknowledged receipt $key has $count stored copies; expected one." }
        if ($target -in @('housing.claim_operations','housing.placements','housing.removals') -and $receipt.revision -ne '') {
            $storedRevision = (Sql "SELECT revision FROM $target WHERE operation_id='$id'" $Database).Trim()
            if ($storedRevision -ne [string]$receipt.revision) { throw "Acknowledged revision for $key differs from stored revision." }
        }
    }
    if ($BackupCount -ge 0) {
        foreach ($receipt in $receipts[$BackupCount..($receipts.Count-1)]) {
            $id = [string]$receipt.operationId
            if ((Sql "SELECT count(*) FROM exchange.wallet_entries WHERE operation_id='$id'" $Database).Trim() -ne '0') { throw "Post-backup receipt $id appeared in restore." }
        }
    }
    return $seen.Count
}
try {
    [void](New-Item -ItemType Directory -Path $private -Force)
    $account = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    [void](Invoke-Checked 'icacls.exe' @($private,'/inheritance:r','/grant:r',"${account}:(OI)(CI)F"))
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0); $listener.Start(); $port = $listener.LocalEndpoint.Port; $listener.Stop()
    $primaryPort = $port
    @{run=$run;data=$data;port=$port} | ConvertTo-Json | Set-Content (Join-Path $private 'owner.json')
    $projects = @('Rewards','Progression','Housing') | ForEach-Object { Join-Path $repo "BackendServices/Gameplay.Host.Tests/$_/$_.Host.Tests.csproj" }
    foreach ($project in $projects) {
        $currentStep = "restore $project"
        [void](Invoke-Checked $dotnet @('restore',$project,'--locked-mode','--packages',(Join-Path $repo 'Artifacts/Persistence/Toolchains/nuget'),'--verbosity','quiet'))
        $currentStep = "build $project"
        [void](Invoke-Checked $dotnet @('build',$project,'--no-restore','--configuration','Release','--verbosity','quiet',"-p:BaseOutputPath=$(Join-Path $private 'build')/"))
    }
    $pwfile = Join-Path $private 'admin-password'; [IO.File]::WriteAllText($pwfile,$adminPassword)
    [void](Invoke-Checked (Join-Path $pg 'initdb.exe') @('-D',$data,'-U','fixture_admin','--pwfile',$pwfile,'--auth','scram-sha-256','--encoding','UTF8','--no-locale'))
    Remove-Item -LiteralPath $pwfile -Force
    [IO.File]::AppendAllText((Join-Path $data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$port`nfsync=on`nsynchronous_commit=on`nfull_page_writes=on`n")
    [IO.File]::WriteAllLines((Join-Path $data 'pg_hba.conf'),@('host all fixture_admin 127.0.0.1/32 scram-sha-256',"host gameplay_$run gameplay_svc 127.0.0.1/32 scram-sha-256",'host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    Cluster start
    [void](Sql "CREATE ROLE gameplay_owner NOLOGIN; CREATE ROLE gameplay_svc LOGIN PASSWORD '$servicePassword'; CREATE DATABASE gameplay_$run;")
    [void](Sql "REVOKE ALL ON DATABASE gameplay_$run FROM PUBLIC; GRANT CONNECT ON DATABASE gameplay_$run TO gameplay_svc; GRANT CREATE ON DATABASE gameplay_$run TO gameplay_owner; REVOKE ALL ON SCHEMA public FROM PUBLIC;" "gameplay_$run")
    $schemas = @('InventoryLedger.Postgres','CharacterState.Postgres','Housing.Postgres','Exchange.Postgres','TerrainDelta.Postgres') | ForEach-Object { Get-Content (Join-Path $repo "BackendServices/$_/Schema.sql") -Raw }
    $schemas += Get-Content (Join-Path $repo 'BackendServices/InventoryLedger.Postgres/Schema.Outbox.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/Housing.Postgres/Schema.Gameplay.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/Housing.Postgres/Schema.Geometry.sql') -Raw
    $schemas += Get-Content (Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.Position.sql') -Raw
    [void](Sql ("SET ROLE gameplay_owner;`n" + ($schemas -join "`n")) "gameplay_$run")
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
    $hostDll = Join-Path $private 'build/Release/net10.0/Gameplay.Host.dll'
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
        $dll = Join-Path $private "build/Release/net10.0/$name.Host.Tests.dll"
        $results.Add((Invoke-Checked $dotnet @($dll,$name.ToLowerInvariant()) @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
        }))
    }
    $journeyDll = Join-Path $private 'build/Release/net10.0/Progression.Host.Tests.dll'
    $currentStep = 'journey'
    $results.Add((Invoke-Checked $dotnet @($journeyDll,'journey') @{
        GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection;
        GAMEPLAY_CONTENT_ROOT=(Join-Path $repo 'content'); SF_GAMEPLAY_TOKEN=$token
    }))
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(30000) | Out-Null; $hostProcess.Refresh()
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-restart.out') -RedirectStandardError (Join-Path $private 'host-restart.err')
    $ready = $false
    for ($i=0; $i -lt 100; $i++) {
        if ($hostProcess.HasExited) { throw 'Gameplay host exited after restart.' }
        if ((Test-Path (Join-Path $private 'host-restart.out')) -and (Get-Content (Join-Path $private 'host-restart.out') -Raw) -match 'GAMEPLAY_HOST_READY') { $ready = $true; break }
        Start-Sleep -Milliseconds 100
    }
    if (-not $ready) { throw 'Gameplay host restart timeout.' }
    foreach ($name in @('Rewards','Progression','Housing')) {
        $dll = Join-Path $private "build/Release/net10.0/$name.Host.Tests.dll"
        $results.Add((Invoke-Checked $dotnet @($dll,($name.ToLowerInvariant() + '-replay')) @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
        }))
    }
    $currentStep = 'post-refund crash'
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(30000) | Out-Null; $hostProcess.Refresh()
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
    $hostProcess.WaitForExit(30000) | Out-Null
    $hostProcess.Refresh()
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
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(30000) | Out-Null; $hostProcess.Refresh()
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
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(30000) | Out-Null; $hostProcess.Refresh()
    $env:SF_GAMEPLAY_FAULT_AFTER_PLACE_INTENT = 'place/abandon'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-place-fault.out') -RedirectStandardError (Join-Path $private 'host-place-fault.err')
    for ($i=0; $i -lt 100; $i++) { if ((Test-Path (Join-Path $private 'host-place-fault.out')) -and (Get-Content (Join-Path $private 'host-place-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-place-abandon') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $hostProcess.WaitForExit(30000) | Out-Null; $hostProcess.Refresh()
    if (-not $hostProcess.HasExited) { throw 'Placement fault host did not exit.' }
    Remove-Item Env:SF_GAMEPLAY_FAULT_AFTER_PLACE_INTENT
    $currentStep = 'abandoned reward'
    $env:SF_GAMEPLAY_FAULT_AFTER_QUEST_GRANT = 'quest:chr_quest_abandon:quest/frontier_founder:defeat_boar:reward/frontier_founder_completion'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-quest-fault.out') -RedirectStandardError (Join-Path $private 'host-quest-fault.err')
    for ($i=0; $i -lt 100; $i++) { if ((Test-Path (Join-Path $private 'host-quest-fault.out')) -and (Get-Content (Join-Path $private 'host-quest-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-quest-abandon') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $hostProcess.WaitForExit(30000) | Out-Null; $hostProcess.Refresh()
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
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(30000) | Out-Null; $hostProcess.Refresh()
    $currentStep = 'listing escrow crash recovery'
    $env:SF_GAMEPLAY_FAULT_AFTER_LISTING_ESCROW = 'exchange/list-fault'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-list-fault.out') -RedirectStandardError (Join-Path $private 'host-list-fault.err')
    for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Listing fault host exited before ready.' }; if ((Test-Path (Join-Path $private 'host-list-fault.out')) -and (Get-Content (Join-Path $private 'host-list-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    try { [void](Invoke-Checked $dotnet @($hostDll,'fixture-exchange-fault') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; GAMEPLAY_EXCHANGE_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }); throw 'Listing fault did not fire.' }
    catch { if ($_.Exception.Message -match 'Listing fault did not fire') { throw } }
    $hostProcess.WaitForExit(30000) | Out-Null; $hostProcess.Refresh()
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
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(30000) | Out-Null; $hostProcess.Refresh()
    $env:SF_GAMEPLAY_FAULT_AFTER_EXCHANGE_DELIVERY = 'exchange/buy-fault'
    $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'host-exchange-fault.out') -RedirectStandardError (Join-Path $private 'host-exchange-fault.err')
    for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Exchange fault host exited before ready.' }; if ((Test-Path (Join-Path $private 'host-exchange-fault.out')) -and (Get-Content (Join-Path $private 'host-exchange-fault.out') -Raw) -match 'GAMEPLAY_HOST_READY') { break }; Start-Sleep -Milliseconds 100 }
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-exchange-fault') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; GAMEPLAY_EXCHANGE_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token }))
    $hostProcess.WaitForExit(30000) | Out-Null; $hostProcess.Refresh()
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
        $dll = Join-Path $private "build/Release/net10.0/$name.Host.Tests.dll"
        $results.Add((Invoke-Checked $dotnet @($dll,($name.ToLowerInvariant() + '-outage')) @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token
        }))
    }
    Cluster start
    $currentStep = 'receipt and conservation checks'
    $db = "gameplay_$run"
    $acknowledged = Assert-Receipts $db $receiptFile
    [void](Sql "INSERT INTO terrain_delta.regions(zone_id,cell_x,cell_y,revision) VALUES ('greenlands_silverbrook',71,19,1); INSERT INTO terrain_delta.operations(operation_id,fingerprint,zone_id,cell_x,cell_y,revision,character_id,claim_id,anchor_x,anchor_y,anchor_z,kind,magnitude,tombstone) VALUES ('terrain/rehearsal','terrain/rehearsal','greenlands_silverbrook',71,19,1,'chr_rehearsal','claim_rehearsal',1,2,3,'Dig',1,false)" "gameplay_$run")
    $backupAcknowledgements = @(Get-Content -LiteralPath $receiptFile).Count
    $bad = (Sql "SELECT (SELECT count(*) FROM inventory.entries WHERE quantity < 0) + (SELECT count(*) FROM exchange.wallets WHERE balance < 0 OR reserved < 0 OR reserved > balance) + (SELECT count(*) FROM housing.placements p WHERE state='Committed' AND NOT EXISTS (SELECT 1 FROM inventory.operation_receipts r WHERE r.operation_id=p.operation_id || '/consume')) + (SELECT count(*) FROM exchange.settlements s WHERE state='committed' AND kind='listing' AND NOT EXISTS (SELECT 1 FROM exchange.listings l WHERE l.id=s.subject_id AND l.state='sold'))" $db).Trim()
    if ($bad -ne '0') { throw "Cross-store conservation violations: $bad" }
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(30000) | Out-Null; $hostProcess.Refresh()
    $hostProcess = $null
    $currentStep = 'consistent five-store backup'
    $versions = @{inventory=2;character_state=3;housing=5;exchange=3;terrain_delta=1}
    foreach ($schema in $versions.Keys) {
        if ((Sql "SELECT version FROM $schema.schema_version WHERE singleton=true" $db).Trim() -ne [string]$versions[$schema]) { throw "Unsupported source schema: $schema" }
    }
    $before = TableSnapshot $db
    $dump = Join-Path $EvidenceRoot 'five-stores.dump'
    [void](Invoke-Checked (Join-Path $pg 'pg_dump.exe') @('-Fc','--no-owner','--no-acl','--schema=inventory','--schema=character_state','--schema=housing','--schema=exchange','--schema=terrain_delta','-h','127.0.0.1','-p',"$port",'-U','fixture_admin','-d',$db,'-f',$dump) @{ PGPASSWORD=$adminPassword })
    $after = TableSnapshot $db
    if (($before | ConvertTo-Json -Compress) -cne ($after | ConvertTo-Json -Compress)) { throw 'Source changed during backup.' }
    $backupHash = (Get-FileHash $dump -Algorithm SHA256).Hash.ToLowerInvariant()
    $before | ConvertTo-Json | Set-Content (Join-Path $EvidenceRoot 'backup-point.json')
    $results.Add((Invoke-Checked $dotnet @($hostDll,'fixture-rehearsal-after-backup') @{ GAMEPLAY_LEDGER_CONNECTION=$connection; GAMEPLAY_EXCHANGE_CONNECTION=$connection; GAMEPLAY_HOST_PORT="$hostPort"; SF_GAMEPLAY_TOKEN=$token }))
    if ((TableSnapshot $db)['exchange.wallet_entries'] -ceq $before['exchange.wallet_entries']) { throw 'Post-backup write did not change source.' }
    $currentStep = 'fresh owned five-store restore'
    $restoreData = Join-Path $private 'restore-data'
    $restorePortListener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0); $restorePortListener.Start(); $restorePort = $restorePortListener.LocalEndpoint.Port; $restorePortListener.Stop()
    $pwfile = Join-Path $private 'restore-password'; [IO.File]::WriteAllText($pwfile,$adminPassword)
    [void](Invoke-Checked (Join-Path $pg 'initdb.exe') @('-D',$restoreData,'-U','fixture_admin','--pwfile',$pwfile,'--auth','scram-sha-256','--encoding','UTF8','--no-locale'))
    Remove-Item -LiteralPath $pwfile -Force
    [IO.File]::AppendAllText((Join-Path $restoreData 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$restorePort`nfsync=on`nsynchronous_commit=on`n")
    [IO.File]::WriteAllLines((Join-Path $restoreData 'pg_hba.conf'),@('host all fixture_admin 127.0.0.1/32 scram-sha-256','host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    [void](Invoke-Checked (Join-Path $pg 'pg_ctl.exe') @('-D',$restoreData,'-l',(Join-Path $private 'restore-postgres.log'),'-w','-t','30','start'))
    $restoreStarted = $true
    $sourcePort = $port; $port = $restorePort
    $restoredDb = 'restored_' + $run
    [void](Sql "CREATE ROLE gameplay_owner NOLOGIN; CREATE DATABASE $restoredDb; GRANT CREATE ON DATABASE $restoredDb TO gameplay_owner;")
    [void](Invoke-Checked (Join-Path $pg 'pg_restore.exe') @('--exit-on-error','--no-owner','--no-acl','--role=gameplay_owner','-h','127.0.0.1','-p',"$restorePort",'-U','fixture_admin','-d',$restoredDb,$dump) @{ PGPASSWORD=$adminPassword })
    if ((Get-FileHash $dump -Algorithm SHA256).Hash.ToLowerInvariant() -cne $backupHash) { throw 'Dump hash changed.' }
    $restored = TableSnapshot $restoredDb
    if (($before | ConvertTo-Json -Compress) -cne ($restored | ConvertTo-Json -Compress)) { throw 'Restored state differs from backup point.' }
    $restoredAcknowledged = Assert-Receipts $restoredDb $receiptFile $backupAcknowledgements
    if ($restoredAcknowledged -ne $acknowledged) { throw 'Restored acknowledgement count differs from recovery.' }
    foreach ($schema in $versions.Keys) {
        $restoreConnection = "Host=127.0.0.1;Port=$restorePort;Database=$restoredDb;Username=fixture_admin;Password=$adminPassword;Pooling=true;Maximum Pool Size=64;Minimum Pool Size=8;Timeout=2;Command Timeout=5"
        $schemaEnvironment = @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$restoreConnection; GAMEPLAY_CHARACTER_CONNECTION=$restoreConnection; GAMEPLAY_HOUSING_CONNECTION=$restoreConnection; GAMEPLAY_EXCHANGE_CONNECTION=$restoreConnection; SF_GAMEPLAY_TOKEN=$token; REHEARSAL_SCHEMA=$schema }
        if ($schema -eq 'terrain_delta') {
            $savedPgPassword = $env:PGPASSWORD
            $env:PGPASSWORD = $adminPassword
            try {
                & (Join-Path $repo 'tools/Backup-ServiceStore.ps1') -Store TerrainDelta -OutputDirectory (Join-Path $EvidenceRoot 'terrain-delta-verified') -HostName '127.0.0.1' -Port $restorePort -Database $restoredDb -Username fixture_admin | Out-Null
                [void](Sql 'UPDATE terrain_delta.schema_version SET version=2 WHERE singleton=true' $restoredDb)
                $rejected = $false
                try {
                    & (Join-Path $repo 'tools/Backup-ServiceStore.ps1') -Store TerrainDelta -OutputDirectory (Join-Path $EvidenceRoot 'terrain-delta-newer') -HostName '127.0.0.1' -Port $restorePort -Database $restoredDb -Username fixture_admin | Out-Null
                } catch {
                    if ($_.Exception.Message -notmatch 'Unsupported source schema version') { throw }
                    $rejected = $true
                }
                if (-not $rejected) { throw 'TerrainDelta newer schema was accepted.' }
                [void](Sql 'UPDATE terrain_delta.schema_version SET version=1 WHERE singleton=true' $restoredDb)
            } finally { $env:PGPASSWORD = $savedPgPassword }
            continue
        }
        [void](Invoke-Checked $dotnet @($hostDll,'fixture-rehearsal-schema-baseline') $schemaEnvironment)
        $nextVersion = [int]$versions[$schema] + 1
        [void](Sql "UPDATE $schema.schema_version SET version=$nextVersion WHERE singleton=true" $restoredDb)
        [void](Invoke-Checked $dotnet @($hostDll,'fixture-rehearsal-schema-newer') $schemaEnvironment)
        [void](Sql "UPDATE $schema.schema_version SET version=$($versions[$schema]) WHERE singleton=true" $restoredDb)
    }
    $port = $sourcePort
    $results.Add("PASS $backupAcknowledgements client acknowledgements ($acknowledged distinct receipts) survived crash and restore; $(@(Get-Content -LiteralPath $receiptFile).Count - $backupAcknowledgements) post-backup acknowledgements absent")
    $results.Add('PASS one five-schema backup, later write excluded, table digests match, newer schema refused per store')
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
    $port = $primaryPort
    foreach ($key in @('SF_GAMEPLAY_TOKEN','SF_GAMEPLAY_FAULT_AFTER_REMOVE_GRANT','SF_GAMEPLAY_FAULT_AFTER_PLACE_INTENT','SF_GAMEPLAY_FAULT_AFTER_QUEST_GRANT','SF_GAMEPLAY_FAULT_AFTER_LISTING_ESCROW','GAMEPLAY_LEDGER_CONNECTION','GAMEPLAY_CHARACTER_CONNECTION','GAMEPLAY_HOUSING_CONNECTION')) {
        Remove-Item "Env:$key" -ErrorAction SilentlyContinue
    }
    try {
        if ($hostProcess -and -not $hostProcess.HasExited) { Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(30000) | Out-Null; $hostProcess.Refresh() }
        if ($hostProcess2 -and -not $hostProcess2.HasExited) { Stop-Process -Id $hostProcess2.Id -Force; $hostProcess2.WaitForExit(10000) | Out-Null }
        if ($restoreStarted -and (Test-Path -LiteralPath $restoreData)) {
            $restorePid = Join-Path $restoreData 'postmaster.pid'
            $restoreLines = @(Get-Content -LiteralPath $restorePid -TotalCount 4)
            if ($restoreLines.Count -lt 4 -or [IO.Path]::GetFullPath($restoreLines[1]) -ne [IO.Path]::GetFullPath($restoreData) -or [int]$restoreLines[3] -ne $restorePort) { throw 'Restore cluster identity mismatch.' }
            [void](Invoke-Checked (Join-Path $pg 'pg_ctl.exe') @('-D',$restoreData,'-m','immediate','-w','-t','30','stop'))
        }
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

