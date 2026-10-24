[CmdletBinding()]
param([ValidateRange(12000,65000)][int]$WorldPort = 17884)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'InventoryLedgerHostFixture.ps1') # pinned process, SQL and port helpers
$runRoot = Join-Path $repo ('Artifacts/Persistence/IdentityGatewayWorld/run-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$private = Join-Path $runRoot 'private'
$fx = @{ repo=$repo; private=$private; secrets=@() }
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$pgStarted = $false
$dotnet = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
$pg = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
$editor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project = Join-Path $repo 'GameClient/ShatteredFrontier.uproject'
foreach ($path in @($dotnet,$editor,$project,(Join-Path $pg 'initdb.exe'),(Join-Path $pg 'pg_ctl.exe'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required fixture input missing: $path" }
}
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $WorldPort).Count) {
    throw "World port $WorldPort is already in use."
}
$null = New-Item -ItemType Directory -Path $private -Force
$fx.dotnet = $dotnet; $fx.pg = $pg; $fx.pgPort = Get-FreeTcpPort
$fx.db = 'identity_' + [guid]::NewGuid().ToString('N')
$fx.adminPassword = New-HexSecret; $fx.servicePassword = New-HexSecret
$verifierKey = New-HexSecret; $ticketKey = [Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
$password = New-HexSecret
$fx.secrets = @($fx.adminPassword,$fx.servicePassword,$verifierKey,$ticketKey,$password)
$data = Join-Path $private 'data'
$GatewayPort = Get-FreeTcpPort
while ($GatewayPort -eq $WorldPort) { $GatewayPort = Get-FreeTcpPort }
$gatewayUrl = "http://127.0.0.1:$GatewayPort"
function Wait-Log([string]$File,[string]$Pattern,[int]$Seconds=90) {
    $clock = [Diagnostics.Stopwatch]::StartNew()
    while ($clock.Elapsed.TotalSeconds -lt $Seconds) {
        if ((Test-Path -LiteralPath $File) -and (Select-String -LiteralPath $File -Pattern $Pattern -Quiet)) { return }
        Start-Sleep -Milliseconds 300
    }
    throw "Timed out waiting for $Pattern in $(Split-Path $File -Leaf)."
}
function Start-Editor([string]$Name,[string]$Url,[string[]]$Extra,[hashtable]$Environment=@{}) {
    $user = Join-Path $private ($Name+'-user'); $null = New-Item -ItemType Directory -Path $user
    $log = Join-Path $runRoot ($Name+'.log')
    $editorArgs = @('"'+$project+'"','"'+$Url+'"','-unattended','-NullRHI','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"')) + $Extra
    Invoke-WithEnv $Environment {
        $script:startedEditor = Start-Process -FilePath $editor -ArgumentList $editorArgs -PassThru -WindowStyle Hidden `
            -RedirectStandardOutput (Join-Path $runRoot ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $runRoot ($Name+'.stderr.txt'))
    }
    $null = $script:startedEditor.Handle; $owned.Add($script:startedEditor)
    return $log
}
try {
    $pwfile = Join-Path $private 'admin-password'; [IO.File]::WriteAllText($pwfile,$fx.adminPassword)
    [void](Invoke-FxOk $fx (Join-Path $pg 'initdb.exe') @('-D',$data,'-U','fixture_admin','--pwfile',$pwfile,'--auth','scram-sha-256','--encoding','UTF8','--no-locale'))
    Remove-Item -LiteralPath $pwfile -Force
    [IO.File]::AppendAllText((Join-Path $data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$($fx.pgPort)`n")
    [IO.File]::WriteAllLines((Join-Path $data 'pg_hba.conf'),@('host all fixture_admin 127.0.0.1/32 scram-sha-256',
        "host $($fx.db) identity_service 127.0.0.1/32 scram-sha-256",'host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    [void](Invoke-FxOk $fx (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-l',(Join-Path $private 'postgres.log'),'-w','-t','30','start'))
    $pgStarted = $true
    [void](Invoke-LedgerSql $fx "CREATE ROLE identity_owner NOLOGIN; CREATE ROLE identity_service LOGIN PASSWORD '$($fx.servicePassword)'; CREATE DATABASE $($fx.db);")
    [void](Invoke-LedgerSql $fx 'REVOKE ALL ON SCHEMA public FROM PUBLIC; CREATE SCHEMA identity AUTHORIZATION identity_owner;' $fx.db)
    [void](Invoke-LedgerSql $fx ("SET ROLE identity_owner;`n" + (Get-Content (Join-Path $repo 'BackendServices/IdentityGateway/Schema.sql') -Raw)) $fx.db)
    [void](Invoke-LedgerSql $fx 'GRANT USAGE ON SCHEMA identity TO identity_service; GRANT SELECT ON identity.schema_version TO identity_service; GRANT SELECT,INSERT,UPDATE,DELETE ON identity.accounts,identity.sessions,identity.characters,identity.ticket_uses TO identity_service;' $fx.db)
    $connection = "Host=127.0.0.1;Port=$($fx.pgPort);Database=$($fx.db);Username=identity_service;Password=$($fx.servicePassword);Pooling=false"
    $gatewayProject = Join-Path $repo 'BackendServices/IdentityGateway/IdentityGateway.csproj'
    $gatewayBuild = Join-Path $private 'gateway-build'; $null = New-Item -ItemType Directory -Path $gatewayBuild
    [void](Invoke-FxOk $fx $dotnet @('build',$gatewayProject,'--configuration','Release','--output',$gatewayBuild,'--verbosity','quiet'))
    $gatewayEnv = @{ Identity__Connection=$connection; Identity__TicketSigningKey=$ticketKey; Identity__WorldVerifierKey=$verifierKey;
        Identity__ListenUrl=$gatewayUrl; Identity__WorldAddress="127.0.0.1:$WorldPort"; Identity__TicketLifetimeSeconds='120' }
    Invoke-WithEnv $gatewayEnv {
        $script:gatewayProcess = Start-Process -FilePath $dotnet -ArgumentList ('"'+(Join-Path $gatewayBuild 'IdentityGateway.dll')+'"') -PassThru -WindowStyle Hidden `
            -RedirectStandardOutput (Join-Path $runRoot 'gateway.stdout.txt') -RedirectStandardError (Join-Path $runRoot 'gateway.stderr.txt')
    }
    $null = $script:gatewayProcess.Handle; $owned.Add($script:gatewayProcess)
    Wait-Log (Join-Path $runRoot 'gateway.stdout.txt') 'Now listening' 30
    $map = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
    $serverLog = Start-Editor 'server' $map @('-server',"-port=$WorldPort",'-multihome=127.0.0.1','-SFIdentityGateway',
        "-ini:Game:[/Script/ShatteredFrontier.SFFrontEndSettings]:GatewayBaseUrl=$gatewayUrl") @{ SF_IDENTITY_VERIFIER_KEY=$verifierKey }
    Wait-Log $serverLog 'E1_WORLD_READY' 120
    $headers = @{ 'Idempotency-Key' = [guid]::NewGuid().ToString('N') }
    $accountName = 'gate' + [guid]::NewGuid().ToString('N').Substring(0,12)
    $null = Invoke-RestMethod "$gatewayUrl/v1/accounts" -Method Post -Headers $headers -ContentType 'application/json' -Body (@{account_name=$accountName;password=$password} | ConvertTo-Json -Compress)
    $begin = Invoke-RestMethod "$gatewayUrl/v1/login/begin" -Method Post -Headers $headers -ContentType 'application/json' -Body (@{account_name=$accountName;password=$password} | ConvertTo-Json -Compress)
    $complete = Invoke-RestMethod "$gatewayUrl/v1/login/complete" -Method Post -Headers $headers -ContentType 'application/json' -Body (@{login_id=$begin.login_id} | ConvertTo-Json -Compress)
    $headers.Authorization = 'Bearer ' + $complete.session_token
    $packet = [byte[]]::new(29); $packet[0]=2; $packet[3]=1; $packet[4]=128; $packet[5]=128; $packet[6]=8
    $characterName = 'Gate' + (-join (1..9 | ForEach-Object { [char](Get-Random -Minimum 97 -Maximum 123) }))
    $character = Invoke-RestMethod "$gatewayUrl/v1/characters" -Method Post -Headers $headers -ContentType 'application/json' `
        -Body (@{realm_id='greenlands';name=$characterName;appearance_v2_base64=[Convert]::ToBase64String($packet)} | ConvertTo-Json -Compress)
    $entry = Invoke-RestMethod "$gatewayUrl/v1/characters/$($character.character_id)/enter-world" -Method Post -Headers $headers -ContentType 'application/json' -Body '{}'
    $untrusted = Invoke-WebRequest "$gatewayUrl/v1/world/verify-ticket" -Method Post -Headers @{ 'X-World-Verifier-Key'='invalid' } `
        -ContentType 'application/json' -Body (@{ticket=$entry.ticket;server_id='srvi_dev_local'} | ConvertTo-Json -Compress) -SkipHttpErrorCheck
    if ($untrusted.StatusCode -ne 401) { throw 'Untrusted world verifier was accepted.' }
    $travel = "127.0.0.1:$WorldPort" + '?SFIdentityTicket=' + $entry.ticket
    $clientLog = Start-Editor 'client' $travel @('-game')
    Wait-Log $serverLog ('GATEWAY_IDENTITY_BOUND Character=' + $character.character_id) 100
    $replayLog = Start-Editor 'replay' $travel @('-game')
    Wait-Log $serverLog 'GATEWAY_TICKET_REJECTED Reason=verify' 100
    $missingLog = Start-Editor 'missing' "127.0.0.1:$WorldPort" @('-game')
    Wait-Log $serverLog 'GATEWAY_TICKET_REJECTED Reason=missing' 100
    $accepted = @(Select-String -LiteralPath $serverLog -Pattern 'GATEWAY_IDENTITY_BOUND').Count
    if ($accepted -ne 1) { throw "Expected one admitted connection, found $accepted." }
    Write-Output 'PASS account/login/create/enter-world; 1 admitted, replay and missing ticket refused by dedicated server'
}
finally {
    foreach ($process in $owned) { if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue; $process.WaitForExit() } }
    if ($pgStarted) { [void](Invoke-FxChecked $fx (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-m','immediate','-w','-t','30','stop')) }
    & $dotnet build-server shutdown | Out-Null
}
