[CmdletBinding()]
param([switch]$Include50,[switch]$Include100,[switch]$Only10,[switch]$Only25,[switch]$Ramp,[ValidateRange(0,300)][int]$WarmupSeconds=30,[ValidateRange(1,7200)][int]$MeasuredSeconds=900,[ValidateRange(100,5000)][int]$ThinkMilliseconds=1000,[ValidateRange(25,800)][int]$RampCap=800,[ValidateRange(0,800)][int]$RampSingleParticipants=0)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($RampSingleParticipants -ne 0 -and (-not $Ramp -or $RampSingleParticipants -lt 25)) { throw 'RampSingleParticipants requires -Ramp and a value from 25 to 800.' }
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& (Join-Path $repo 'tools/Assert-InventoryOutboxMigration.ps1') -MigrationPath (Join-Path $repo 'BackendServices/InventoryLedger.Postgres/Schema.Outbox.sql') | Out-Null
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/Persistence/GameplayHosts'))
if ($Ramp -and $RampSingleParticipants -eq 0) {
    $searchRoot = Join-Path $allowed ('capacity-ramp-' + [guid]::NewGuid().ToString('N'))
    [void](New-Item -ItemType Directory -Path $searchRoot)
    $queue = @(25,50,100,200,400,800) | Where-Object { $_ -le $RampCap }
    $steps = @(); $lastPass = 0; $firstFail = 0; $stepNumber = 0
    while ($queue.Count -gt 0) {
        $tier = $queue[0]; $queue = @($queue | Select-Object -Skip 1); $stepNumber++
        $stdout = Join-Path $searchRoot "step-$stepNumber.out"; $stderr = Join-Path $searchRoot "step-$stepNumber.err"
        $child = Start-Process -FilePath (Join-Path $PSHOME 'pwsh.exe') -ArgumentList @('-NoProfile','-File',$PSCommandPath,'-Ramp','-RampSingleParticipants',"$tier",'-WarmupSeconds',"$WarmupSeconds",'-MeasuredSeconds',"$MeasuredSeconds",'-ThinkMilliseconds',"$ThinkMilliseconds") -PassThru -Wait -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $output = Get-Content $stdout -Raw
        if ($output -match '(?m)^Evidence: (.+)$') {
            $childRoot = $Matches[1].Trim() | Split-Path -Parent
            $gate = Get-Content (Join-Path $childRoot "gate-$tier-step1.json") -Raw | ConvertFrom-Json
            $resource = Get-Content (Join-Path $childRoot "resource-$tier-step1.json") -Raw | ConvertFrom-Json
            $errors = ($gate.rpcs | Measure-Object errors -Sum).Sum
            $pass = $resource.pass -and $gate.audit.passed -and $errors -eq 0
            $steps += [pscustomobject]@{ participants=$tier; throughput=[math]::Round($gate.acceptedPerSecond,2); p95=$resource.worstP95Ms; errors=$errors; audit=$gate.audit.passed; hostCpu=[math]::Round($resource.hostCpuPct,1); postgresCpu=[math]::Round($resource.postgresCpuPct,1); connections=$resource.connections; waits=$resource.topWaits; pass=$pass; root=$childRoot }
            Write-Host "step=$stepNumber participants=$tier p95=$($resource.worstP95Ms) errors=$errors audit=$($gate.audit.passed) pass=$pass throughput=$([math]::Round($gate.acceptedPerSecond,2))/s"
        } else {
            $pass = $false
            $steps += [pscustomobject]@{ participants=$tier; throughput='n/a'; p95='n/a'; errors='setup failure'; audit='not run'; hostCpu='n/a'; postgresCpu='n/a'; connections='n/a'; waits='see stderr'; pass=$false; root=$stderr }
            Write-Host "step=$stepNumber participants=$tier setup failed; see $stderr"
        }
        if ($pass) { $lastPass = $tier } else { $firstFail = $tier }
        if ($firstFail -gt 0) {
            if ($lastPass -gt 0 -and ($firstFail - $lastPass) / $lastPass -gt 0.1) { $queue = @([int][math]::Floor(($lastPass + $firstFail)/2)) }
            else { $queue = @() }
        }
    }
    $summary = @('# Isolated backend capacity ramp','',"Fixed target: one cycle per participant per $ThinkMilliseconds ms; warmup $WarmupSeconds s; measurement $MeasuredSeconds s. Each step has a fresh PostgreSQL cluster, database, host, and run-private output.",'','| Participants | Accepted/s | Worst durable p95 ms | Errors | Audit | Host CPU % | PostgreSQL CPU % | Peak DB connections | Top waits | Pass | Evidence |','| ---: | ---: | ---: | ---: | --- | ---: | ---: | ---: | --- | --- | --- |')
    foreach ($row in $steps) { $summary += "| $($row.participants) | $($row.throughput) | $($row.p95) | $($row.errors) | $($row.audit) | $($row.hostCpu) | $($row.postgresCpu) | $($row.connections) | $($row.waits) | $($row.pass) | $($row.root) |" }
    $interval = if ($firstFail -eq 0) { 'not reached' } elseif ($lastPass -eq 0) { 'below first tier' } else { "$([math]::Round(100*($firstFail-$lastPass)/$lastPass,1))%" }
    $summary += "`nLargest observed passing tier: $lastPass; first failing tier: $firstFail; interval $interval."
    $summary | Set-Content (Join-Path $searchRoot 'report.md')
    Write-Host "Evidence: $(Join-Path $searchRoot 'report.md')"
    exit 0
}
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
    if ($Ramp) {
        # The release harness admits at most 100; widen only this run-private copy.
        $overlay = Join-Path $private 'capacity-overlay'
        [void](New-Item -ItemType Directory -Path $overlay)
        $source = (Get-Content (Join-Path $repo 'BackendServices/CapacityHarness/Program.cs') -Raw).Replace('"--participants", 10, 1, 100','"--participants", 10, 1, 800')
        $source = $source.Replace('var rng = new Random(unchecked(seed * 397 ^ index));', '')
        $source = $source.Replace('var x = (east ? 1150 : -1150) + (east ? -1 : 1) * (slot % 5) * 55;', 'var globalSlot = int.Parse(Environment.GetEnvironmentVariable("CAPACITY_CLAIM_OFFSET") ?? "0") + slot; var physicalSlot = globalSlot + (globalSlot >= 172 ? 2 : 0) + (globalSlot >= 212 ? 2 : 0); var x = -1150 + (physicalSlot % 42) * 55;')
        $source = $source.Replace('var y = (north ? 1100 : -1100) + (north ? -1 : 1) * (slot / 5) * 55;', 'var band = physicalSlot / 42; var y = band < 10 ? -1150 + band * 55 : 655 + (band - 10) * 55;')
        $source = $source.Replace('var cycle = 0;', 'var cycle = 0; var nextCycleAt = DateTimeOffset.UtcNow;')
        $source = $source.Replace('await Task.Delay(think + rng.Next(-think / 4, think / 4 + 1));', 'nextCycleAt = nextCycleAt.AddMilliseconds(think); await Task.Delay(Math.Max(0, (int)(nextCycleAt - DateTimeOffset.UtcNow).TotalMilliseconds));')
        [IO.File]::WriteAllText((Join-Path $overlay 'Program.cs'),$source)
        Copy-Item (Join-Path $repo 'BackendServices/CapacityHarness/JourneySeed.cs') $overlay
        Copy-Item (Join-Path $repo 'BackendServices/CapacityHarness/packages.lock.json') $overlay
        $hostProject = Join-Path $repo 'BackendServices/Gameplay.Host/Gameplay.Host.csproj'
        Copy-Item (Join-Path $repo 'BackendServices/Directory.Build.props') $overlay
        Copy-Item (Join-Path $repo 'BackendServices/Directory.Packages.props') $overlay
        [IO.File]::WriteAllText((Join-Path $overlay 'CapacityHarness.csproj'),"<Project Sdk=`"Microsoft.NET.Sdk`"><PropertyGroup><OutputType>Exe</OutputType></PropertyGroup><ItemGroup><ProjectReference Include=`"$hostProject`" /><PackageReference Include=`"Grpc.Net.Client`" /></ItemGroup></Project>")
        $projects = @(Join-Path $overlay 'CapacityHarness.csproj')
    }
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
    # Disposable single-box capacity fixture: enough backend slots for the
    # declared host pools plus the direct fixture client and sampler.
    $postgresMaxConnections = 160
    if ($postgresMaxConnections -lt 100 -or $postgresMaxConnections -gt 256) { throw 'Fixture connection bound outside 100..256.' }
    [IO.File]::AppendAllText((Join-Path $data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$port`nmax_connections=$postgresMaxConnections`nfsync=on`nsynchronous_commit=on`nfull_page_writes=on`n")
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
    # The load generator seeds and audits with its own process pool; the host
    # applies its separate per-service limits to the same topology/credentials.
    $connection = "Host=127.0.0.1;Port=$port;Database=gameplay_$run;Username=gameplay_svc;Password=$servicePassword;Pooling=true;Maximum Pool Size=64;Minimum Pool Size=0;Timeout=2;Command Timeout=5;Include Error Detail=false"
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
    $tiers = if ($RampSingleParticipants) { @($RampSingleParticipants) } elseif ($Ramp) { @(25,50,100,200,400,800) | Where-Object { $_ -le $RampCap } } elseif ($Only10) { @(10) } elseif ($Only25) { @(25) } else { @(10,25) }
    $tiers = @($tiers)
    if (-not $Ramp -and ($Include50 -or $Include100)) { $tiers += 50 }
    if (-not $Ramp -and $Include100) { $tiers += 100 }
    $gateResults = @()
    $step = 0; $lastPass = 0; $firstFail = 0; $claimOffset = 0
    while ($tiers.Count -gt 0) {
        $tier = $tiers[0]; $tiers = @($tiers | Select-Object -Skip 1)
        $step++
        $currentStep = "capacity $tier"
        $outFile = Join-Path $EvidenceRoot "gate-$tier-step$step.json"
        $seed = 20261021 + $step
        $env:CAPACITY_OUTPUT = $outFile
        $env:GAMEPLAY_HOST_PORT = "$hostPort"
        $env:GAMEPLAY_LEDGER_CONNECTION = $connection
        $env:SF_GAMEPLAY_TOKEN = $token
        if ($Ramp) { $env:CAPACITY_CLAIM_OFFSET = "$claimOffset" }
        $stdout = Join-Path $EvidenceRoot "gate-$tier-step$step.out"
        $stderr = Join-Path $EvidenceRoot "gate-$tier-step$step.err"
        $child = Start-Process -FilePath $dotnet -ArgumentList @($journeyDll,'--participants',"$tier",'--warmup-seconds',"$WarmupSeconds",'--seconds',"$MeasuredSeconds",'--think-ms',"$ThinkMilliseconds",'--seed',"$seed") -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $samples = @(); $startedAt = Get-Date; $priorAt = $startedAt; $priorHostCpu = $hostProcess.TotalProcessorTime.TotalSeconds; $priorPgCpu = @{}
        try {
            while (-not $child.HasExited) {
                Start-Sleep -Seconds 5
                if (((Get-Date) - $startedAt).TotalSeconds -gt ($WarmupSeconds + $MeasuredSeconds + 120)) { Stop-Process -Id $child.Id -Force; throw "Capacity child timeout at $tier" }
                $stats = Sql "SELECT json_build_object('connections',(SELECT count(*) FROM pg_stat_activity WHERE datname=current_database()),'services',coalesce((SELECT json_object_agg(service,n) FROM (SELECT coalesce(nullif(application_name,''),'unlabeled') service,count(*) n FROM pg_stat_activity WHERE datname=current_database() GROUP BY 1) s),'{}'::json),'waits',coalesce((SELECT json_object_agg(wait,n) FROM (SELECT concat(wait_event_type,':',wait_event) wait,count(*) n FROM pg_stat_activity WHERE datname=current_database() AND wait_event IS NOT NULL GROUP BY 1) w),'{}'::json))" "gameplay_$run"
                $db = $stats | ConvertFrom-Json
                $pgIds = @((Sql "SELECT pid FROM pg_stat_activity WHERE datname=current_database()" "gameplay_$run") -split '\r?\n' | ForEach-Object { if ($_ -match '^\d+$') { [int]$_ } })
                $pgCpuDelta = 0.0; $currentPgCpu = @{}
                foreach ($pgId in $pgIds) { $p = Get-Process -Id $pgId -ErrorAction SilentlyContinue; if ($p) { $cpu = $p.TotalProcessorTime.TotalSeconds; if ($priorPgCpu.ContainsKey($pgId)) { $pgCpuDelta += [math]::Max(0,$cpu-$priorPgCpu[$pgId]) }; $currentPgCpu[$pgId] = $cpu } }
                $hostProcess.Refresh(); $now = Get-Date; $elapsed = ($now - $priorAt).TotalSeconds
                $hostCpu = $hostProcess.TotalProcessorTime.TotalSeconds
                $samples += [pscustomobject]@{ hostCpuPct=[math]::Round(100*($hostCpu-$priorHostCpu)/$elapsed/[Environment]::ProcessorCount,1); postgresCpuPct=[math]::Round(100*$pgCpuDelta/$elapsed/[Environment]::ProcessorCount,1); connections=$db.connections; services=$db.services; waits=$db.waits }
                $priorAt=$now; $priorHostCpu=$hostCpu; $priorPgCpu=$currentPgCpu
            }
            $child.WaitForExit()
        } finally { if (-not $child.HasExited) { Stop-Process -Id $child.Id -Force }; Remove-Item Env:CAPACITY_OUTPUT -ErrorAction SilentlyContinue }
        $result = Get-Content $stdout -Raw
        if (-not (Test-Path -LiteralPath $outFile)) { throw "Capacity child did not write $outFile`: $result" }
        $gate = Get-Content $outFile -Raw | ConvertFrom-Json
        $durable = @($gate.rpcs | Where-Object { $_.rpc -notin @('Progression.GetCharacterState','Exchange.Wallet','cycle') })
        $maxP95 = ($durable | Measure-Object -Property p95Ms -Maximum).Maximum
        $errors = ($gate.rpcs | Measure-Object -Property errors -Sum).Sum
        $pass = $gate.audit.passed -and $errors -eq 0 -and $maxP95 -le 250 -and $child.ExitCode -eq 0
        $waits = @{}; foreach ($sample in $samples) { if ($sample.waits) { foreach ($name in $sample.waits.PSObject.Properties.Name) { $waits[$name] = [int]$waits[$name] + [int]$sample.waits.$name } } }
        $topWaits = @($waits.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 3 | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ', '
        $servicePeaks = @{}; foreach ($sample in $samples) { if ($sample.services) { foreach ($name in $sample.services.PSObject.Properties.Name) { $servicePeaks[$name] = [math]::Max([int]$servicePeaks[$name], [int]$sample.services.$name) } } }
        $gate | Add-Member -NotePropertyName resource -NotePropertyValue ([pscustomobject]@{ hostCpuPct=(@($samples | Measure-Object hostCpuPct -Average)[0].Average); postgresCpuPct=(@($samples | Measure-Object postgresCpuPct -Average)[0].Average); connections=(@($samples | Measure-Object connections -Maximum)[0].Maximum); serviceConnectionPeaks=$servicePeaks; topWaits=$topWaits; pass=$pass; worstP95Ms=$maxP95 })
        $gate.resource | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $EvidenceRoot "resource-$tier-step$step.json")
        $gateResults += $gate
        if ($Ramp) { $claimOffset += [int]($gate.roles.'build') }
        Write-Host $result
        Write-Host "step=$step participants=$tier p95=$maxP95 errors=$errors pass=$pass throughput=$([math]::Round($gate.acceptedPerSecond,2))/s"
        if ($Ramp) {
            if ($pass) { $lastPass = $tier } else { $firstFail = $tier }
            if ($firstFail -gt 0) {
                if ($lastPass -gt 0 -and ($firstFail - $lastPass) / $lastPass -gt 0.1) { $tiers = @([int][math]::Floor(($lastPass + $firstFail)/2)) }
                else { $tiers = @() }
            }
        } elseif (-not $gate.audit.passed) { break }
        if ($Ramp -and $tiers.Count -gt 0) {
            Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null
            $hostProcess = Start-Process -FilePath $dotnet -ArgumentList @($hostDll) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private "host-$step.out") -RedirectStandardError (Join-Path $private "host-$step.err")
            $ready = $false
            for ($i=0; $i -lt 100; $i++) { if ($hostProcess.HasExited) { throw 'Gameplay host exited before ready.' }; if ((Test-Path (Join-Path $private "host-$step.out")) -and (Get-Content (Join-Path $private "host-$step.out") -Raw) -match 'GAMEPLAY_HOST_READY') { $ready=$true; break }; Start-Sleep -Milliseconds 100 }
            if (-not $ready) { throw 'Gameplay host restart readiness timeout.' }
        }
    }
    $spec = Get-CimInstance Win32_ComputerSystem
    $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
    $report = @("# Backend capacity evidence - $(Get-Date -Format 'yyyy-MM-dd')",'',"Machine: $($cpu.Name); $($spec.TotalPhysicalMemory / 1GB -as [int]) GiB RAM; Windows $([Environment]::OSVersion.Version); .NET 10.0.402; PostgreSQL 18.6; loopback.",'',"Workload: $((@($gateResults | ForEach-Object participants)) -join ', ') participants; 40% gather/craft, 20% build/edit, 20% fight/quest, 10% trade/market, 10% reconnect. Warmup $WarmupSeconds s; measured $MeasuredSeconds s per tier; deterministic per-step seeds from 20261021; $ThinkMilliseconds ms think time; 25 ms delay before and after each RPC to simulate 50 ms RTT. Target density: one character per participant, one backpack each, one claim per builder and one escrow per trader. No shared contested scene.",'','Backend only: no Unreal dedicated server, graphics, movement, network replication, or real-client load. The 50 ms delay is simulated on loopback, not measured network RTT; these results cannot establish the full MVP capacity gate.','', '| Participants | RPC | Calls | p50 ms | p95 ms | p99 ms | Errors | Error rate | Unavailable | Unavailable rate | Accepted/s | Integrity |','| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |')
    foreach ($gate in $gateResults) { foreach ($rpc in $gate.rpcs) { $report += "| $($gate.participants) | $($rpc.rpc) | $($rpc.calls) | $($rpc.p50Ms) | $($rpc.p95Ms) | $($rpc.p99Ms) | $($rpc.errors) | $($rpc.errorRate) | $($rpc.unavailable) | $($rpc.unavailableRate) | $([math]::Round($gate.acceptedPerSecond,2)) | $($gate.audit.passed) |" } }
    if ($Ramp) {
        $report += @('', '| Participants | Accepted/s | Worst durable p95 ms | Errors | Audit | Host CPU % | PostgreSQL CPU % | Peak DB connections | Top wait samples | Pass |', '| ---: | ---: | ---: | ---: | --- | ---: | ---: | ---: | --- | --- |')
        foreach ($gate in $gateResults) { $e = ($gate.rpcs | Measure-Object errors -Sum).Sum; $res = $gate.resource; $report += "| $($gate.participants) | $([math]::Round($gate.acceptedPerSecond,2)) | $($res.worstP95Ms) | $e | $($gate.audit.passed) | $([math]::Round($res.hostCpuPct,1)) | $([math]::Round($res.postgresCpuPct,1)) | $($res.connections) | $($res.topWaits) | $($res.pass) |" }
        if ($RampSingleParticipants) { $report += "`nIsolated probe at $RampSingleParticipants participants; this report alone does not establish a knee." }
        else { $report += "`nKnee: largest observed passing tier $lastPass participants; first failing tier $firstFail; ramp cap $RampCap. A zero first-failing tier means the knee was not reached." }
    }
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
    foreach ($key in @('SF_GAMEPLAY_TOKEN','GAMEPLAY_LEDGER_CONNECTION','GAMEPLAY_CHARACTER_CONNECTION','GAMEPLAY_HOUSING_CONNECTION','GAMEPLAY_EXCHANGE_CONNECTION','GAMEPLAY_CONTENT_ROOT','GAMEPLAY_HOST_PORT','CAPACITY_CLAIM_OFFSET','CAPACITY_OUTPUT')) { Remove-Item "Env:$key" -ErrorAction SilentlyContinue }
    if ($hostProcess -and -not $hostProcess.HasExited) { Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(10000) | Out-Null }
    if (Test-Path (Join-Path $private 'owner.json')) {
        Assert-Owned
        if ($started -or ($attempted -and (Test-Path (Join-Path $data 'postmaster.pid')))) { [void](Invoke-Checked (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-m','immediate','-w','-t','30','stop')) }
        Assert-Owned
        Remove-Item -LiteralPath $private -Recurse -Force
    }
}
if ($failure -ne '') { throw $failure }
