[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17997,
    [ValidateRange(60,600)][int]$TimeoutSeconds = 300
)
# Exchange gRPC plumbing: an editor -server process runs ShatteredFrontier.Backend.Grpc.Exchange against an owned
# Gameplay.Host on owned PostgreSQL (wallet read-back, typed refusal). Transport only; trading flows belong to Trading UI.
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repo 'GameClient\ShatteredFrontier.uproject'
foreach ($required in @($editor,$project)) { if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required input missing: $required" } }
$root = Join-Path $repo ('Artifacts\ExchangeTransport\run-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $root
. (Join-Path $PSScriptRoot 'InventoryLedgerHostFixture.ps1')
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$result = [ordered]@{ status='Running'; scope='Development loopback fixture: Win64 editor -server automation, owned PostgreSQL, Gameplay.Host' }
$fx = @{}
try {
    New-LedgerFixture $fx $repo $root
    $schemas = @('CharacterState.Postgres/Schema.sql','CharacterState.Postgres/Schema.Position.sql','CharacterState.Postgres/Schema.Recipes.sql','Housing.Postgres/Schema.sql','Exchange.Postgres/Schema.sql','Housing.Postgres/Schema.Gameplay.sql','Housing.Postgres/Schema.Geometry.sql','Housing.Postgres/Schema.Construction.sql') | ForEach-Object { Get-Content (Join-Path $repo "BackendServices/$_") -Raw }
    [void](Invoke-LedgerSql $fx ("SET ROLE ledger_owner;`n" + ($schemas -join "`n")) $fx.db)
    [void](Invoke-LedgerSql $fx ('GRANT USAGE ON SCHEMA character_state,housing,exchange TO ledger_svc; ' + ((@('character_state','housing','exchange') | ForEach-Object { "GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA $_ TO ledger_svc;" }) -join ' ')) $fx.db)
    $gameplayProject = Join-Path $repo 'BackendServices/Gameplay.Host/Gameplay.Host.csproj'
    $gameplayOut = Join-Path $fx.base 'gameplay-host'  # private output: another session may be running a host from the shared bin folder
    [void](Invoke-FxOk $fx $fx.dotnet @('restore',$gameplayProject,'--locked-mode','--packages',$fx.nuget,'--verbosity','quiet'))
    [void](Invoke-FxOk $fx $fx.dotnet @('build',$gameplayProject,'--no-restore','--configuration','Release','--verbosity','quiet','--output',$gameplayOut))
    $gameplayPort = Get-FreeTcpPort; $gameplayToken = New-HexSecret; $fx.secrets += $gameplayToken
    $gameplayEnv = @{ GAMEPLAY_HOST_PORT="$gameplayPort"; SF_GAMEPLAY_TOKEN=$gameplayToken; GAMEPLAY_LEDGER_CONNECTION=$fx.connection; GAMEPLAY_CHARACTER_CONNECTION=$fx.connection
        GAMEPLAY_HOUSING_CONNECTION=$fx.connection; GAMEPLAY_EXCHANGE_CONNECTION=$fx.connection; GAMEPLAY_CONTENT_ROOT=(Join-Path $repo 'content') }
    Invoke-WithEnv $gameplayEnv { $script:gameplay = Start-Process -FilePath $fx.dotnet -ArgumentList @(('"'+(Join-Path $gameplayOut 'Gameplay.Host.dll')+'"')) -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $root 'gameplay.stdout.txt') -RedirectStandardError (Join-Path $root 'gameplay.stderr.txt') }
    $null = $gameplay.Handle; $owned.Add($gameplay)
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while (-not ((Test-Path (Join-Path $root 'gameplay.stdout.txt')) -and (Get-Content (Join-Path $root 'gameplay.stdout.txt') -Raw) -match 'GAMEPLAY_HOST_READY')) {
        if ($gameplay.HasExited -or $watch.Elapsed.TotalSeconds -gt 60) { throw 'Gameplay host did not become ready.' }
        Start-Sleep -Milliseconds 200
    }
    $log = Join-Path $root 'exchange-roundtrip.log'; $user = Join-Path $root 'exchange-roundtrip-user'; $null = New-Item -ItemType Directory -Path $user
    $arguments = @(('"'+$project+'"'),'/Game/Frontier/Maps/Greenlands_ConstructionSandbox','-server',('-port='+$Port),'-multihome=127.0.0.1','-unattended','-NullRHI','-nosound','-nop4',
        ('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'),'-ExecCmds="Automation RunTests ShatteredFrontier.Backend.Grpc.Exchange"','-TestExit="Automation Test Queue Empty"')
    Invoke-WithEnv @{ SF_GAMEPLAY_TEST_ENDPOINT="127.0.0.1:$gameplayPort"; SF_GAMEPLAY_TOKEN=$gameplayToken } {
        $script:ue = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $root 'exchange-roundtrip.stdout.txt') -RedirectStandardError (Join-Path $root 'exchange-roundtrip.stderr.txt') }
    $null = $ue.Handle; $owned.Add($ue)
    if (-not $ue.WaitForExit($TimeoutSeconds * 1000)) { throw 'Automation run timed out.' }
    $text = Get-Content -LiteralPath $log -Raw
    if ($text -notmatch 'GRPC_EXCHANGE_OK' -or $text -match 'GRPC_EXCHANGE_FAILED|GRPC_EXCHANGE_SKIPPED|Fatal error:' -or $text -notmatch 'Test Completed\. Result=\{Success\}') { throw 'Exchange round trip did not pass (see exchange-roundtrip.log).' }
    $result.roundTrip = [regex]::Match($text,'GRPC_EXCHANGE_OK[^\r\n]*').Value
    $result.status = 'Passed'
} catch { $result.status = 'Failed'; $result.error = $_.Exception.Message; throw }
finally {
    foreach ($process in $owned) { if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() } }
    Remove-LedgerFixture $fx; $result.cleanup = if ($fx.cleanupFailure) { $fx.cleanupFailure } else { 'removed' }
    $result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($result.status)"
}
