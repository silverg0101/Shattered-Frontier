[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17881,
    [ValidateRange(60,900)][int]$TimeoutSeconds = 300
)
# Two-client Exchange run on an owned PostgreSQL, InventoryLedger.Host and Gameplay.Host (same fixture as
# Test-QuestPilot -Leg Full -Services). Two clients connect to Silverbrook; the server's -ExchangeScenario driver plays both
# players through the real intent path (USFExchangeComponent::ExecuteIntent -> Gameplay.Host Exchange):
#   A offers two oak logs for ten gold, B accepts; A lists one iron ore at five gold, B buys it;
#   then the accept and the buy are replayed with the same derived operation ids.
# The runner seeds the wallets and the escrow containers (after the scenario logs the character ids, before it opens the gate
# file) and then checks the database: exactly one trade and one listing settlement, items delivered once, and B's wallet
# unchanged by the replay. Needs the Backend module's Exchange transport (INT-05) and a build with these flags.
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repo 'GameClient\ShatteredFrontier.uproject'
foreach ($required in @($editor,$project)) { if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required input missing: $required" } }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) { throw 'Test port is already in use.' }
$root = Join-Path $repo ('Artifacts\ExchangeTrade\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $root
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$fx = @{}
. (Join-Path $PSScriptRoot 'InventoryLedgerHostFixture.ps1')
$result = [ordered]@{ status='Running'; runs=@() }
$silverbrook = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra) {
    $log = Join-Path $root ($Name+'.log'); $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $arguments = @(('"'+$project+'"'),$Url,'-unattended','-NullRHI','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt'))
    $null = $process.Handle; $owned.Add($process)
    $result.runs += [ordered]@{ name=$Name; pid=$process.Id; log=$log; arguments=$arguments }
    return $process
}
function Wait-Log([Diagnostics.Process]$Process,[string]$Name,[string]$Pattern) {
    $watch = [Diagnostics.Stopwatch]::StartNew(); $log = Join-Path $root ($Name+'.log')
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern $Pattern -Quiet)) { return }
        if ($Pattern -like 'EXCHANGE_SCENARIO_DONE*' -and (Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern 'EXCHANGE_SCENARIO_FAILED' -Quiet)) { throw 'Scenario reported a failure.' }
        if ($Process.HasExited) { throw "$Name exited before $Pattern." }
        Start-Sleep -Milliseconds 300
    }
    throw "Timed out waiting for $Name ($Pattern)."
}
function Read-Log([string]$Name) { Get-Content -LiteralPath (Join-Path $root ($Name+'.log')) -Raw }
function Query([string]$Sql) { return (Invoke-LedgerSql $fx $Sql $fx.db).Trim() }
try {
    New-LedgerFixture $fx $repo $root
    $schemas = @('CharacterState.Postgres/Schema.sql','Housing.Postgres/Schema.sql','Exchange.Postgres/Schema.sql','Housing.Postgres/Schema.Gameplay.sql','Housing.Postgres/Schema.Geometry.sql','CharacterState.Postgres/Schema.Position.sql') | ForEach-Object { Get-Content (Join-Path $repo "BackendServices/$_") -Raw }
    [void](Invoke-LedgerSql $fx ("SET ROLE ledger_owner;`n" + ($schemas -join "`n")) $fx.db)
    [void](Invoke-LedgerSql $fx ('GRANT USAGE ON SCHEMA character_state,housing,exchange TO ledger_svc; ' + (@('character_state','housing','exchange') | ForEach-Object { "GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA $_ TO ledger_svc;" }) -join ' ') $fx.db)
    $gameplayProject = Join-Path $repo 'BackendServices/Gameplay.Host/Gameplay.Host.csproj'
    [void](Invoke-FxOk $fx $fx.dotnet @('restore',$gameplayProject,'--locked-mode','--packages',$fx.nuget,'--verbosity','quiet'))
    $gameplayOut = Join-Path $fx.base 'gameplay-host'  # private output: another session may run a host from the shared bin folder
    [void](Invoke-FxOk $fx $fx.dotnet @('build',$gameplayProject,'--no-restore','--configuration','Release','--verbosity','quiet','--output',$gameplayOut))
    $fx.hostPort = Get-FreeTcpPort; $gameplayPort = Get-FreeTcpPort; $gameplayToken = New-HexSecret; $fx.secrets += $gameplayToken
    Start-LedgerHost $fx ''
    $gameplayEnv = @{ GAMEPLAY_HOST_PORT="$gameplayPort"; SF_GAMEPLAY_TOKEN=$gameplayToken; GAMEPLAY_LEDGER_CONNECTION=$fx.connection; GAMEPLAY_CHARACTER_CONNECTION=$fx.connection
        GAMEPLAY_HOUSING_CONNECTION=$fx.connection; GAMEPLAY_EXCHANGE_CONNECTION=$fx.connection; GAMEPLAY_CONTENT_ROOT=(Join-Path $repo 'content') }
    $gameplayDll = Join-Path $gameplayOut 'Gameplay.Host.dll'
    Invoke-WithEnv $gameplayEnv { $script:gameplay = Start-Process -FilePath $fx.dotnet -ArgumentList @(('"'+$gameplayDll+'"')) -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $root 'gameplay.stdout.txt') -RedirectStandardError (Join-Path $root 'gameplay.stderr.txt') }
    $null = $gameplay.Handle; $owned.Add($gameplay)
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while (-not ((Test-Path (Join-Path $root 'gameplay.stdout.txt')) -and (Get-Content (Join-Path $root 'gameplay.stdout.txt') -Raw) -match 'GAMEPLAY_HOST_READY')) {
        if ($gameplay.HasExited -or $watch.Elapsed.TotalSeconds -gt 60) { throw 'Gameplay host did not become ready.' }
        Start-Sleep -Milliseconds 200
    }

    $gate = Join-Path $root 'scenario.gate'
    $ledgerIni = '-ini:Game:[/Script/ShatteredFrontier.SFInventoryLedgerSettings]'; $servicesIni = '-ini:Game:[/Script/ShatteredFrontier.SFGameplayServicesSettings]'
    $flags = @('-server',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox','-GatheringPilot','-ExchangeScenario',('-ExchangeScenarioGate="'+$gate+'"'),
        ($ledgerIni+':bEnabled=True'),($ledgerIni+':Endpoint=127.0.0.1:'+$fx.hostPort),($servicesIni+':bEnabled=True'),($servicesIni+':bExchangeEnabled=True'),($servicesIni+':Endpoint=127.0.0.1:'+$gameplayPort))
    $serverEnv = @{ SF_INVENTORY_LEDGER_TOKEN=$fx.token; SF_GAMEPLAY_TOKEN=$gameplayToken }
    Invoke-WithEnv $serverEnv { $script:server = Start-Owned 'server' ($silverbrook + '?game=/Script/ShatteredFrontier.SFGameMode') $flags }
    Wait-Log $server 'server' 'GATHERING_PILOT_READY'
    $first = Start-Owned 'first' ('127.0.0.1:'+$Port) @('-game')
    $second = Start-Owned 'second' ('127.0.0.1:'+$Port) @('-game')
    Wait-Log $server 'server' 'EXCHANGE_SCENARIO_START'
    $start = [regex]::Match((Read-Log 'server'),'EXCHANGE_SCENARIO_START A=(\S+) ABag=(\S+) B=(\S+) BBag=(\S+)')
    if (-not $start.Success) { throw 'Scenario did not announce both players.' }
    $a = $start.Groups[1].Value; $b = $start.Groups[3].Value
    if ($a -notmatch '^chr_dev_[a-z0-9_]+$' -or $b -notmatch '^chr_dev_[a-z0-9_]+$' -or $a -eq $b) { throw 'Scenario announced invalid character identities.' }
    # BindIdentity may rebuild its FString while UE_LOG evaluates the bag arguments; derive the stable dev IDs from the characters.
    $aBag = 'ctn_dev_' + $a.Substring(8); $bBag = 'ctn_dev_' + $b.Substring(8)
    # The fixture creates character-owned backpacks first, then trade escrow with the same owner and capacity.
    # Listing escrow is created by Gameplay.Host on first listing use. Capacity 160 is the shipped 8 slots x 20 units.
    [void](Invoke-LedgerSql $fx ("INSERT INTO exchange.wallets(owner_id,balance) VALUES ('$a',0),('$b',100) ON CONFLICT DO NOTHING; " +
        "INSERT INTO inventory.containers(id,owner_kind,owner_id,kind,capacity) VALUES ('$aBag',0,'$a',1,160),('$bBag',0,'$b',1,160) ON CONFLICT DO NOTHING; " +
        "INSERT INTO inventory.containers(id,owner_kind,owner_id,kind,capacity) SELECT id||':escrow',owner_kind,owner_id,7,capacity FROM inventory.containers WHERE id IN ('$aBag','$bBag') ON CONFLICT DO NOTHING; " +
        "INSERT INTO inventory.entries(container_id,definition_id,quantity) VALUES ('$($aBag):escrow','item/oak_log',2),('$aBag','item/iron_ore',1) ON CONFLICT DO NOTHING;") $fx.db)
    Set-Content -LiteralPath $gate -Value 'go'
    Wait-Log $server 'server' 'EXCHANGE_SCENARIO_DONE Trade=1 Listing=1 ReplayUnchanged=1'
    if ((Read-Log 'server') -match 'EXCHANGE_SCENARIO_FAILED') { throw 'Scenario reported a failure.' }
    Start-Sleep -Seconds 3  # a duplicated settlement would show up now

    $trades = [int](Query "SELECT count(*) FROM exchange.settlements WHERE kind='trade'")
    $listings = [int](Query "SELECT count(*) FROM exchange.settlements WHERE kind='listing'")
    if ($trades -ne 1 -or $listings -ne 1) { throw "Expected one trade and one listing settlement, found $trades and $listings." }
    $oak = [int](Query "SELECT COALESCE(sum(quantity),0) FROM inventory.entries WHERE container_id='$bBag' AND definition_id='item/oak_log'")
    $ore = [int](Query "SELECT COALESCE(sum(quantity),0) FROM inventory.entries WHERE container_id='$bBag' AND definition_id='item/iron_ore'")
    if ($oak -ne 2 -or $ore -ne 1) { throw "B should hold exactly 2 oak logs and 1 iron ore, found $oak and $ore." }
    $left = [int](Query "SELECT COALESCE(sum(quantity),0) FROM inventory.entries WHERE container_id='$aBag' AND definition_id IN ('item/oak_log','item/iron_ore')")
    if ($left -ne 0) { throw "A should hold none of the traded items, found $left." }
    $result.evidence = [ordered]@{ tradeSettlements=$trades; listingSettlements=$listings; bOakLogs=$oak; bIronOre=$ore; aRemaining=$left
        bBalance=[int64](Query "SELECT balance FROM exchange.wallets WHERE owner_id='$b'"); aBalance=[int64](Query "SELECT balance FROM exchange.wallets WHERE owner_id='$a'") }
    if ((Read-Log 'server') -match 'Fatal error:|Assertion failed:') { throw 'Server failed.' }
    $result.status = 'Passed'
} catch { $result.status = 'Failed'; $result.error = $_.Exception.Message; throw }
finally {
    foreach ($process in $owned) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
        foreach ($run in $result.runs) { if ($run.pid -eq $process.Id) { $run.exitCode = $process.ExitCode } }
    }
    Remove-LedgerFixture $fx; $result.cleanup = if ($fx.cleanupFailure) { $fx.cleanupFailure } else { 'removed' }
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($result.status)"
}
