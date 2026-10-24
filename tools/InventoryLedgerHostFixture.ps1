# Dot-sourced by Test-GatheringPilot.ps1 -Ledger: owned loopback PostgreSQL + InventoryLedger.Host + SQL/get helpers.
# Provisioning mirrors Test-InventoryLedgerDurable.ps1 (pinned hashes, SCRAM, loopback hba, ledger_owner/ledger_svc).
# State lives in one hashtable ($fx) so the caller's `finally` can always call Remove-LedgerFixture.

function Invoke-FxChecked([hashtable]$Fx,[string]$Executable,[string[]]$Arguments,[hashtable]$Environment = @{}) {
    $info = [Diagnostics.ProcessStartInfo]::new(); $info.FileName = $Executable; $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true
    $info.Arguments = (($Arguments | ForEach-Object { '"' + $_.Replace('"','\"') + '"' }) -join ' ')
    foreach ($key in @($info.EnvironmentVariables.Keys)) { if ($key -match '^PG') { $info.EnvironmentVariables.Remove($key) } }
    foreach ($key in $Environment.Keys) { $info.EnvironmentVariables[$key] = $Environment[$key] }
    $child = [Diagnostics.Process]::new(); $child.StartInfo = $info; [void]$child.Start()
    $out = $child.StandardOutput.ReadToEndAsync(); $err = $child.StandardError.ReadToEndAsync()
    if (-not $child.WaitForExit(120000)) { $child.Kill(); throw 'Owned child timeout.' }
    $stdout = if ($out.Wait(1000)) { $out.Result } else { '' }
    $stderr = if ($err.Wait(1000)) { $err.Result } else { '' }
    foreach ($secret in $Fx.secrets) { $stdout = $stdout.Replace($secret,'[REDACTED]'); $stderr = $stderr.Replace($secret,'[REDACTED]') }
    $code = $child.ExitCode
    return [pscustomobject]@{ ExitCode=$code; Stdout=$stdout.Trim(); Stderr=$stderr.Trim() }
}
function Invoke-FxOk([hashtable]$Fx,[string]$Executable,[string[]]$Arguments,[hashtable]$Environment = @{}) {
    $r = Invoke-FxChecked $Fx $Executable $Arguments $Environment
    if ($r.ExitCode -ne 0) { throw "$Executable exited $($r.ExitCode): $($r.Stderr) $($r.Stdout)" }
    return $r.Stdout
}
function Get-FreeTcpPort {
    $l = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0); $l.Start(); $p = $l.LocalEndpoint.Port; $l.Stop(); return $p
}
function New-HexSecret { $b = [byte[]]::new(32); $r = [Security.Cryptography.RandomNumberGenerator]::Create(); $r.GetBytes($b); $r.Dispose(); return [BitConverter]::ToString($b).Replace('-','').ToLowerInvariant() }

function Invoke-LedgerSql([hashtable]$Fx,[string]$Query,[string]$Database = 'postgres') {
    $file = Join-Path $Fx.private ('sql-' + [guid]::NewGuid().ToString('N') + '.sql')
    [IO.File]::WriteAllText($file,$Query)
    try { return Invoke-FxOk $Fx (Join-Path $Fx.pg 'psql.exe') @('-X','-q','-A','-t','-h','127.0.0.1','-p',"$($Fx.pgPort)",'-U','fixture_admin','-d',$Database,'-v','ON_ERROR_STOP=1','-f',$file) @{ PGPASSWORD=$Fx.adminPassword; PGCONNECT_TIMEOUT='5' } }
    finally { Remove-Item -LiteralPath $file -Force -ErrorAction SilentlyContinue }
}

# Provisions PostgreSQL and builds the host, filling the caller-owned $Fx hashtable so cleanup works after a partial failure.
function New-LedgerFixture([hashtable]$Fx,[string]$RepoRoot,[string]$EvidenceRoot) {
    & (Join-Path $RepoRoot 'tools/Assert-InventoryOutboxMigration.ps1') -MigrationPath (Join-Path $RepoRoot 'BackendServices/InventoryLedger.Postgres/Schema.Outbox.sql') | Out-Null
    $Fx.repo=$RepoRoot; $Fx.evidence=$EvidenceRoot; $Fx.started=$false; $Fx.hostProcess=$null; $Fx.hostPort=0; $Fx.secrets=@()
    $pins = Get-Content (Join-Path $RepoRoot 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
    # A worktree can reuse the pinned, ignored toolchain from the main checkout.
    $toolchainRoot = if ($env:SF_FIXTURE_TOOLCHAIN_ROOT) { [IO.Path]::GetFullPath($env:SF_FIXTURE_TOOLCHAIN_ROOT) } else { $RepoRoot }
    $Fx.dotnet = Join-Path $toolchainRoot 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
    $Fx.nuget = Join-Path $toolchainRoot 'Artifacts/Persistence/Toolchains/nuget'
    $Fx.pg = Join-Path $toolchainRoot 'Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
    $Fx.project = Join-Path $RepoRoot 'BackendServices/InventoryLedger.Host/InventoryLedger.Host.csproj'
    $Fx.dll = Join-Path $EvidenceRoot 'private/host-build/InventoryLedger.Host.dll'
    $schema = Join-Path $RepoRoot 'BackendServices/InventoryLedger.Postgres/Schema.sql'
    foreach ($path in @($Fx.dotnet,$Fx.project,$schema,(Join-Path $Fx.pg 'postgres.exe'),(Join-Path $Fx.pg 'initdb.exe'),(Join-Path $Fx.pg 'pg_ctl.exe'),(Join-Path $Fx.pg 'psql.exe'))) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file missing: $path" } }
    if ((Get-FileHash $Fx.dotnet -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.sdk.executableSha256 -or (Get-FileHash (Join-Path $Fx.pg 'postgres.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.postgres.executableSha256) { throw 'Pinned toolchain mismatch.' }
    $Fx.run = [guid]::NewGuid().ToString('N')
    $Fx.base = Join-Path $EvidenceRoot 'private/ledger-pg'
    $Fx.private = $Fx.base; $Fx.data = Join-Path $Fx.base 'data'
    $Fx.adminPassword = New-HexSecret; $Fx.servicePassword = New-HexSecret; $Fx.token = New-HexSecret
    $Fx.secrets = @($Fx.adminPassword,$Fx.servicePassword,$Fx.token)
    $Fx.db = 'ledger_' + $Fx.run
    $null = New-Item -ItemType Directory -Path $Fx.base
    $account = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    [void](Invoke-FxOk $Fx 'icacls.exe' @($Fx.base,'/inheritance:r','/grant:r',"${account}:(OI)(CI)F"))
    $Fx.pgPort = Get-FreeTcpPort
    # Build once with the pinned SDK (locked restore, Release).
    [void](Invoke-FxOk $Fx $Fx.dotnet @('restore',$Fx.project,'--locked-mode','--packages',$Fx.nuget,'--verbosity','quiet'))
    [void](Invoke-FxOk $Fx $Fx.dotnet @('build',$Fx.project,'--no-restore','--configuration','Release','--verbosity','quiet',"-p:OutDir=$(Join-Path $EvidenceRoot 'private/host-build')/"))
    if (-not (Test-Path -LiteralPath $Fx.dll)) { throw 'Host build produced no dll.' }
    $pwfile = Join-Path $Fx.base 'admin-password'; [IO.File]::WriteAllText($pwfile,$Fx.adminPassword)
    [void](Invoke-FxOk $Fx (Join-Path $Fx.pg 'initdb.exe') @('-D',$Fx.data,'-U','fixture_admin','--pwfile',$pwfile,'--auth','scram-sha-256','--encoding','UTF8','--no-locale'))
    Remove-Item -LiteralPath $pwfile -Force
    [IO.File]::AppendAllText((Join-Path $Fx.data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$($Fx.pgPort)`nfsync=on`nsynchronous_commit=on`nfull_page_writes=on`n")
    [IO.File]::WriteAllLines((Join-Path $Fx.data 'pg_hba.conf'),@('host all fixture_admin 127.0.0.1/32 scram-sha-256',"host $($Fx.db) ledger_svc 127.0.0.1/32 scram-sha-256",'host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    $Fx.attempted = $true
    [void](Invoke-FxOk $Fx (Join-Path $Fx.pg 'pg_ctl.exe') @('-D',$Fx.data,'-l',(Join-Path $Fx.base 'postgres.log'),'-w','-t','30','start'))
    $Fx.started = $true
    [void](Invoke-LedgerSql $Fx "CREATE ROLE ledger_owner NOLOGIN; CREATE ROLE ledger_svc LOGIN PASSWORD '$($Fx.servicePassword)'; CREATE DATABASE $($Fx.db);")
    [void](Invoke-LedgerSql $Fx "REVOKE ALL ON DATABASE $($Fx.db) FROM PUBLIC; GRANT CONNECT ON DATABASE $($Fx.db) TO ledger_svc; GRANT CREATE ON DATABASE $($Fx.db) TO ledger_owner; REVOKE ALL ON SCHEMA public FROM PUBLIC; CREATE SCHEMA inventory AUTHORIZATION ledger_owner;" $Fx.db)
    [void](Invoke-LedgerSql $Fx ("SET ROLE ledger_owner;`n" + (Get-Content $schema -Raw) + "`n" + (Get-Content (Join-Path $RepoRoot 'BackendServices/InventoryLedger.Postgres/Schema.Outbox.sql') -Raw) + "`n" + (Get-Content (Join-Path $RepoRoot 'BackendServices/InventoryLedger.Postgres/Schema.GatheringNodes.sql') -Raw) + "`n" + (Get-Content (Join-Path $RepoRoot 'BackendServices/InventoryLedger.Postgres/Schema.DefeatRewards.sql') -Raw)) $Fx.db)
    [void](Invoke-LedgerSql $Fx 'GRANT USAGE ON SCHEMA inventory TO ledger_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA inventory TO ledger_svc;' $Fx.db)
    $Fx.connection = "Host=127.0.0.1;Port=$($Fx.pgPort);Database=$($Fx.db);Username=ledger_svc;Password=$($Fx.servicePassword);Pooling=false;Timeout=2;Command Timeout=5;Include Error Detail=false"
    return
}

# Runs a scriptblock with temporary process env vars (inherited by Start-Process), always removing them afterwards.
function Invoke-WithEnv([hashtable]$Vars,[scriptblock]$Body) {
    foreach ($k in $Vars.Keys) { Set-Item -Path "Env:$k" -Value $Vars[$k] }
    try { & $Body } finally { foreach ($k in $Vars.Keys) { Remove-Item -Path "Env:$k" -ErrorAction SilentlyContinue } }
}

function Start-LedgerHost([hashtable]$Fx,[string]$Fault = '') {
    if ($Fx.hostProcess -and -not $Fx.hostProcess.HasExited) { throw 'Host already running.' }
    if (-not $Fx.hostPort) { $Fx.hostPort = Get-FreeTcpPort }  # callers may pre-pick it (HostDown needs the port before the host runs)
    $n = if ($Fx.Contains('hostRuns')) { $Fx.hostRuns + 1 } else { 1 }; $Fx.hostRuns = $n
    $Fx.hostOut = Join-Path $Fx.evidence "host-$n.stdout.txt"; $Fx.hostErr = Join-Path $Fx.evidence "host-$n.stderr.txt"
    $vars = @{ INVENTORY_LEDGER_PORT="$($Fx.hostPort)"; INVENTORY_LEDGER_CONNECTION=$Fx.connection; SF_INVENTORY_LEDGER_TOKEN=$Fx.token; INVENTORY_LEDGER_FAULT=$Fault
        INVENTORY_LEDGER_ALPHA_STARTER_KIT=$(if ($Fx.Contains('starterKit') -and $Fx.starterKit) { 'on' } else { '' }) }
    $hostArgs = @(('"' + $Fx.dll + '"'))
    $p = $null
    Invoke-WithEnv $vars { $script:__hp = Start-Process -FilePath $Fx.dotnet -ArgumentList $hostArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput $Fx.hostOut -RedirectStandardError $Fx.hostErr }
    $p = $script:__hp; $null = $p.Handle; $Fx.hostProcess = $p
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 30) {
        if ((Test-Path -LiteralPath $Fx.hostOut) -and (Select-String -LiteralPath $Fx.hostOut -Pattern "INVENTORY_LEDGER_HOST_READY port=$($Fx.hostPort)" -Quiet)) { return }
        if ($p.HasExited) { throw "Host exited $($p.ExitCode) before ready." }
        Start-Sleep -Milliseconds 200
    }
    throw 'Timed out waiting for host ready.'
}

function Stop-LedgerHost([hashtable]$Fx) {
    if ($Fx.hostProcess -and -not $Fx.hostProcess.HasExited) { Stop-Process -Id $Fx.hostProcess.Id -Force; $Fx.hostProcess.WaitForExit() }
    # Redact the token if it ever reached the captured output.
    foreach ($f in @($Fx.hostOut,$Fx.hostErr)) {
        if ($f -and (Test-Path -LiteralPath $f)) {
            $t = Get-Content -LiteralPath $f -Raw
            if ($t -and $t.Contains($Fx.token)) { Set-Content -LiteralPath $f -Value $t.Replace($Fx.token,'[REDACTED]') -Encoding utf8 }
        }
    }
}

# Combined stdout of every host run so far (for counting FAULT_INJECTED / GRANT lines).
function Get-LedgerHostText([hashtable]$Fx) {
    $parts = foreach ($f in (Get-ChildItem -LiteralPath $Fx.evidence -Filter 'host-*.stdout.txt' -ErrorAction SilentlyContinue)) { Get-Content -LiteralPath $f.FullName -Raw }
    return ($parts -join "`n")
}

# `host get <id>`; returns ExitCode, Line, Revision and Items (definition -> qty).
function Get-LedgerView([hashtable]$Fx,[string]$ContainerId) {
    $r = Invoke-FxChecked $Fx $Fx.dotnet @($Fx.dll,'get',$ContainerId) @{ INVENTORY_LEDGER_PORT="$($Fx.hostPort)"; SF_INVENTORY_LEDGER_TOKEN=$Fx.token }
    $view = [ordered]@{ container=$ContainerId; exitCode=$r.ExitCode; line=$r.Stdout; revision=$null; items=[ordered]@{} }
    if ($r.ExitCode -eq 0 -and $r.Stdout -match '^INVENTORY_LEDGER_VIEW container=(\S+) revision=(\d+)(.*)$') {
        $view.revision = [int64]$Matches[2]
        foreach ($m in [regex]::Matches($Matches[3],'(\S+)=(\d+)')) { $view.items[$m.Groups[1].Value] = [int64]$m.Groups[2].Value }
    }
    return $view
}

function Get-ReceiptCount([hashtable]$Fx) { return [int](Invoke-LedgerSql $Fx 'SELECT count(*) FROM inventory.operation_receipts' $Fx.db).Trim() }

function Remove-LedgerFixture([hashtable]$Fx) {
    if (-not $Fx -or -not $Fx.Contains("base")) { return }
    $failure = ''
    try { Stop-LedgerHost $Fx } catch { $failure += " host: $($_.Exception.Message)" }
    try {
        if ($Fx.Contains('attempted') -and (Test-Path -LiteralPath $Fx.data)) {
            & (Join-Path $Fx.pg 'pg_ctl.exe') -D $Fx.data status *> $null
            if ($LASTEXITCODE -eq 0) { [void](Invoke-FxOk $Fx (Join-Path $Fx.pg 'pg_ctl.exe') @('-D',$Fx.data,'-m','immediate','-w','-t','30','stop')) }
        }
        $evidence = [IO.Path]::GetFullPath($Fx.evidence).TrimEnd([IO.Path]::DirectorySeparatorChar)
        $artifacts = [IO.Path]::GetFullPath((Join-Path $Fx.repo 'Artifacts')).TrimEnd([IO.Path]::DirectorySeparatorChar)
        $docsEvidence = [IO.Path]::GetFullPath((Join-Path $Fx.repo 'docs/evidence')).TrimEnd([IO.Path]::DirectorySeparatorChar)
        $evidenceIsArtifactChild = ($evidence + [IO.Path]::DirectorySeparatorChar).StartsWith(($artifacts + [IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)
        $evidenceIsDocsChild = ($evidence + [IO.Path]::DirectorySeparatorChar).StartsWith(($docsEvidence + [IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)
        if (-not ($evidenceIsArtifactChild -or $evidenceIsDocsChild)) { throw 'Evidence root must be a child of repo/Artifacts or repo/docs/evidence.' }
        if ($evidenceIsDocsChild) {
            foreach ($path in @((Join-Path $Fx.repo 'docs'),$docsEvidence,$evidence)) {
                if ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Evidence path contains a reparse point.' }
            }
        }
        $allowed = $evidence + [IO.Path]::DirectorySeparatorChar
        $full = [IO.Path]::GetFullPath($Fx.base)
        if (-not $full.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Owned path mismatch.' }
        if (@(Get-ChildItem -LiteralPath $full -Recurse -Attributes ReparsePoint -ErrorAction SilentlyContinue).Count) { throw 'Owned tree contains a reparse point.' }
        Remove-Item -LiteralPath $full -Recurse -Force
    } catch { $failure += " cluster: $($_.Exception.Message)" }
    $Fx.cleanupFailure = $failure.Trim()
    $Fx.privateStorageRemoved = -not (Test-Path -LiteralPath $Fx.base)
}
