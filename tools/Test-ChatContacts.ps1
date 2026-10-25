[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot, [switch]$MailOnly)
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
$started = $false; $hostProcess = $null; $identityProcess = $null
function FreePort {
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0)
    $listener.Start(); $value = $listener.LocalEndpoint.Port; $listener.Stop(); return $value
}
$port = FreePort; $hostPort = FreePort; $identityPort = FreePort; $internalPort = FreePort; $mailPort = FreePort
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
function StartIdentity {
    $script:identityProcess = Start-Process -FilePath $dotnet -ArgumentList @((Join-Path $repo 'BackendServices/IdentityGateway/bin/Release/net10.0/IdentityGateway.dll')) -WorkingDirectory $repo -Environment @{ASPNETCORE_ENVIRONMENT='Development'} -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'identity.out') -RedirectStandardError (Join-Path $private 'identity.err')
    $ready = $false
    for ($i=0; $i -lt 100; $i++) {
        if ($script:identityProcess.HasExited) { throw 'Identity gateway exited during startup.' }
        try { if ((Invoke-WebRequest "http://127.0.0.1:$identityPort/v1/characters" -SkipHttpErrorCheck -TimeoutSec 1).StatusCode -eq 401) { $ready = $true; break } } catch { }
        Start-Sleep -Milliseconds 100
    }
    if (-not $ready) { throw 'Identity gateway startup timeout.' }
}
try {
    [void](New-Item -ItemType Directory -Path $private -Force)
    $account = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    Run 'icacls.exe' @($private,'/inheritance:r','/grant:r',"${account}:(OI)(CI)F")
    @{run=$run;data=$data;port=$port} | ConvertTo-Json | Set-Content (Join-Path $private 'owner.json')
    Run $dotnet @('restore',(Join-Path $repo 'BackendServices/Chat.Host.Tests/Chat.Host.Tests.csproj'),'--locked-mode','--packages',$nuget,'--verbosity','quiet')
    Run $dotnet @('build',(Join-Path $repo 'BackendServices/Chat.Host.Tests/Chat.Host.Tests.csproj'),'--no-restore','--configuration','Release','--verbosity','quiet')
    Run $dotnet @('restore',(Join-Path $repo 'BackendServices/IdentityGateway/IdentityGateway.csproj'),'--locked-mode','--packages',$nuget,'--verbosity','quiet')
    Run $dotnet @('build',(Join-Path $repo 'BackendServices/IdentityGateway/IdentityGateway.csproj'),'--no-restore','--configuration','Release','--verbosity','quiet')
    $passwordFile = Join-Path $private 'admin-password'; [IO.File]::WriteAllText($passwordFile,$adminPassword)
    Run (Join-Path $pg 'initdb.exe') @('-D',$data,'-U','fixture_admin','--pwfile',$passwordFile,'--auth','scram-sha-256','--encoding','UTF8','--no-locale')
    Remove-Item -LiteralPath $passwordFile -Force
    [IO.File]::AppendAllText((Join-Path $data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$port`nfsync=on`nsynchronous_commit=on`nfull_page_writes=on`n")
    [IO.File]::WriteAllLines((Join-Path $data 'pg_hba.conf'),@('host all fixture_admin 127.0.0.1/32 scram-sha-256',"host chat_$run chat_svc 127.0.0.1/32 scram-sha-256","host chat_$run identity_svc 127.0.0.1/32 scram-sha-256",'host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    CheckOwned
    Run (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-l',(Join-Path $private 'postgres.log'),'-w','-t','30','start'); $started = $true
    $env:PGPASSWORD = $adminPassword
    Sql "CREATE ROLE chat_owner NOLOGIN; CREATE ROLE identity_owner NOLOGIN; CREATE ROLE chat_svc LOGIN PASSWORD '$servicePassword'; CREATE ROLE identity_svc LOGIN PASSWORD '$servicePassword'; CREATE DATABASE chat_$run;"
    Sql "REVOKE ALL ON DATABASE chat_$run FROM PUBLIC; GRANT CONNECT ON DATABASE chat_$run TO chat_svc,identity_svc; GRANT CREATE ON DATABASE chat_$run TO chat_owner,identity_owner; REVOKE ALL ON SCHEMA public FROM PUBLIC;" "chat_$run"
    $migration = "Host=127.0.0.1;Port=$port;Database=chat_$run;Username=fixture_admin;Password=$adminPassword;Pooling=false"
    $env:CHAT_MAINTENANCE_CONNECTION = $migration
    & (Join-Path $repo 'tools/Invoke-SchemaMigrations.ps1') -Connection $migration -Schemas chat -Role chat_owner -Dotnet $dotnet -Nuget $nuget | Out-Null
    & (Join-Path $repo 'tools/Invoke-SchemaMigrations.ps1') -Connection $migration -Schemas identity -Role identity_owner -Dotnet $dotnet -Nuget $nuget | Out-Null
    Sql 'GRANT USAGE ON SCHEMA chat TO chat_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA chat TO chat_svc;' "chat_$run"
    Sql 'GRANT USAGE ON SCHEMA identity TO identity_svc; GRANT SELECT ON identity.schema_version TO identity_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON identity.accounts,identity.sessions,identity.characters,identity.ticket_uses TO identity_svc;' "chat_$run"
    Sql 'GRANT USAGE ON SCHEMA auth TO identity_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON auth.account_data_keys,auth.account_data_key_erasure TO identity_svc; GRANT SELECT,INSERT ON auth.erasure_tombstones,auth.data_key_unwraps TO identity_svc;' "chat_$run"
    $kekPath = Join-Path $private 'account-data-kek'; [IO.File]::WriteAllBytes($kekPath,[Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $env:Identity__KekDevPath = $kekPath
    $env:CHAT_CONNECTION = "Host=127.0.0.1;Port=$port;Database=chat_$run;Username=chat_svc;Password=$servicePassword;Pooling=true;Maximum Pool Size=8;Timeout=2;Command Timeout=5"
    $env:Identity__Connection = "Host=127.0.0.1;Port=$port;Database=chat_$run;Username=identity_svc;Password=$servicePassword;Pooling=true;Maximum Pool Size=8;Timeout=2;Command Timeout=5"
    $env:Identity__TicketSigningKey = [Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $env:Identity__WorldVerifierKey = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $env:Identity__ChatVerifierKey = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $env:Identity__DataKeyChatKey = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $env:Identity__ListenUrl = "http://127.0.0.1:$identityPort"
    $env:Identity__InternalListenUrl = "https://127.0.0.1:$internalPort"
    function NewFixtureCert([string]$name) {
        $rsa = [Security.Cryptography.RSA]::Create(2048)
        $request = [Security.Cryptography.X509Certificates.CertificateRequest]::new("CN=$name",$rsa,[Security.Cryptography.HashAlgorithmName]::SHA256,[Security.Cryptography.RSASignaturePadding]::Pkcs1)
        $cert = $request.CreateSelfSigned([DateTimeOffset]::UtcNow.AddHours(-1),[DateTimeOffset]::UtcNow.AddDays(2))
        $path = Join-Path $private "$name.pfx"
        [IO.File]::WriteAllBytes($path,$cert.Export([Security.Cryptography.X509Certificates.X509ContentType]::Pfx))
        return @{path=$path;sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($cert.RawData))}
    }
    $serverCert = NewFixtureCert 'identity-server'; $clientCert = NewFixtureCert 'chat-client'; $wrongCert = NewFixtureCert 'wrong-client'; $privacyCert = NewFixtureCert 'privacy-client'
    $env:Identity__InternalServerCertificatePath = $serverCert.path
    $env:Identity__DataKeyChatClientSha256 = $clientCert.sha256
    $env:Identity__PrivacyClientSha256 = $privacyCert.sha256
    $env:Identity__PrivacyWorkflowKey = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $env:CHAT_IDENTITY_INTERNAL_URL = $env:Identity__InternalListenUrl
    $env:CHAT_IDENTITY_CLIENT_CERT_PATH = $clientCert.path
    $env:CHAT_IDENTITY_WRONG_CERT_PATH = $wrongCert.path
    $env:CHAT_IDENTITY_PRIVACY_CERT_PATH = $privacyCert.path
    $env:CHAT_IDENTITY_SERVER_CERT_SHA256 = $serverCert.sha256
    $env:Identity__TicketLifetimeSeconds = '5'
    $env:CHAT_IDENTITY_URL = $env:Identity__ListenUrl
    $env:CHAT_IDENTITY_SERVER_ID = 'srvi_chat_local'
    $env:SF_IDENTITY_CHAT_VERIFIER_KEY = $env:Identity__ChatVerifierKey
    $env:CHAT_TEST_STATE = Join-Path $private 'contacts-state.json'
    $env:CHAT_HOST_PORT = "$hostPort"; $env:SF_CHAT_TOKEN = $token
    $env:CHAT_WS_PORT = "$mailPort"; $env:SF_IDENTITY_DATA_KEY_CHAT_KEY = $env:Identity__DataKeyChatKey
    $env:CHAT_GROUP_URL = 'http://127.0.0.1:1'; $env:CHAT_GUILD_URL = 'http://127.0.0.1:1'
    $env:SF_GAMEPLAY_TOKEN = $token; $env:SF_GUILD_TOKEN = $token
    $env:SF_CHAT_PRIVACY_TOKEN = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $startupReject = Start-Process -FilePath $dotnet -ArgumentList @((Join-Path $repo 'BackendServices/IdentityGateway/bin/Release/net10.0/IdentityGateway.dll'),'--maintain-data-keys') -WorkingDirectory $repo -Environment @{ASPNETCORE_ENVIRONMENT='Production'} -PassThru -Wait -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'startup-reject.out') -RedirectStandardError (Join-Path $private 'startup-reject.err')
    if ($startupReject.ExitCode -eq 0 -or (Get-Content (Join-Path $private 'startup-reject.err') -Raw) -notmatch 'KekDevPath is allowed only in Development') { throw 'Production dev-KEK startup was not rejected.' }
    Write-Output 'PASS production dev-KEK startup rejection'
    StartIdentity
    if ((Get-Content (Join-Path $private 'identity.out') -Raw) -notmatch 'IDENTITY_KEK_PROVIDER provider=development-file kek_id=v1') { throw 'KEK provider startup log missing.' }
    Write-Output 'PASS KEK provider startup log'
    StartHost
    if (-not $MailOnly) {
        Run $dotnet @((Join-Path $repo 'BackendServices/Chat.Host.Tests/bin/Release/net10.0/Chat.Host.Tests.dll'),'first')
        Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit(); $hostProcess = $null
        Remove-Item -LiteralPath (Join-Path $private 'chat-host.out'),(Join-Path $private 'chat-host.err') -Force
        StartHost
        Run $dotnet @((Join-Path $repo 'BackendServices/Chat.Host.Tests/bin/Release/net10.0/Chat.Host.Tests.dll'),'restart')
    }
    Run $dotnet @((Join-Path $repo 'BackendServices/Chat.Host.Tests/bin/Release/net10.0/Chat.Host.Tests.dll'),'mail')
    $maintenanceProcess = Start-Process -FilePath $dotnet -ArgumentList @((Join-Path $repo 'BackendServices/IdentityGateway/bin/Release/net10.0/IdentityGateway.dll'),'--maintain-data-keys') -WorkingDirectory $repo -Environment @{ASPNETCORE_ENVIRONMENT='Development';Identity__MaintenanceConnection=$migration} -PassThru -Wait -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'maintenance.out') -RedirectStandardError (Join-Path $private 'maintenance.err')
    if ($maintenanceProcess.ExitCode -ne 0 -or (Get-Content (Join-Path $private 'maintenance.out') -Raw) -notmatch 'finalized=0 purged_unwrap_audits=0') { throw 'Owner maintenance command failed.' }
    Write-Output 'PASS owner maintenance command'
    Write-Output 'PASS ChatContacts fixture'
}
finally {
    if ($hostProcess -and -not $hostProcess.HasExited) { Stop-Process -Id $hostProcess.Id -Force; $hostProcess.WaitForExit() }
    if ($identityProcess -and -not $identityProcess.HasExited) { Stop-Process -Id $identityProcess.Id -Force; $identityProcess.WaitForExit() }
    if ($started) { CheckOwned; Run (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-m','immediate','-w','-t','30','stop') }
    foreach ($name in @('PGPASSWORD','CHAT_CONNECTION','CHAT_MAINTENANCE_CONNECTION','CHAT_HOST_PORT','CHAT_WS_PORT','SF_CHAT_TOKEN','SF_CHAT_PRIVACY_TOKEN','CHAT_IDENTITY_URL','CHAT_IDENTITY_SERVER_ID','SF_IDENTITY_CHAT_VERIFIER_KEY','SF_IDENTITY_DATA_KEY_CHAT_KEY','CHAT_IDENTITY_INTERNAL_URL','CHAT_IDENTITY_CLIENT_CERT_PATH','CHAT_IDENTITY_WRONG_CERT_PATH','CHAT_IDENTITY_PRIVACY_CERT_PATH','CHAT_IDENTITY_SERVER_CERT_SHA256','CHAT_GROUP_URL','CHAT_GUILD_URL','SF_GAMEPLAY_TOKEN','SF_GUILD_TOKEN','CHAT_TEST_STATE','Identity__Connection','Identity__TicketSigningKey','Identity__WorldVerifierKey','Identity__ChatVerifierKey','Identity__DataKeyChatKey','Identity__ListenUrl','Identity__InternalListenUrl','Identity__InternalServerCertificatePath','Identity__DataKeyChatClientSha256','Identity__PrivacyClientSha256','Identity__PrivacyWorkflowKey','Identity__TicketLifetimeSeconds','Identity__KekDevPath')) { Remove-Item "Env:$name" -ErrorAction SilentlyContinue }
    & $dotnet build-server shutdown | Out-Null
}
