[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17991,
    [ValidateRange(60,600)][int]$TimeoutSeconds = 240
)
# BLD-02 A2 + A7 on the network path: real claims and housing persistence through Gameplay.Host (owned PostgreSQL).
# Phase 1: alpha claims the first Silverbrook build plot, places two pieces and removes one. The UE server then restarts
# while the hosts and database stay up. Phase 2: alpha's piece is rebuilt from Housing in place; bravo is refused on
# alpha's claim until alpha grants build rights, then bravo's placement succeeds. Dev identities use the guarded
# ?SFDevCharacter fixture (-SFDevIdentity on the server). Wall bundles are seeded through the ledger host (fixture only).
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repo 'GameClient\ShatteredFrontier.uproject'
foreach ($required in @($editor,$project)) { if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required input missing: $required" } }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) { throw 'Test port is already in use.' }
$root = Join-Path $repo ('Artifacts\ClaimHousing\run-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $root
. (Join-Path $PSScriptRoot 'InventoryLedgerHostFixture.ps1')
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$result = [ordered]@{ status='Running'; scope='Development loopback fixture: Win64 editor -server + editor clients, owned PostgreSQL, InventoryLedger.Host + Gameplay.Host, guarded dev identities'; runs=@() }
$fx = @{}
$silverbrook = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook?game=/Script/ShatteredFrontier.SFConstructionNetworkGameMode'
$claim = 'clm_greenlands_silverbrook_1'
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra) {
    $log = Join-Path $root ($Name+'.log'); $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $arguments = @(('"'+$project+'"'),('"'+$Url+'"'),'-unattended','-NullRHI','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
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
        if ($Process.HasExited) { throw "$Name exited before $Pattern." }
        Start-Sleep -Milliseconds 300
    }
    throw "Timed out waiting for $Name ($Pattern)."
}
function Read-Log([string]$Name) { Get-Content -LiteralPath (Join-Path $root ($Name+'.log')) -Raw }
function Stop-Owned([Diagnostics.Process]$Process) { if (-not $Process.HasExited) { Stop-Process -Id $Process.Id; $Process.WaitForExit() } }
try {
    New-LedgerFixture $fx $repo $root
    $schemas = @('CharacterState.Postgres/Schema.sql','Housing.Postgres/Schema.sql','Exchange.Postgres/Schema.sql','Housing.Postgres/Schema.Gameplay.sql','Housing.Postgres/Schema.Geometry.sql') | ForEach-Object { Get-Content (Join-Path $repo "BackendServices/$_") -Raw }
    [void](Invoke-LedgerSql $fx ("SET ROLE ledger_owner;`n" + ($schemas -join "`n")) $fx.db)
    [void](Invoke-LedgerSql $fx ('GRANT USAGE ON SCHEMA character_state,housing,exchange TO ledger_svc; ' + ((@('character_state','housing','exchange') | ForEach-Object { "GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA $_ TO ledger_svc;" }) -join ' ')) $fx.db)
    $gameplayProject = Join-Path $repo 'BackendServices/Gameplay.Host/Gameplay.Host.csproj'
    $gameplayOut = Join-Path $fx.base 'gameplay-host'  # private output: another session may be running a host from the shared bin folder
    [void](Invoke-FxOk $fx $fx.dotnet @('restore',$gameplayProject,'--locked-mode','--packages',$fx.nuget,'--verbosity','quiet'))
    [void](Invoke-FxOk $fx $fx.dotnet @('build',$gameplayProject,'--no-restore','--configuration','Release','--verbosity','quiet','--output',$gameplayOut))
    $fx.hostPort = Get-FreeTcpPort; $gameplayPort = Get-FreeTcpPort; $gameplayToken = New-HexSecret; $fx.secrets += $gameplayToken
    Start-LedgerHost $fx ''
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
    # Fixture seed: each piece costs one wall bundle; alpha places two, bravo one. Capacity matches the backpack's typed default.
    foreach ($seed in @(@('alpha',2),@('bravo',1))) {
        $r = Invoke-FxChecked $fx $fx.dotnet @($fx.dll,'seed',"ctn_dev_$($seed[0])","chr_dev_$($seed[0])",'160','item/wall_material_bundle',"$($seed[1])","seed/$($seed[0])") @{ INVENTORY_LEDGER_PORT="$($fx.hostPort)"; SF_INVENTORY_LEDGER_TOKEN=$fx.token }
        if ($r.ExitCode -ne 0 -or $r.Stdout -notmatch 'INVENTORY_LEDGER_SEEDED') { throw "Seeding $($seed[0]) failed: $($r.Stdout)" }
    }
    $ledgerIni = '-ini:Game:[/Script/ShatteredFrontier.SFInventoryLedgerSettings]'; $servicesIni = '-ini:Game:[/Script/ShatteredFrontier.SFGameplayServicesSettings]'
    $serverFlags = @('-server',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox','-GatheringPilot','-SFDevIdentity','-ClaimScenario',
        ($ledgerIni+':bEnabled=True'),($ledgerIni+':Endpoint=127.0.0.1:'+$fx.hostPort),($servicesIni+':bEnabled=True'),($servicesIni+':Endpoint=127.0.0.1:'+$gameplayPort))
    $serverEnv = @{ SF_INVENTORY_LEDGER_TOKEN=$fx.token; SF_GAMEPLAY_TOKEN=$gameplayToken }

    # Phase 1: claim, place two, remove one.
    Invoke-WithEnv $serverEnv { $script:server1 = Start-Owned 'server1' $silverbrook $serverFlags }
    Wait-Log $server1 'server1' "HOUSING_CLAIM_LOADED Claim=$claim Claimed=0"
    $alpha1 = Start-Owned 'alpha1' ("127.0.0.1:$Port"+'?SFDevCharacter=alpha') @('-game','-ClaimScenario=owner')
    Wait-Log $alpha1 'alpha1' "CLAIM_SCENARIO_OWNER_DONE Claim=$claim Members=1"
    Wait-Log $server1 'server1' 'DEV_IDENTITY_BOUND Character=chr_dev_alpha'
    $before = [regex]::Match((Read-Log 'alpha1'),'CLAIM_SCENARIO_OWNER_DONE Claim=\S+ Members=1 Content=(\S+)').Groups[1].Value
    Stop-Owned $alpha1; Stop-Owned $server1

    # Phase 2: a fresh UE server against the same Housing state.
    Invoke-WithEnv $serverEnv { $script:server2 = Start-Owned 'server2' $silverbrook $serverFlags }
    Wait-Log $server2 'server2' "HOUSING_CLAIM_LOADED Claim=$claim Claimed=1 Owner=chr_dev_alpha Structures=1 .* Restored=1"
    $barrier = Join-Path $root 'grant.flag'
    $alpha2 = Start-Owned 'alpha2' ("127.0.0.1:$Port"+'?SFDevCharacter=alpha') @('-game','-ClaimScenario=owner_restart',('-ClaimGrantBarrier="'+$barrier+'"'))
    Wait-Log $alpha2 'alpha2' "CLAIM_SCENARIO_RESTORED Claim=$claim Members=1 Cell=X=0 Y=0 Z=0"
    $bravo = Start-Owned 'bravo' ("127.0.0.1:$Port"+'?SFDevCharacter=bravo') @('-game','-ClaimScenario=guest')
    Wait-Log $bravo 'bravo' "CLAIM_SCENARIO_GUEST_REFUSED Claim=$claim"
    Set-Content -LiteralPath $barrier -Value 'grant' -Encoding ascii
    Wait-Log $alpha2 'alpha2' "CLAIM_SCENARIO_GRANTED Claim=$claim Target=chr_dev_bravo"
    Wait-Log $bravo 'bravo' "CLAIM_SCENARIO_GUEST_BUILT Claim=$claim Cell=X=2 Y=0 Z=0"

    $text = (Read-Log 'server1') + (Read-Log 'server2')
    if ($text -match 'DEV_IDENTITY_IGNORED|CONSTRUCTION_NETWORK_RESTORE_REJECTED|HOUSING_PROJECTION_MISMATCH|Fatal error:|Assertion failed:') { throw 'Server reported an identity, restore or projection failure.' }
    $after = [regex]::Match((Read-Log 'alpha2'),'CLAIM_SCENARIO_RESTORED Claim=\S+ Members=1 Cell=\S+ \S+ \S+ Content=(\S+)').Groups[1].Value
    if (-not $before -or $before -ne $after) { throw "Restored content $after differs from the content before the restart ($before)." }
    $claims = (Invoke-LedgerSql $fx "SELECT count(*)||','||min(owner_id) FROM housing.claims" $fx.db).Trim()
    if ($claims -ne '1,chr_dev_alpha') { throw "Expected one claim owned by chr_dev_alpha, got $claims." }
    $result.contentBeforeAndAfterRestart = $before; $result.claims = $claims
    $result.status = 'Passed'
} catch { $result.status = 'Failed'; $result.error = $_.Exception.Message; throw }
finally {
    foreach ($process in $owned) { if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() } }
    Remove-LedgerFixture $fx; $result.cleanup = if ($fx.cleanupFailure) { $fx.cleanupFailure } else { 'removed' }
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($result.status)"
}
