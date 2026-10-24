param([Parameter(Mandatory)][string]$Dotnet, [Parameter(Mandatory)][string]$Connection)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$evidence = Join-Path $repo 'Artifacts/Persistence/IdentityGatewayFixture'
New-Item -ItemType Directory -Force $evidence | Out-Null
$previousConnection = $env:Identity__Connection
$previousKey = $env:Identity__TicketSigningKey
$previousVerifierKey = $env:Identity__WorldVerifierKey
$env:Identity__Connection = $Connection
$env:Identity__TicketSigningKey = [Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
$env:Identity__WorldVerifierKey = [Guid]::NewGuid().ToString('N') + [Guid]::NewGuid().ToString('N')
$hostProcess = $null
try {
    $hostProcess = Start-Process -FilePath $Dotnet -ArgumentList 'BackendServices/IdentityGateway/bin/Release/net10.0/IdentityGateway.dll' `
        -WorkingDirectory $repo -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $evidence 'host.out') -RedirectStandardError (Join-Path $evidence 'host.err')
    $url = 'http://127.0.0.1:5084'
    $ready = $false
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Milliseconds 200
        try { $response = Invoke-WebRequest "$url/v1/characters" -SkipHttpErrorCheck; if ($response.StatusCode -eq 401) { $ready = $true; break } } catch { }
    }
    if (-not $ready) { throw 'Identity host did not become ready.' }
    $headers = @{ 'Idempotency-Key' = 'smoke-1' }
    $name = 'test' + [Guid]::NewGuid().ToString('N').Substring(0, 12)
    $password = 'development-test-password'
    $account = Invoke-RestMethod "$url/v1/accounts" -Method Post -Headers $headers -ContentType 'application/json' -Body (@{ account_name=$name; password=$password } | ConvertTo-Json)
    $begin = Invoke-RestMethod "$url/v1/login/begin" -Method Post -Headers $headers -ContentType 'application/json' -Body (@{ account_name=$name; password=$password } | ConvertTo-Json)
    $complete = Invoke-RestMethod "$url/v1/login/complete" -Method Post -Headers $headers -ContentType 'application/json' -Body (@{ login_id=$begin.login_id } | ConvertTo-Json)
    $headers.Authorization = 'Bearer ' + $complete.session_token
    $packet = [byte[]]::new(29)
    $packet[0] = 2; $packet[3] = 1; $packet[4] = 128; $packet[5] = 128; $packet[6] = 8
    $characterName = 'Test' + (-join (1..8 | ForEach-Object { [char](Get-Random -Minimum 97 -Maximum 123) }))
    $character = Invoke-RestMethod "$url/v1/characters" -Method Post -Headers $headers -ContentType 'application/json' `
        -Body (@{ realm_id='greenlands'; name=$characterName; appearance_v2_base64=[Convert]::ToBase64String($packet) } | ConvertTo-Json)
    $list = Invoke-RestMethod "$url/v1/characters" -Headers $headers
    $entry = Invoke-RestMethod "$url/v1/characters/$($character.character_id)/enter-world" -Method Post -Headers $headers -ContentType 'application/json' -Body '{}'
    if (-not $account.account_id -or $list.characters.Count -ne 1 -or -not $entry.ticket) { throw 'Identity HTTP flow failed.' }
    $verifyHeaders = @{ 'X-World-Verifier-Key' = $env:Identity__WorldVerifierKey }
    $first = Invoke-RestMethod "$url/v1/world/verify-ticket" -Method Post -Headers $verifyHeaders -ContentType 'application/json' -Body (@{ticket=$entry.ticket;server_id='srvi_dev_local'} | ConvertTo-Json)
    $replay = Invoke-WebRequest "$url/v1/world/verify-ticket" -Method Post -Headers $verifyHeaders -ContentType 'application/json' -Body (@{ticket=$entry.ticket;server_id='srvi_dev_local'} | ConvertTo-Json) -SkipHttpErrorCheck
    if ($first.character_id -ne $character.character_id -or $replay.StatusCode -ne 401) { throw 'World ticket verification or replay guard failed.' }
    Invoke-RestMethod "$url/v1/characters/$($character.character_id)" -Method Delete -Headers $headers | Out-Null
    $after = Invoke-RestMethod "$url/v1/characters" -Headers $headers
    if ($after.characters.Count -ne 0) { throw 'Character deletion failed.' }
    'PASS HTTP account/login/character/enter-world on PostgreSQL'
}
finally {
    if ($hostProcess) { Stop-Process -Id $hostProcess.Id -ErrorAction SilentlyContinue }
    $env:Identity__Connection = $previousConnection
    $env:Identity__TicketSigningKey = $previousKey
    $env:Identity__WorldVerifierKey = $previousVerifierKey
}
