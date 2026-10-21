[CmdletBinding()]
param([switch]$Include50,[switch]$Include100,[switch]$Only10,[switch]$Only25,[int]$WarmupSeconds=30,[int]$MeasuredSeconds=900,[int]$ThinkMilliseconds=1000)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& (Join-Path $repo 'tools/Assert-InventoryOutboxMigration.ps1') -MigrationPath (Join-Path $repo 'BackendServices/InventoryLedger.Postgres/Schema.Outbox.sql') | Out-Null
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/Persistence/GameplayHosts'))
$EvidenceRoot = [IO.Path]::GetFullPath((Join-Path $allowed ('capacity-' + [guid]::NewGuid().ToString('N'))))
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
function Invoke-Checked([string]$Executable,[string[]]$Arguments,[hashtable]$Environment = @{},[int]$TimeoutMs = 180000,[bool]$AllowNonZero = $false) {
    $info = [Diagnostics.ProcessStartInfo]::new(); $info.FileName = $Executable; $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true
    $info.Arguments = (($Arguments | ForEach-Object { '"' + $_.Replace('"','\"') + '"' }) -join ' ')
    foreach ($key in @($info.EnvironmentVariables.Keys)) { if ($key -match '^PG|^GAMEPLAY_') { $info.EnvironmentVariables.Remove($key) } }
    foreach ($key in $Environment.Keys) { $info.EnvironmentVariables[$key] = $Environment[$key] }
    $child = [Diagnostics.Process]::new(); $child.StartInfo = $info; [void]$child.Start()
    $out = $child.StandardOutput.ReadToEndAsync(); $err = $child.StandardError.ReadToEndAsync()
    if (-not $child.WaitForExit($TimeoutMs)) {
        $child.Kill(); $child.WaitForExit(5000) | Out-Null
        $partial = if ($out.Wait(1000)) { $out.Result } else { '' }
        $partial = $partial.Replace($adminPassword,'[REDACTED]').Replace($servicePassword,'[REDACTED]')
        throw "Owned child timeout: $partial"
    }
    $stdout = if ($out.Wait(1000)) { $out.Result } else { '[output pipe held]' }
    $stderr = if ($err.Wait(1000)) { $err.Result } else { '[error pipe held]' }
    foreach ($secret in @($adminPassword,$servicePassword)) { $stdout = $stdout.Replace($secret,'[REDACTED]'); $stderr = $stderr.Replace($secret,'[REDACTED]') }
    if ($child.ExitCode -ne 0 -and -not $AllowNonZero) { throw "$Executable exited $($child.ExitCode): $stderr $stdout" }
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
    $projects = @((Join-Path $repo 'BackendServices/CapacityHarness/CapacityHarness.csproj'))
    foreach ($project in $projects) {
        $currentStep = "restore $project"
        [void](Invoke-Checked $dotnet @('restore',$project,'--locked-mode','--packages',(Join-Path $repo 'Artifacts/Persistence/Toolchains/nuget'),'--verbosity','quiet'))
        $currentStep = "build $project"
        $buildArgs = @('build',$project,'--no-restore','--configuration','Release','--verbosity','quiet',"-p:BaseOutputPath=$(Join-Path $private 'build')/")
        [void](Invoke-Checked $dotnet $buildArgs)
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
    $currentStep = 'journey seed'
    $journeyDll = Join-Path $private 'build/Release/net10.0/CapacityHarness.dll'
    [void](Invoke-Checked $dotnet @($journeyDll,'journey') @{ GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token })
    $tiers = if ($Only25) { @(25) } elseif ($Only10) { @(10) } else { @(10,25) }
    $tiers = @($tiers)
    if ($Include50 -or $Include100) { $tiers += 50 }
    if ($Include100) { $tiers += 100 }
    $gateResults = @()
    foreach ($tier in $tiers) {
        $currentStep = "capacity $tier"
        $outFile = Join-Path $EvidenceRoot "gate-$tier.json"
        $result = Invoke-Checked $dotnet @($journeyDll,'--participants',"$tier",'--warmup-seconds',"$WarmupSeconds",'--seconds',"$MeasuredSeconds",'--think-ms',"$ThinkMilliseconds") @{
            GAMEPLAY_HOST_PORT="$hostPort"; GAMEPLAY_LEDGER_CONNECTION=$connection; SF_GAMEPLAY_TOKEN=$token; CAPACITY_OUTPUT=$outFile
        } (($WarmupSeconds + $MeasuredSeconds + 120) * 1000) $true
        if (-not (Test-Path -LiteralPath $outFile)) { throw "Capacity child did not write $outFile`: $result" }
        $gateResults += Get-Content $outFile -Raw | ConvertFrom-Json
        Write-Host $result
        if (-not $gateResults[-1].audit.passed) { break }
    }
    $spec = Get-CimInstance Win32_ComputerSystem
    $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
    $report = @("# Backend capacity evidence - $(Get-Date -Format 'yyyy-MM-dd')",'',"Machine: $($cpu.Name); $($spec.TotalPhysicalMemory / 1GB -as [int]) GiB RAM; Windows $([Environment]::OSVersion.Version); .NET 10.0.402; PostgreSQL 18.6; loopback.",'',"Workload: $($tiers -join ', ') participants; 40% gather/craft, 20% build/edit, 20% fight/quest, 10% trade/market, 10% reconnect. Warmup $WarmupSeconds s; measured $MeasuredSeconds s per tier; deterministic seed 20261021; $ThinkMilliseconds ms think time; 25 ms delay before and after each RPC to simulate 50 ms RTT. Target density: one character per participant, one backpack each, one claim per builder and one escrow per trader. No shared contested scene.",'','Backend only: no Unreal dedicated server, graphics, movement, network replication, or real-client load. The 50 ms delay is simulated on loopback, not measured network RTT; these results cannot establish the full MVP capacity gate.','', '| Participants | RPC | Calls | p50 ms | p95 ms | p99 ms | Errors | Error rate | Unavailable | Unavailable rate | Accepted/s | Integrity |','| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |')
    foreach ($gate in $gateResults) { foreach ($rpc in $gate.rpcs) { $report += "| $($gate.participants) | $($rpc.rpc) | $($rpc.calls) | $($rpc.p50Ms) | $($rpc.p95Ms) | $($rpc.p99Ms) | $($rpc.errors) | $($rpc.errorRate) | $($rpc.unavailable) | $($rpc.unavailableRate) | $([math]::Round($gate.acceptedPerSecond,2)) | $($gate.audit.passed) |" } }
    $report += @('', 'Per-tier observed role allocation and backend result:')
    foreach ($gate in $gateResults) {
        $durable = @($gate.rpcs | Where-Object { $_.rpc -notin @('Progression.GetCharacterState','Exchange.Wallet','cycle') })
        $calls = ($gate.rpcs | Measure-Object -Property calls -Sum).Sum
        $errors = ($gate.rpcs | Measure-Object -Property errors -Sum).Sum
        $maxP95 = ($durable | Measure-Object -Property p95Ms -Maximum).Maximum
        $backendResult = if ($gate.audit.passed -and $calls -gt 0 -and $errors / $calls -lt 0.01 -and $maxP95 -le 250) { 'meets measured backend thresholds' } else { 'misses measured backend thresholds' }
        $report += "- $($gate.participants): roles $($gate.roles | ConvertTo-Json -Compress); $errors/$calls errors; worst durable RPC p95 $maxP95 ms; $backendResult."
    }
    $report += @('', 'Milestone comparison: durable action p95 <=250 ms at 50 ms RTT, unexpected failures <1%, 15 minutes after warmup, and zero integrity violations. The network RTT condition is unmet, so latency is informational. The 100 participant gate also requires a two-hour soak.','', 'Integrity SQL checks:')
    foreach ($gate in $gateResults) { $report += "- $($gate.participants) participants: $($gate.audit.checks | ConvertTo-Json -Compress)" }
    $evidence = Join-Path $EvidenceRoot 'report.md'
    $report | Set-Content $evidence
    Write-Host "Evidence: $evidence"
    if (@($gateResults | Where-Object { -not $_.audit.passed }).Count -gt 0) { $failure = 'Capacity integrity audit failed; see evidence.' }
}
catch {
    $failure = ($currentStep + ': ' + $_.Exception.Message).Replace($adminPassword,'[REDACTED]').Replace($servicePassword,'[REDACTED]')
    $pgLog = Join-Path $private 'postgres.log'
    if (Test-Path $pgLog) {
        $tail = (Get-Content $pgLog -Tail 30) -join "`n"
        $tail.Replace($adminPassword,'[REDACTED]').Replace($servicePassword,'[REDACTED]') | Set-Content (Join-Path $EvidenceRoot 'postgres-errors.txt')
    }
}
finally {
    foreach ($key in @('SF_GAMEPLAY_TOKEN','GAMEPLAY_LEDGER_CONNECTION','GAMEPLAY_CHARACTER_CONNECTION','GAMEPLAY_HOUSING_CONNECTION','GAMEPLAY_EXCHANGE_CONNECTION','GAMEPLAY_CONTENT_ROOT','GAMEPLAY_HOST_PORT')) { Remove-Item "Env:$key" -ErrorAction SilentlyContinue }
    if ($hostProcess -and -not $hostProcess.HasExited) { Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null }
    if (Test-Path (Join-Path $private 'owner.json')) {
        Assert-Owned
        if ($started -or ($attempted -and (Test-Path (Join-Path $data 'postmaster.pid')))) { [void](Invoke-Checked (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-m','immediate','-w','-t','30','stop')) }
        Assert-Owned
        Remove-Item -LiteralPath $private -Recurse -Force
    }
}
if ($failure -ne '') { throw $failure }
