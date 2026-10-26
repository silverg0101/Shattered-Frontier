[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'InventoryLedgerHostFixture.ps1')
$runRoot = Join-Path $repo ('Artifacts/Persistence/IdentityGatewayCreate/run-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$private = Join-Path $runRoot 'private'
$null = New-Item -ItemType Directory -Path $private -Force
$fx = @{ repo=$repo; private=$private; secrets=@() }
$dotnet = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
$nuget = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/nuget'
$pg = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin'
foreach ($path in @($dotnet,(Join-Path $pg 'initdb.exe'),(Join-Path $pg 'pg_ctl.exe'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required fixture input missing: $path" }
}
$fx.dotnet = $dotnet; $fx.pg = $pg; $fx.pgPort = Get-FreeTcpPort
$fx.db = 'identity_' + [guid]::NewGuid().ToString('N')
$fx.adminPassword = New-HexSecret; $fx.servicePassword = New-HexSecret
$fx.secrets = @($fx.adminPassword,$fx.servicePassword)
$data = Join-Path $private 'data'
$gatewayPort = Get-FreeTcpPort
$gatewayUrl = "http://127.0.0.1:$gatewayPort"
$gateway = $null; $pgStarted = $false
$previousNuget = [Environment]::GetEnvironmentVariable('NUGET_PACKAGES')
$gatewayVars = @('Identity__Connection','Identity__TicketSigningKey','Identity__WorldVerifierKey','Identity__KekDevPath','Identity__ListenUrl','ASPNETCORE_ENVIRONMENT')
$previousGateway = @{}
foreach ($name in $gatewayVars) { $previousGateway[$name] = [Environment]::GetEnvironmentVariable($name) }
function Start-Gateway {
    $env:Identity__Connection = "Host=127.0.0.1;Port=$($fx.pgPort);Database=$($fx.db);Username=identity_service;Password=$($fx.servicePassword);Pooling=false"
    $env:Identity__TicketSigningKey = [Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $env:Identity__WorldVerifierKey = New-HexSecret
    $env:Identity__KekDevPath = Join-Path $private 'kek'
    $env:Identity__ListenUrl = $gatewayUrl
    $env:ASPNETCORE_ENVIRONMENT = 'Development'
    $script:gateway = Start-Process -FilePath $dotnet -ArgumentList ('"'+(Join-Path $runRoot 'build/IdentityGateway.dll')+'"') `
        -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $runRoot 'gateway.stdout.txt') `
        -RedirectStandardError (Join-Path $runRoot 'gateway.stderr.txt')
    $null = $script:gateway.Handle
    $clock = [Diagnostics.Stopwatch]::StartNew()
    while ($clock.Elapsed.TotalSeconds -lt 30) {
        if ($script:gateway.HasExited) { throw 'Gateway exited during startup.' }
        try { $response = Invoke-WebRequest "$gatewayUrl/v1/characters" -SkipHttpErrorCheck -TimeoutSec 1; if ($response.StatusCode -eq 401) { return } }
        catch { Start-Sleep -Milliseconds 200 }
    }
    throw 'Gateway startup timeout.'
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
    [void](Invoke-LedgerSql $fx "REVOKE ALL ON SCHEMA public FROM PUBLIC; GRANT CREATE ON DATABASE $($fx.db) TO identity_owner;" $fx.db)
    $owner = "Host=127.0.0.1;Port=$($fx.pgPort);Database=$($fx.db);Username=fixture_admin;Password=$($fx.adminPassword);Pooling=false"
    $env:NUGET_PACKAGES = $nuget
    & (Join-Path $repo 'tools/Invoke-SchemaMigrations.ps1') -Connection $owner -Role identity_owner -Schemas identity -Dotnet $dotnet -Nuget $nuget | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Identity migration failed.' }
    $installed = Invoke-LedgerSql $fx "SELECT to_regclass('identity.character_create_requests') IS NOT NULL;" $fx.db
    if ($installed.Trim() -ne 't') { throw 'Create receipt migration not installed.' }
    [void](Invoke-LedgerSql $fx 'GRANT USAGE ON SCHEMA identity TO identity_service; GRANT SELECT ON identity.schema_version TO identity_service; GRANT SELECT,INSERT,UPDATE,DELETE ON identity.accounts,identity.sessions,identity.characters,identity.ticket_uses TO identity_service; GRANT SELECT,INSERT ON identity.character_create_requests TO identity_service;' $fx.db)
    [void](Invoke-LedgerSql $fx 'GRANT USAGE ON SCHEMA auth TO identity_service; GRANT SELECT,INSERT,UPDATE,DELETE ON auth.account_data_keys,auth.account_data_key_erasure TO identity_service; GRANT SELECT,INSERT ON auth.data_key_unwraps,auth.erasure_tombstones TO identity_service;' $fx.db)
    $privileges = Invoke-LedgerSql $fx "SELECT has_table_privilege('identity_service','identity.character_create_requests','SELECT'),has_table_privilege('identity_service','identity.character_create_requests','INSERT'),has_table_privilege('identity_service','identity.character_create_requests','DELETE');" $fx.db
    if ($privileges.Trim() -ne 't|t|f') { throw "Unexpected create receipt grants: $privileges" }
    & $dotnet restore (Join-Path $repo 'BackendServices/IdentityGateway/IdentityGateway.csproj') --locked-mode --packages $nuget --verbosity quiet
    if ($LASTEXITCODE -ne 0) { throw 'Gateway restore failed.' }
    $build = Join-Path $runRoot 'build'; $null = New-Item -ItemType Directory -Path $build -Force
    & $dotnet build (Join-Path $repo 'BackendServices/IdentityGateway/IdentityGateway.csproj') --no-restore --configuration Release --output $build --verbosity quiet
    if ($LASTEXITCODE -ne 0) { throw 'Gateway build failed.' }
    [IO.File]::WriteAllBytes((Join-Path $private 'kek'),[Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    Start-Gateway
    $headers = @{ 'Idempotency-Key' = 'account-create' }
    $accountName = 'retry' + [guid]::NewGuid().ToString('N').Substring(0,12)
    $password = New-HexSecret
    $null = Invoke-RestMethod "$gatewayUrl/v1/accounts" -Method Post -Headers $headers -ContentType 'application/json' -Body (@{account_name=$accountName;password=$password} | ConvertTo-Json -Compress)
    $begin = Invoke-RestMethod "$gatewayUrl/v1/login/begin" -Method Post -Headers $headers -ContentType 'application/json' -Body (@{account_name=$accountName;password=$password} | ConvertTo-Json -Compress)
    $complete = Invoke-RestMethod "$gatewayUrl/v1/login/complete" -Method Post -Headers $headers -ContentType 'application/json' -Body (@{login_id=$begin.login_id} | ConvertTo-Json -Compress)
    $headers.Authorization = 'Bearer ' + $complete.session_token
    $headers['Idempotency-Key'] = 'parallel-create'
    $packet = [byte[]]::new(29); $packet[0]=2; $packet[3]=1; $packet[4]=128; $packet[5]=128; $packet[6]=8
    $body = @{realm_id='greenlands';name='Retryhero';appearance_v2_base64=[Convert]::ToBase64String($packet);archetype_id='mage'} | ConvertTo-Json -Compress
    $client = [System.Net.Http.HttpClient]::new()
    $calls = @()
    for ($i=0; $i -lt 12; $i++) {
        $request = [System.Net.Http.HttpRequestMessage]::new([System.Net.Http.HttpMethod]::Post,"$gatewayUrl/v1/characters")
        $request.Headers.TryAddWithoutValidation('Authorization',$headers.Authorization) | Out-Null
        $request.Headers.TryAddWithoutValidation('Idempotency-Key',$headers['Idempotency-Key']) | Out-Null
        $request.Content = [System.Net.Http.StringContent]::new($body,[Text.Encoding]::UTF8,'application/json')
        $calls += $client.SendAsync($request)
    }
    [Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]$calls)
    $ids = @()
    foreach ($call in $calls) {
        if ([int]$call.Result.StatusCode -ne 201) { throw "Concurrent create returned $($call.Result.StatusCode)" }
        $ids += (($call.Result.Content.ReadAsStringAsync().GetAwaiter().GetResult() | ConvertFrom-Json).character_id)
    }
    if (@($ids | Select-Object -Unique).Count -ne 1) { throw 'Concurrent create produced multiple IDs.' }
    $rows = Invoke-LedgerSql $fx 'SELECT (SELECT count(*) FROM identity.characters),(SELECT count(*) FROM identity.character_create_requests);' $fx.db
    if ($rows.Trim() -ne '1|1') { throw "Create and receipt count mismatch: $rows" }
    Write-Output 'PASS PostgreSQL migration/grants and 12 concurrent creates (one character, one receipt)'
    $changed = $body.Replace('Retryhero','Different')
    $conflict = Invoke-WebRequest "$gatewayUrl/v1/characters" -Method Post -Headers $headers -ContentType 'application/json' -Body $changed -SkipHttpErrorCheck
    if ($conflict.StatusCode -ne 409 -or ($conflict.Content | ConvertFrom-Json).reason -ne 'IDEMPOTENCY_KEY_CONFLICT') { throw 'Changed payload did not conflict.' }
    Write-Output 'PASS changed payload conflicts'
    Stop-Process -Id $gateway.Id -Force; $gateway.WaitForExit()
    Start-Gateway
    $replay = Invoke-WebRequest "$gatewayUrl/v1/characters" -Method Post -Headers $headers -ContentType 'application/json' -Body $body -SkipHttpErrorCheck
    if ($replay.StatusCode -ne 201 -or ($replay.Content | ConvertFrom-Json).character_id -ne $ids[0]) { throw 'Restart replay changed response.' }
    Write-Output 'PASS gateway restart replays original 201 response'
    $client.Dispose()
}
finally {
    if ($gateway -and -not $gateway.HasExited) { Stop-Process -Id $gateway.Id -Force -ErrorAction SilentlyContinue; $gateway.WaitForExit() }
    if ($pgStarted) { [void](Invoke-FxChecked $fx (Join-Path $pg 'pg_ctl.exe') @('-D',$data,'-m','immediate','-w','-t','30','stop')) }
    [Environment]::SetEnvironmentVariable('NUGET_PACKAGES',$previousNuget)
    foreach ($name in $gatewayVars) { [Environment]::SetEnvironmentVariable($name,$previousGateway[$name]) }
    & $dotnet build-server shutdown | Out-Null
}
