[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/Persistence/ChatContacts'))
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $EvidenceRoot)) { throw 'Supply a fresh ChatContacts evidence child.' }
$dotnet = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
$pg = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
$nuget = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/nuget'
foreach ($path in @($dotnet,(Join-Path $pg 'postgres.exe'),(Join-Path $pg 'initdb.exe'),(Join-Path $pg 'pg_ctl.exe'),(Join-Path $pg 'psql.exe'))) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file missing: $path" } }
$pins = Get-Content (Join-Path $repo 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
if ((Get-FileHash $dotnet -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.sdk.executableSha256 -or (Get-FileHash (Join-Path $pg 'postgres.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.postgres.executableSha256) { throw 'Pinned toolchain mismatch.' }
$private = Join-Path $EvidenceRoot 'private'; $data = Join-Path $private 'data'
$run = [guid]::NewGuid().ToString('N')
$bytes = [byte[]]::new(32)
[Security.Cryptography.RandomNumberGenerator]::Fill($bytes); $adminPassword = [Convert]::ToHexString($bytes)
[Security.Cryptography.RandomNumberGenerator]::Fill($bytes); $servicePassword = [Convert]::ToHexString($bytes)
[Security.Cryptography.RandomNumberGenerator]::Fill($bytes); $token = [Convert]::ToHexString($bytes)
$started = $false; $hostProcess = $null
function FreePort {
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0)
    $listener.Start(); $value = $listener.LocalEndpoint.Port; $listener.Stop(); return $value
}
$port = FreePort; $hostPort = FreePort
function Run([string]$file,[string[]]$arguments) {
    & $file @arguments
    if ($LASTEXITCODE -ne 0) { throw "$file exited $LASTEXITCODE" }
}
function Sql([string]$query,[string]$database='postgres') {
    $file = Join-Path $private ('sql-' + [guid]::NewGuid().ToString('N') + '.sql')
    [IO.File]::WriteAllText($file,$query)
    try { Run (Join-Path $pg 'psql.exe') @('-X','-q','-h','127.0.0.1','-p',"$port",'-U','fixture_admin','-d',$database,'-v','ON_ERROR_STOP=1','-f',$file) }
    finally { Remove-Item -LiteralPath $file -Force }
}
function CheckOwned {
    if ([IO.Path]::GetFullPath($data) -ne [IO.Path]::GetFullPath((Join-Path $EvidenceRoot 'private/data')) -or -not $EvidenceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Owned path mismatch.' }
    $marker = Get-Content (Join-Path $private 'owner.json') -Raw | ConvertFrom-Json
    if ($marker.run -ne $run -or $marker.data -ne $data -or $marker.port -ne $port) { throw 'Owned marker mismatch.' }
    foreach ($path in @($EvidenceRoot,$private,$data)) { if ((Get-Item $path).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Owned path is a reparse point.' } }
}
function StartHost {
    $script:hostProcess = Start-Process -FilePath $dotnet -ArgumentList @((Join-Path $repo 'BackendServices/Chat.Host/bin/Release/net10.0/Chat.Host.dll')) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'chat-host.out') -RedirectStandardError (Join-Path $private 'chat-host.err')
    $ready = $false
    for ($i=0; $i -lt 100; $i++) {
        if ($script:hostProcess.HasExited) { throw 'Chat host exited during startup.' }
        if ((Test-Path (Join-Path $private 'chat-host.out')) -and (Get-Content (Join-Path $private 'chat-host.out') -Raw) -match 'CHAT_HOST_READY') { $ready = $true; break }
        Start-Sleep -Milliseconds 100
    }
    if (-not $ready) { throw 'Chat host startup timeout.' }
}
try {
    [void](New-Item -ItemType Directory -Path $private -Force)
    $account = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    Run 'icacls.exe' @($private,'/inheritance:r','/grant:r',"${account}:(OI)(CI)F")
    @{run=$run;data=$data;port=$port} | ConvertTo-Json | Set-Content (Join-Path $private 'owner.json')
    Run $dotnet @('restore',(Join-Path $repo 'BackendServices/Chat.Host.Tests/Chat.Host.Tests.csproj'),'--locked-mode','--packages',$nuget,'--verbosity','quiet')
    Run $dotnet @('build',(Join-Path $repo 'BackendServices/Chat.Host.Tests/Chat.Host.Tests.csproj'),'--no-restore','--configuration','Release','--verbosity','quiet')
    $passwordFile = Join-Path $private 'admin-password'; [IO.File]::WriteAllText($passwordFile,$adminPassword)
    Run (Join-Path $pg 'initdb.exe') @('-D',$data,'-U','fixture_admin','--pwfile',$passwordFile,'--auth','scram-sha-256','--encoding','UTF8','--no-locale')
    Remove-Item -LiteralPath $passwordFile -Force
    [IO.File]::AppendAllText((Join-Path $data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$port`nfsync=on`nsynchronous_commit=on`nfull_page_writes=on`n")
    [IO.File]::WriteAllLines((Join-Path $data 'pg_hba.conf'),@('host all fixture_admin 127.0.0.1/32 scram-sha-256',"host chat_$run chat_svc 127.0.0.1/32 scram-sha-256",'host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    CheckOwned
    Run (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-l',(Join-Path $private 'postgres.log'),'-w','-t','30','start'); $started = $true
    $env:PGPASSWORD = $adminPassword
    Sql "CREATE ROLE chat_owner NOLOGIN; CREATE ROLE chat_svc LOGIN PASSWORD '$servicePassword'; CREATE DATABASE chat_$run;"
    Sql "REVOKE ALL ON DATABASE chat_$run FROM PUBLIC; GRANT CONNECT ON DATABASE chat_$run TO chat_svc; GRANT CREATE ON DATABASE chat_$run TO chat_owner; REVOKE ALL ON SCHEMA public FROM PUBLIC;" "chat_$run"
    $migration = "Host=127.0.0.1;Port=$port;Database=chat_$run;Username=fixture_admin;Password=$adminPassword;Pooling=false"
    & (Join-Path $repo 'tools/Invoke-SchemaMigrations.ps1') -Connection $migration -Schemas chat -Role chat_owner -Dotnet $dotnet -Nuget $nuget | Out-Null
    Sql 'GRANT USAGE ON SCHEMA chat TO chat_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA chat TO chat_svc;' "chat_$run"
    $env:CHAT_CONNECTION = "Host=127.0.0.1;Port=$port;Database=chat_$run;Username=chat_svc;Password=$servicePassword;Pooling=true;Maximum Pool Size=8;Timeout=2;Command Timeout=5"
    $env:CHAT_HOST_PORT = "$hostPort"; $env:SF_CHAT_TOKEN = $token
    StartHost
    Run $dotnet @((Join-Path $repo 'BackendServices/Chat.Host.Tests/bin/Release/net10.0/Chat.Host.Tests.dll'),'first')
    Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(); $hostProcess = $null
    Remove-Item -LiteralPath (Join-Path $private 'chat-host.out'),(Join-Path $private 'chat-host.err') -Force
    StartHost
    Run $dotnet @((Join-Path $repo 'BackendServices/Chat.Host.Tests/bin/Release/net10.0/Chat.Host.Tests.dll'),'restart')
    Write-Output 'PASS ChatContacts fixture (21 checks)'
}
finally {
    if ($hostProcess -and -not $hostProcess.HasExited) { Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit() }
    if ($started) { CheckOwned; Run (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-m','immediate','-w','-t','30','stop') }
    foreach ($name in @('PGPASSWORD','CHAT_CONNECTION','CHAT_HOST_PORT','SF_CHAT_TOKEN')) { Remove-Item "Env:$name" -ErrorAction SilentlyContinue }
    & $dotnet build-server shutdown | Out-Null
}
