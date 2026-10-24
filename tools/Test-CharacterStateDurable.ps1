[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/Persistence/CharacterStateDurable'))
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $EvidenceRoot)) { throw 'Supply a fresh CharacterStateDurable evidence child.' }
$pins = Get-Content (Join-Path $repo 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
$dotnet = Join-Path $repo 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
$pg = Join-Path $repo 'Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
$nuget = Join-Path $repo 'Artifacts/Persistence/Toolchains/nuget'
if (-not (Test-Path -LiteralPath $dotnet)) { $dotnet = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe' }
if (-not (Test-Path -LiteralPath (Join-Path $pg 'postgres.exe'))) { $pg = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin' }
if (-not (Test-Path -LiteralPath $nuget)) { $nuget = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/nuget' }
$project = Join-Path $repo 'BackendServices/CharacterState.Postgres.Tests/CharacterState.Postgres.Tests.csproj'
$schema = Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.sql'
$dll = Join-Path $repo 'BackendServices/CharacterState.Postgres.Tests/bin/Release/net10.0/CharacterState.Postgres.Tests.dll'
$backupTool = Join-Path $repo 'tools/Backup-ServiceStore.ps1'; $restoreTool = Join-Path $repo 'tools/Restore-ServiceStore.ps1'
$backup = Join-Path $EvidenceRoot 'backup'; $restoredInstance = Join-Path $EvidenceRoot 'restored-instance'
foreach ($path in @($dotnet,$project,$schema,(Join-Path $pg 'postgres.exe'),(Join-Path $pg 'initdb.exe'),(Join-Path $pg 'pg_ctl.exe'),(Join-Path $pg 'psql.exe'))) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file missing: $path" } }
if ((Get-FileHash $dotnet -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.sdk.executableSha256 -or (Get-FileHash (Join-Path $pg 'postgres.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.postgres.executableSha256) { throw 'Pinned toolchain mismatch.' }
$run = [guid]::NewGuid().ToString('N'); $private = Join-Path $EvidenceRoot 'private'; $data = Join-Path $private 'data'
$rng = [Security.Cryptography.RandomNumberGenerator]::Create()
$bytes = [byte[]]::new(32); $rng.GetBytes($bytes); $adminPassword = [BitConverter]::ToString($bytes).Replace('-','')
$rng.GetBytes($bytes); $servicePassword = [BitConverter]::ToString($bytes).Replace('-',''); $rng.Dispose()
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
    foreach ($secret in @($adminPassword,$servicePassword)) { $stdout = $stdout.Replace($secret,'[REDACTED]'); $stderr = $stderr.Replace($secret,'[REDACTED]') }
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
    [void](Invoke-Checked $dotnet @('restore',$project,'--locked-mode','--packages',$nuget,'--verbosity','quiet'))
    [void](Invoke-Checked $dotnet @('build',$project,'--no-restore','--configuration','Release','--verbosity','quiet'))
    $pwfile = Join-Path $private 'admin-password'; [IO.File]::WriteAllText($pwfile,$adminPassword)
    [void](Invoke-Checked (Join-Path $pg 'initdb.exe') @('-D',$data,'-U','fixture_admin','--pwfile',$pwfile,'--auth','scram-sha-256','--encoding','UTF8','--no-locale'))
    Remove-Item -LiteralPath $pwfile -Force
    [IO.File]::AppendAllText((Join-Path $data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$port`nfsync=on`nsynchronous_commit=on`nfull_page_writes=on`n")
    [IO.File]::WriteAllLines((Join-Path $data 'pg_hba.conf'),@('host all fixture_admin 127.0.0.1/32 scram-sha-256',"host state_$run state_svc 127.0.0.1/32 scram-sha-256",'host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    Cluster start
    [void](Sql "CREATE ROLE state_owner NOLOGIN; CREATE ROLE state_svc LOGIN PASSWORD '$servicePassword'; CREATE DATABASE state_$run;")
    [void](Sql "REVOKE ALL ON DATABASE state_$run FROM PUBLIC; GRANT CONNECT ON DATABASE state_$run TO state_svc; GRANT CREATE ON DATABASE state_$run TO state_owner; REVOKE ALL ON SCHEMA public FROM PUBLIC;" "state_$run")
    [void](Sql ("SET ROLE state_owner;`n" + (Get-Content $schema -Raw)) "state_$run")
    [void](Sql ("SET ROLE state_owner;`n" + (Get-Content (Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.Position.sql') -Raw)) "state_$run")
    [void](Sql ("SET ROLE state_owner;`n" + (Get-Content (Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.Recipes.sql') -Raw)) "state_$run")
    [void](Sql ("SET ROLE state_owner;`n" + (Get-Content (Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.Appearance.sql') -Raw)) "state_$run")
    [void](Sql ("SET ROLE state_owner;`n" + (Get-Content (Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.GatheringSkills.sql') -Raw)) "state_$run")
    [void](Sql ("SET ROLE state_owner;`n" + (Get-Content (Join-Path $repo 'BackendServices/CharacterState.Postgres/Schema.Death.sql') -Raw)) "state_$run")
    $migrationConnection = "Host=127.0.0.1;Port=$port;Database=state_$run;Username=fixture_admin;Password=$adminPassword;Pooling=false"
    & (Join-Path $repo 'tools/Invoke-SchemaMigrations.ps1') -Connection $migrationConnection -Schemas character_state -Role state_owner -Dotnet $dotnet -Nuget $nuget | Out-Null
    [void](Sql 'GRANT USAGE ON SCHEMA character_state TO state_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA character_state TO state_svc;' "state_$run")
    $connection = "Host=127.0.0.1;Port=$port;Database=state_$run;Username=state_svc;Password=$servicePassword;Pooling=true;Maximum Pool Size=64;Minimum Pool Size=8;Timeout=2;Command Timeout=5;Include Error Detail=false"
    $results = [Collections.Generic.List[string]]::new()
    foreach ($phase in @('seed','restart','concurrent','outage','recover','classification')) {
        if ($phase -eq 'restart' -or $phase -eq 'outage') { Cluster stop }
        if ($phase -eq 'restart' -or $phase -eq 'concurrent' -or $phase -eq 'recover') { if (-not $started) { Cluster start } }
        $results.Add((Invoke-Checked $dotnet @($dll,$phase) @{ CHARACTER_STATE_CONNECTION=$connection }))
        if ($phase -eq 'seed') {
            # The legacy domain fixture omits reward item content; give its pending intent a valid version-2 snapshot.
            [void](Sql 'UPDATE character_state.reward_receipts SET item_lines=''[{"DefinitionId":"item/ore","Quantity":1}]''::jsonb WHERE state=''Pending'' AND item_lines=''[]''::jsonb' "state_$run")
            [void](Sql "INSERT INTO character_state.positions(character_id,revision,zone_id,x,y,z,yaw) VALUES ('character-1',1,'silverbrook',12.5,-3,40,90)" "state_$run")
        }
    }
    $results.Add((Invoke-Checked 'powershell.exe' @('-NoProfile','-File',$backupTool,'-Store','CharacterState','-OutputDirectory',$backup,'-HostName','127.0.0.1','-Port',"$port",'-Database',"state_$run",'-Username','state_svc') @{ PGPASSWORD=$servicePassword }))
    $backupRevision = (Sql "SELECT revision FROM character_state.progression WHERE character_id='character-1'" "state_$run").Trim()
    [void](Sql "UPDATE character_state.progression SET revision=revision+1 WHERE character_id='character-1'" "state_$run")
    [void](Sql "UPDATE character_state.positions SET revision=revision+1,x=99 WHERE character_id='character-1'" "state_$run")
    if ((Sql "SELECT revision FROM character_state.progression WHERE character_id='character-1'" "state_$run").Trim() -ne "$([long]$backupRevision + 1)") { throw 'Post-backup write failed.' }
    $corrupt = Join-Path $EvidenceRoot 'corrupt-backup'; [void](New-Item -ItemType Directory -Path $corrupt)
    Copy-Item (Join-Path $backup 'manifest.json') $corrupt
    $corruptDump = Join-Path $corrupt 'characterstate.dump'; Copy-Item (Join-Path $backup 'characterstate.dump') $corruptDump
    $stream = [IO.File]::Open($corruptDump,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite)
    try { [void]$stream.Seek(64,[IO.SeekOrigin]::Begin); $byte=$stream.ReadByte(); [void]$stream.Seek(64,[IO.SeekOrigin]::Begin); $stream.WriteByte([byte]($byte -bxor 1)) } finally { $stream.Dispose() }
    try { [void](Invoke-Checked 'powershell.exe' @('-NoProfile','-File',$restoreTool,'-Store','CharacterState','-BackupDirectory',$corrupt,'-InstanceRoot',$restoredInstance)); throw 'Corrupt backup was accepted.' }
    catch { if ($_.Exception.Message -notmatch 'SHA256 mismatch') { throw }; $results.Add('PASS flipped-byte backup refused') }
    $results.Add((Invoke-Checked 'powershell.exe' @('-NoProfile','-File',$restoreTool,'-Store','CharacterState','-BackupDirectory',$backup,'-InstanceRoot',$restoredInstance)))
    $restored = Get-Content (Join-Path $restoredInstance 'connection.json') -Raw | ConvertFrom-Json
    $restoredRevision = (Invoke-Checked (Join-Path $pg 'psql.exe') @('-X','-A','-t','-h','127.0.0.1','-p',"$($restored.port)",'-U',$restored.username,'-d',$restored.database,'-c',"SELECT revision FROM character_state.progression WHERE character_id='character-1'") @{ PGPASSWORD=$restored.password }).Trim()
    if ($restoredRevision -ne $backupRevision) { throw 'Restored character revision differs from backup point.' }
    $restoredPosition = (Invoke-Checked (Join-Path $pg 'psql.exe') @('-X','-A','-t','-h','127.0.0.1','-p',"$($restored.port)",'-U',$restored.username,'-d',$restored.database,'-c',"SELECT revision,x,zone_id FROM character_state.positions WHERE character_id='character-1'") @{ PGPASSWORD=$restored.password }).Trim()
    if ($restoredPosition -ne '1|12.5|silverbrook') { throw 'Restored position differs from backup point.' }
    $results.Add('PASS restored character state equals backup point')
    [void](Sql 'UPDATE character_state.schema_version SET version=8' "state_$run")
    $results.Add((Invoke-Checked $dotnet @($dll,'newer') @{ CHARACTER_STATE_CONNECTION=$connection }))
    try { [void](Invoke-Checked 'powershell.exe' @('-NoProfile','-File',$backupTool,'-Store','CharacterState','-OutputDirectory',(Join-Path $EvidenceRoot 'newer-backup'),'-HostName','127.0.0.1','-Port',"$port",'-Database',"state_$run",'-Username','state_svc') @{ PGPASSWORD=$servicePassword }); throw 'Newer schema backup accepted.' }
    catch { if ($_.Exception.Message -notmatch 'Unsupported source schema') { throw }; $results.Add('PASS newer schema backup refused') }
    $results | Set-Content (Join-Path $EvidenceRoot 'results.txt')
}
catch { $failure = $_.Exception.Message.Replace($adminPassword,'[REDACTED]').Replace($servicePassword,'[REDACTED]') }
finally {
    try {
        if (Test-Path (Join-Path $restoredInstance 'owner.json')) { [void](Invoke-Checked 'powershell.exe' @('-NoProfile','-File',$restoreTool,'-Store','CharacterState','-InstanceRoot',$restoredInstance,'-Cleanup')) }
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
if ($failure -ne '' -or -not $removed) { throw "Character state durable fixture failed: $failure Evidence: $EvidenceRoot" }
Write-Output "Character state durable fixture passed; owned private storage removed. Evidence: $EvidenceRoot"
