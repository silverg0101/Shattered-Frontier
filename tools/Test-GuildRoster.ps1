[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'Artifacts/Persistence/GuildRoster'))
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $EvidenceRoot)) { throw 'Supply a fresh GuildRoster evidence child.' }
$dotnet = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
$pg = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
$nuget = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/nuget'
foreach ($path in @($dotnet,(Join-Path $pg 'postgres.exe'),(Join-Path $pg 'initdb.exe'),(Join-Path $pg 'pg_ctl.exe'),(Join-Path $pg 'psql.exe'))) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required file missing: $path" } }
$private = Join-Path $EvidenceRoot 'private'; $data = Join-Path $private 'data'
$run = [guid]::NewGuid().ToString('N')
$bytes = [byte[]]::new(32)
[Security.Cryptography.RandomNumberGenerator]::Fill($bytes); $adminPassword = [Convert]::ToHexString($bytes)
[Security.Cryptography.RandomNumberGenerator]::Fill($bytes); $servicePassword = [Convert]::ToHexString($bytes)
[Security.Cryptography.RandomNumberGenerator]::Fill($bytes); $serviceToken = [Convert]::ToHexString($bytes)
$started = $false; $guildProcess = $null; $identityProcess = $null; $checks = 0
function FreePort {
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0)
    $listener.Start(); $value = $listener.LocalEndpoint.Port; $listener.Stop(); return $value
}
$port = FreePort; $guildPort = FreePort; $identityPort = FreePort
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
function StartGuild {
    $script:guildProcess = Start-Process -FilePath $dotnet -ArgumentList @((Join-Path $repo 'BackendServices/Guild.Host/bin/Release/net10.0/Guild.Host.dll')) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'guild.out') -RedirectStandardError (Join-Path $private 'guild.err')
    for ($i=0; $i -lt 100; $i++) {
        if ($script:guildProcess.HasExited) { throw ('Guild host exited: ' + (Get-Content (Join-Path $private 'guild.err') -Raw)) }
        if ((Test-Path (Join-Path $private 'guild.out')) -and (Get-Content (Join-Path $private 'guild.out') -Raw) -match 'GUILD_HOST_READY') { return }
        Start-Sleep -Milliseconds 100
    }
    throw 'Guild host startup timeout.'
}
function StartIdentity {
    $script:identityProcess = Start-Process -FilePath $dotnet -ArgumentList @((Join-Path $repo 'BackendServices/IdentityGateway/bin/Release/net10.0/IdentityGateway.dll')) -WorkingDirectory $repo -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $private 'identity.out') -RedirectStandardError (Join-Path $private 'identity.err')
    for ($i=0; $i -lt 100; $i++) {
        if ($script:identityProcess.HasExited) { throw ('Identity exited: ' + (Get-Content (Join-Path $private 'identity.err') -Raw)) }
        try { if ((Invoke-WebRequest "http://127.0.0.1:$identityPort/v1/characters" -SkipHttpErrorCheck -TimeoutSec 1).StatusCode -eq 401) { return } } catch { }
        Start-Sleep -Milliseconds 100
    }
    throw 'Identity startup timeout.'
}
function Request([string]$method,[string]$url,[object]$body,[hashtable]$headers=@{}) {
    $args = @{ Uri=$url; Method=$method; Headers=$headers; SkipHttpErrorCheck=$true; TimeoutSec=8 }
    if ($null -ne $body) { $args.Body = ConvertTo-Json -InputObject $body -Compress; $args.ContentType = 'application/json' }
    $response = Invoke-WebRequest @args
    $json = if ($response.Content) { $response.Content | ConvertFrom-Json } else { $null }
    return @{ status=[int]$response.StatusCode; json=$json }
}
function Check([bool]$condition,[string]$name) { if (-not $condition) { throw "FAIL $name" }; $script:checks++; Write-Output "PASS $name" }
function IdentityPost([string]$path,[object]$body,[string]$bearer='') {
    $h = @{ 'Idempotency-Key' = ('guild-' + [guid]::NewGuid().ToString('N')) }
    if ($bearer) { $h.Authorization = "Bearer $bearer" }
    $r = Request 'POST' "http://127.0.0.1:$identityPort/$path" $body $h
    if ($r.status -lt 200 -or $r.status -ge 300) { throw "Identity $path failed $($r.status): $($r.json | ConvertTo-Json -Compress)" }
    return $r.json
}
function Account([string]$name) {
    [void](IdentityPost 'v1/accounts' @{account_name=$name;password='guild-test-password'})
    $login = IdentityPost 'v1/login/begin' @{account_name=$name;password='guild-test-password'}
    return (IdentityPost 'v1/login/complete' @{login_id=$login.login_id}).session_token
}
function Character([string]$bearer,[string]$name,[string]$realm='greenlands') {
    $packet = [byte[]]::new(29); $packet[0]=2; $packet[3]=1; $packet[4]=128; $packet[5]=128; $packet[6]=8
    return (IdentityPost 'v1/characters' @{realm_id=$realm;name=$name;appearance_v2_base64=[Convert]::ToBase64String($packet)} $bearer).character_id
}
function Ticket([string]$bearer,[string]$character) {
    return (IdentityPost "v1/characters/$character/guild-ticket" @{} $bearer).ticket
}
function GuildRequest([string]$method,[string]$path,[object]$body,[string]$bearer,[string]$ticket) {
    $h = @{}
    if ($bearer) { $h.Authorization = "Bearer $bearer" }
    if ($ticket) { $h['X-Guild-Ticket'] = $ticket }
    return Request $method "http://127.0.0.1:$guildPort/$path" $body $h
}
try {
    [void](New-Item -ItemType Directory -Path $private -Force)
    $account = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    Run 'icacls.exe' @($private,'/inheritance:r','/grant:r',"${account}:(OI)(CI)F")
    @{run=$run;data=$data;port=$port} | ConvertTo-Json | Set-Content (Join-Path $private 'owner.json')
    Run $dotnet @('restore',(Join-Path $repo 'BackendServices/Guild.Host/Guild.Host.csproj'),'--locked-mode','--packages',$nuget,'--verbosity','quiet')
    Run $dotnet @('build',(Join-Path $repo 'BackendServices/Guild.Host/Guild.Host.csproj'),'--no-restore','--configuration','Release','--verbosity','quiet')
    Run $dotnet @('build',(Join-Path $repo 'BackendServices/IdentityGateway/IdentityGateway.csproj'),'--configuration','Release','--verbosity','quiet')
    $passwordFile = Join-Path $private 'admin-password'; [IO.File]::WriteAllText($passwordFile,$adminPassword)
    Run (Join-Path $pg 'initdb.exe') @('-D',$data,'-U','fixture_admin','--pwfile',$passwordFile,'--auth','scram-sha-256','--encoding','UTF8','--no-locale')
    Remove-Item -LiteralPath $passwordFile -Force
    [IO.File]::AppendAllText((Join-Path $data 'postgresql.conf'),"`nlisten_addresses='127.0.0.1'`nport=$port`nfsync=on`nsynchronous_commit=on`nfull_page_writes=on`n")
    [IO.File]::WriteAllLines((Join-Path $data 'pg_hba.conf'),@('host all fixture_admin 127.0.0.1/32 scram-sha-256',"host guild_$run guild_svc 127.0.0.1/32 scram-sha-256","host guild_$run identity_svc 127.0.0.1/32 scram-sha-256",'host all all 127.0.0.1/32 reject','host all all ::1/128 reject'))
    CheckOwned
    Run (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-l',(Join-Path $private 'postgres.log'),'-w','-t','30','start'); $started = $true
    $env:PGPASSWORD = $adminPassword
    Sql "CREATE ROLE guild_owner NOLOGIN; CREATE ROLE identity_owner NOLOGIN; CREATE ROLE guild_svc LOGIN PASSWORD '$servicePassword'; CREATE ROLE identity_svc LOGIN PASSWORD '$servicePassword'; CREATE DATABASE guild_$run;"
    Sql "REVOKE ALL ON DATABASE guild_$run FROM PUBLIC; GRANT CONNECT ON DATABASE guild_$run TO guild_svc,identity_svc; GRANT CREATE ON DATABASE guild_$run TO guild_owner,identity_owner; REVOKE ALL ON SCHEMA public FROM PUBLIC;" "guild_$run"
    $migration = "Host=127.0.0.1;Port=$port;Database=guild_$run;Username=fixture_admin;Password=$adminPassword;Pooling=false"
    & (Join-Path $repo 'tools/Invoke-SchemaMigrations.ps1') -Connection $migration -Schemas guild -Role guild_owner -Dotnet $dotnet -Nuget $nuget | Out-Null
    & (Join-Path $repo 'tools/Invoke-SchemaMigrations.ps1') -Connection $migration -Schemas identity -Role identity_owner -Dotnet $dotnet -Nuget $nuget | Out-Null
    Sql 'GRANT USAGE ON SCHEMA guild TO guild_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA guild TO guild_svc; GRANT USAGE ON ALL SEQUENCES IN SCHEMA guild TO guild_svc;' "guild_$run"
    Sql 'GRANT USAGE ON SCHEMA identity TO identity_svc; GRANT SELECT ON identity.schema_version TO identity_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON identity.accounts,identity.sessions,identity.characters,identity.ticket_uses TO identity_svc;' "guild_$run"
    Sql 'GRANT USAGE ON SCHEMA auth TO identity_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON auth.account_data_keys,auth.account_data_key_erasure,auth.erasure_tombstones TO identity_svc; GRANT INSERT ON auth.data_key_unwraps TO identity_svc;' "guild_$run"
    $kekPath = Join-Path $private 'account-data-kek'; [IO.File]::WriteAllBytes($kekPath,[Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $env:Identity__KekDevPath = $kekPath
    $env:GUILD_CONNECTION = "Host=127.0.0.1;Port=$port;Database=guild_$run;Username=guild_svc;Password=$servicePassword;Pooling=true;Maximum Pool Size=8;Timeout=2;Command Timeout=5"
    $env:Identity__Connection = "Host=127.0.0.1;Port=$port;Database=guild_$run;Username=identity_svc;Password=$servicePassword;Pooling=true;Maximum Pool Size=8;Timeout=2;Command Timeout=5"
    $env:Identity__TicketSigningKey = [Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $env:Identity__WorldVerifierKey = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $env:Identity__ChatVerifierKey = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $env:Identity__GuildVerifierKey = [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $env:Identity__ListenUrl = "http://127.0.0.1:$identityPort"
    $env:GUILD_IDENTITY_URL = $env:Identity__ListenUrl
    $env:GUILD_IDENTITY_SERVER_ID = 'srvi_guild_local'
    $env:SF_IDENTITY_GUILD_VERIFIER_KEY = $env:Identity__GuildVerifierKey
    $env:GUILD_HOST_PORT = "$guildPort"; $env:SF_GUILD_TOKEN = $serviceToken
    StartIdentity; StartGuild
    $aToken = Account 'guildtestera'; $bToken = Account 'guildtesterb'; $cToken = Account 'guildtesterc'; $dToken = Account 'guildtesterd'
    $a = Character $aToken 'GuildAlina'; $b = Character $bToken 'GuildBrenna'; $c = Character $cToken 'GuildCora'; $d = Character $dToken 'GuildDara'
    $create = @{idempotencyKey='guild/create';name='Silverbrook Crew';tag='SBC'}
    Check ((GuildRequest 'POST' 'v1/guilds' $create '' (Ticket $aToken $a)).status -eq 401) 'service token required'
    Check ((GuildRequest 'POST' 'v1/guilds' $create $serviceToken '').status -eq 401) 'guild ticket required'
    Check ((GuildRequest 'POST' 'v1/guilds' $create $serviceToken 'invalid').status -eq 401) 'invalid ticket rejected'
    $chatTicket = (IdentityPost "v1/characters/$a/contacts-ticket" @{} $aToken).ticket
    Check ((GuildRequest 'POST' 'v1/guilds' $create $serviceToken $chatTicket).status -eq 401) 'chat-scope ticket rejected'
    $wrong = Request 'POST' "http://127.0.0.1:$identityPort/v1/characters/$a/guild-ticket" @{} @{Authorization="Bearer $bToken";'Idempotency-Key'='guild/wrong'}
    Check ($wrong.status -eq 404) 'cross-account ticket denied'
    $made = GuildRequest 'POST' 'v1/guilds' $create $serviceToken (Ticket $aToken $a)
    Check ($made.status -eq 200 -and $made.json.members.Count -eq 1 -and $made.json.members[0].rank -eq 0) 'founder created'
    $guildId = $made.json.guildId
    $replay = GuildRequest 'POST' 'v1/guilds' $create $serviceToken (Ticket $aToken $a)
    Check ($replay.status -eq 200 -and $replay.json.guildId -eq $guildId) 'create replay'
    $reuse = GuildRequest 'POST' 'v1/guilds' @{idempotencyKey='guild/create';name='Changed Name';tag='XXX'} $serviceToken (Ticket $aToken $a)
    Check ($reuse.status -eq 400) 'changed idempotency input denied'
    Check ((GuildRequest 'GET' "v1/guilds/$guildId/roster" $null $serviceToken (Ticket $bToken $b)).status -eq 403) 'outsider roster denied'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/invitations" @{idempotencyKey='guild/invite-wrong';recipientCharacterId=$c} $serviceToken (Ticket $bToken $b)).status -eq 403) 'outsider invite denied'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/accept" @{idempotencyKey='guild/accept-uninvited'} $serviceToken (Ticket $cToken $c)).status -eq 403) 'uninvited acceptance denied'
    $second = GuildRequest 'POST' 'v1/guilds' @{idempotencyKey='guild/create-c';name='Mara Crew';tag='MRC'} $serviceToken (Ticket $cToken $c)
    Check ($second.status -eq 200) 'second guild created'
    Check ((GuildRequest 'POST' 'v1/guilds' @{idempotencyKey='guild/skeleton';name='Rnara Crew';tag='RNC'} $serviceToken (Ticket $dToken $d)).status -eq 409) 'confusable guild name denied'
    Check ((GuildRequest 'POST' 'v1/guilds' @{idempotencyKey='guild/tag';name='Other Crew';tag='mrc'} $serviceToken (Ticket $dToken $d)).status -eq 409) 'folded guild tag denied'
    $invite = GuildRequest 'POST' "v1/guilds/$guildId/invitations" @{idempotencyKey='guild/invite-b';recipientCharacterId=$b} $serviceToken (Ticket $aToken $a)
    Check ($invite.status -eq 200 -and $invite.json.invited) 'leader invitation'
    $oneTicket = Ticket $bToken $b
    $accepted = GuildRequest 'POST' "v1/guilds/$guildId/accept" @{idempotencyKey='guild/accept-b'} $serviceToken $oneTicket
    Check ($accepted.status -eq 200 -and $accepted.json.members.Count -eq 2 -and $accepted.json.members[1].rank -eq 1) 'recipient joined at rank one'
    Check ((GuildRequest 'GET' "v1/guilds/$guildId/roster" $null $serviceToken $oneTicket).status -eq 401) 'ticket replay denied'
    $acceptReplay = GuildRequest 'POST' "v1/guilds/$guildId/accept" @{idempotencyKey='guild/accept-b'} $serviceToken (Ticket $bToken $b)
    Check ($acceptReplay.status -eq 200 -and $acceptReplay.json.members.Count -eq 2) 'accept replay is stable'
    $roster = GuildRequest 'GET' "v1/guilds/$guildId/roster" $null $serviceToken (Ticket $bToken $b)
    Check ($roster.status -eq 200 -and $roster.json.members.Count -eq 2) 'member roster read'
    Stop-Process -Id $guildProcess.Id -Force; $guildProcess.WaitForExit(); $guildProcess = $null
    Remove-Item -LiteralPath (Join-Path $private 'guild.out'),(Join-Path $private 'guild.err') -Force
    StartGuild
    $restart = GuildRequest 'GET' "v1/guilds/$guildId/roster" $null $serviceToken (Ticket $aToken $a)
    Check ($restart.status -eq 200 -and $restart.json.members.Count -eq 2 -and $restart.json.version -eq 2) 'roster survives restart'
    Check ((GuildRequest 'GET' "v1/guild-membership/$a" $null '' '').status -eq 401) 'membership lookup requires service token'
    $lookup = GuildRequest 'GET' "v1/guild-membership/$b" $null $serviceToken ''
    Check ($lookup.status -eq 200 -and $lookup.json.guildId -eq $guildId -and $lookup.json.members.Count -eq 2) 'service membership lookup'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/ranks/1/permissions" @{idempotencyKey='guild/deny-edit';permissions=@('invite')} $serviceToken (Ticket $bToken $b)).status -eq 403) 'member cannot edit rank permissions'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/members/$a/kick" @{idempotencyKey='guild/deny-kick'} $serviceToken (Ticket $bToken $b)).status -eq 403) 'member cannot kick leader'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/disband" @{idempotencyKey='guild/deny-disband'} $serviceToken (Ticket $bToken $b)).status -eq 403) 'member cannot disband'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/invitations" @{idempotencyKey='guild/deny-invite';recipientCharacterId=$d} $serviceToken (Ticket $bToken $b)).status -eq 403) 'member cannot invite without permission'
    $edit = GuildRequest 'POST' "v1/guilds/$guildId/ranks/1/permissions" @{idempotencyKey='guild/rank1';permissions=@('invite','kick_lower','promote_lower')} $serviceToken (Ticket $aToken $a)
    Check ($edit.status -eq 200) 'leader grants rank permissions'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/invitations" @{idempotencyKey='guild/invite-d';recipientCharacterId=$d} $serviceToken (Ticket $bToken $b)).status -eq 200) 'officer invites'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/accept" @{idempotencyKey='guild/accept-d'} $serviceToken (Ticket $dToken $d)).status -eq 200) 'third member joins'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/members/$d/rank" @{idempotencyKey='guild/deny-demote';rank=9} $serviceToken (Ticket $bToken $b)).status -eq 403) 'officer cannot demote peer'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/members/$d/rank" @{idempotencyKey='guild/demote-d';rank=9} $serviceToken (Ticket $aToken $a)).status -eq 200) 'leader assigns lower rank'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/members/$d/rank" @{idempotencyKey='guild/promote-d';rank=2} $serviceToken (Ticket $bToken $b)).status -eq 200) 'officer promotes lower member'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/members/$d/kick" @{idempotencyKey='guild/kick-d'} $serviceToken (Ticket $bToken $b)).status -eq 200) 'officer kicks lower member'
    Check ((GuildRequest 'GET' "v1/guilds/$guildId/roster" $null $serviceToken (Ticket $dToken $d)).status -eq 403) 'kicked member loses roster access'
    $left = GuildRequest 'POST' "v1/guilds/$guildId/leave" @{idempotencyKey='guild/leader-leave'} $serviceToken (Ticket $aToken $a)
    Check ($left.status -eq 200 -and $left.json.members.Count -eq 1 -and $left.json.members[0].characterId -eq $b -and $left.json.members[0].rank -eq 0) 'leader leave transfers leadership'
    Check ((GuildRequest 'POST' "v1/guilds/$guildId/disband" @{idempotencyKey='guild/disband'} $serviceToken (Ticket $bToken $b)).status -eq 200) 'new leader disbands'
    Check ((GuildRequest 'GET' "v1/guild-membership/$b" $null $serviceToken '').json -eq $null) 'disband clears membership lookup'
    Check ((GuildRequest 'POST' "v1/guilds/$($second.json.guildId)/leave" @{idempotencyKey='guild/last-leave'} $serviceToken (Ticket $cToken $c)).status -eq 200) 'last member leave dissolves guild'
    $events = & (Join-Path $pg 'psql.exe') -X -t -A -h 127.0.0.1 -p $port -U fixture_admin -d "guild_$run" -c "SELECT count(*) FROM guild.outbox WHERE topic='domain.guild' AND partition_key='$guildId' AND envelope->>'event_name'='guild.leader.changed'"
    Check ($LASTEXITCODE -eq 0 -and $events.Trim() -eq '1') 'leader event persisted once'
    Write-Output "GUILD_ROSTER_PASS $checks/$checks"
}
finally {
    if ($guildProcess -and -not $guildProcess.HasExited) { Stop-Process -Id $guildProcess.Id -Force; $guildProcess.WaitForExit() }
    if ($identityProcess -and -not $identityProcess.HasExited) { Stop-Process -Id $identityProcess.Id -Force; $identityProcess.WaitForExit() }
    if ($started) { CheckOwned; Run (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-m','immediate','-w','-t','30','stop') }
    foreach ($name in @('PGPASSWORD','GUILD_CONNECTION','GUILD_HOST_PORT','SF_GUILD_TOKEN','GUILD_IDENTITY_URL','GUILD_IDENTITY_SERVER_ID','SF_IDENTITY_GUILD_VERIFIER_KEY','Identity__Connection','Identity__TicketSigningKey','Identity__WorldVerifierKey','Identity__ChatVerifierKey','Identity__GuildVerifierKey','Identity__ListenUrl','Identity__KekDevPath')) { Remove-Item "Env:$name" -ErrorAction SilentlyContinue }
    & $dotnet build-server shutdown | Out-Null
}
