[CmdletBinding()]
param(
    # Gathering: two clients harvest in Silverbrook; Combat: one client kills a boar (quest started at the boar step).
    [Parameter(Mandatory)][ValidateSet('Gathering','Combat','Unified','Full','Plain')][string]$Leg,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17871,
    [ValidateRange(60,600)][int]$TimeoutSeconds = 240,
    # -Leg Full only: commit through owned InventoryLedger.Host + Gameplay.Host (INT-04), with a host-down phase.
    [switch]$Services
)
# The gathering and combat pilots run in different game modes, so each leg proves its own events reach the quest
# (real server-side harvest/boar outcomes, no synthetic events) and the owning client's tracker view follows.
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repo 'GameClient\ShatteredFrontier.uproject'
foreach ($required in @($editor,$project)) { if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required input missing: $required" } }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) { throw 'Test port is already in use.' }
$root = Join-Path $repo ('Artifacts\QuestPilot\' + $Leg.ToLower() + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $root
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
if ($Services -and $Leg -ne 'Full') { throw '-Services applies to -Leg Full only.' }
$fx = @{}
if ($Services) { . (Join-Path $PSScriptRoot 'InventoryLedgerHostFixture.ps1') }
$result = [ordered]@{ status='Running'; leg=$Leg; runs=@() }
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
        if ($Process.HasExited) { throw "$Name exited before $Pattern." }
        Start-Sleep -Milliseconds 300
    }
    throw "Timed out waiting for $Name ($Pattern)."
}
function Read-Log([string]$Name) { Get-Content -LiteralPath (Join-Path $root ($Name+'.log')) -Raw }
try {
    if ($Leg -in 'Gathering','Unified') {
        # Unified: the combat game mode hosts gathering, construction and combat together (one Silverbrook world).
        $mode = if ($Leg -eq 'Unified') { 'SFCombatPilotGameMode' } else { 'SFConstructionNetworkGameMode' }
        $flags = @('-server',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox','-GatheringPilot','-GatheringPilotScenario','-QuestPilot')
        if ($Leg -eq 'Unified') { $flags += '-CombatPilot' }
        $server = Start-Owned 'server' ($silverbrook + '?game=/Script/ShatteredFrontier.' + $mode) $flags
        Wait-Log $server 'server' 'QUEST_PILOT_READY Quest=quest/frontier_founder Steps=5'
        if ($Leg -eq 'Unified') {
            Wait-Log $server 'server' 'COMBAT_PILOT_READY Boars=10'
            Wait-Log $server 'server' 'GATHERING_PILOT_READY Source=ZoneRecord Nodes=[1-9]'
        }
        $first = Start-Owned 'first' ('127.0.0.1:'+$Port) @('-game','-GatheringPilotScenario')
        $second = Start-Owned 'second' ('127.0.0.1:'+$Port) @('-game','-GatheringPilotScenario')
        Wait-Log $server 'server' 'QUEST_PILOT_EVENT Kind=gather Target=item/oak_log .* Result=0 Step=1 '
        Wait-Log $server 'server' 'QUEST_PILOT_EVENT Kind=gather Target=item/iron_ore .* Result=2 Step=0 '  # out of order: ignored, never credited
        # Only the owner of the oak harvest sees its tracker move on; the other client's view must stay at step 1.
        $watch = [Diagnostics.Stopwatch]::StartNew(); $moved = @()
        while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds -and $moved.Count -lt 1) {
            $moved = @('first','second' | Where-Object { (Test-Path (Join-Path $root "$_.log")) -and (Read-Log $_) -match 'QUEST_PILOT_CLIENT_VIEW Step=2' })
            Start-Sleep -Milliseconds 300
        }
        if ($moved.Count -ne 1) { throw "Expected exactly one client tracker at step 2, got $($moved.Count)." }
    } elseif ($Leg -eq 'Plain') {
        # Regression: a plain Silverbrook gathering client with every service setting off gets construction state for
        # every claim plot and an enabled gathering pilot (what the inventory key needs), as before claims existed.
        $server = Start-Owned 'server' ($silverbrook + '?game=/Script/ShatteredFrontier.SFConstructionNetworkGameMode') @('-server',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox','-GatheringPilot')
        Wait-Log $server 'server' 'GATHERING_PILOT_READY Source=ZoneRecord'
        $client = Start-Owned 'client' ('127.0.0.1:'+$Port) @('-game','-PlainClientCheck')
        Wait-Log $client 'client' 'PLAIN_CLIENT_READY States=3 Ready=3 GatheringEnabled=1 InventoryKeyAllowed=1'
    } elseif ($Leg -eq 'Full') {
        # One client walks the chain through its normal entry points (the server only places the pawn beside each target).
        # -Services: harvests, crafts, the wall, the boar reward and quest progress commit through InventoryLedger.Host and
        # Gameplay.Host on an owned PostgreSQL (INT-04); the wall then costs a crafted wall bundle, so the client adds the
        # economy steps. The hosts start down: the first harvest must be refused and leave the node unchanged.
        $flags = @('-server',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox','-GatheringPilot','-CombatPilot','-QuestPilot','-QuestFullScenario')
        $clientFlags = @('-game','-QuestFullScenario')
        $serverEnv = @{}
        if ($Services) {
            New-LedgerFixture $fx $repo $root
            $schemas = @('CharacterState.Postgres/Schema.sql','Housing.Postgres/Schema.sql','Exchange.Postgres/Schema.sql','Housing.Postgres/Schema.Gameplay.sql','Housing.Postgres/Schema.Geometry.sql','CharacterState.Postgres/Schema.Position.sql') | ForEach-Object { Get-Content (Join-Path $repo "BackendServices/$_") -Raw }
            [void](Invoke-LedgerSql $fx ("SET ROLE ledger_owner;`n" + ($schemas -join "`n")) $fx.db)
            [void](Invoke-LedgerSql $fx ('GRANT USAGE ON SCHEMA character_state,housing,exchange TO ledger_svc; ' + (@('character_state','housing','exchange') | ForEach-Object { "GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA $_ TO ledger_svc;" }) -join ' ') $fx.db)
            $gameplayProject = Join-Path $repo 'BackendServices/Gameplay.Host/Gameplay.Host.csproj'
            [void](Invoke-FxOk $fx $fx.dotnet @('restore',$gameplayProject,'--locked-mode','--packages',$fx.nuget,'--verbosity','quiet'))
            # A private output folder: another session may be running a host from the shared bin folder.
            $gameplayOut = Join-Path $fx.base 'gameplay-host'
            [void](Invoke-FxOk $fx $fx.dotnet @('build',$gameplayProject,'--no-restore','--configuration','Release','--verbosity','quiet','--output',$gameplayOut))
            $fx.hostPort = Get-FreeTcpPort; $gameplayPort = Get-FreeTcpPort; $gameplayToken = New-HexSecret; $fx.secrets += $gameplayToken
            $ledgerIni = '-ini:Game:[/Script/ShatteredFrontier.SFInventoryLedgerSettings]'; $servicesIni = '-ini:Game:[/Script/ShatteredFrontier.SFGameplayServicesSettings]'
            $flags += @(($ledgerIni+':bEnabled=True'),($ledgerIni+':Endpoint=127.0.0.1:'+$fx.hostPort),($servicesIni+':bEnabled=True'),($servicesIni+':Endpoint=127.0.0.1:'+$gameplayPort))
            $clientFlags += '-QuestFullEconomy'
            $serverEnv = @{ SF_INVENTORY_LEDGER_TOKEN=$fx.token; SF_GAMEPLAY_TOKEN=$gameplayToken }
        }
        $url = $silverbrook + '?game=/Script/ShatteredFrontier.SFCombatPilotGameMode'
        if ($Services) { Invoke-WithEnv $serverEnv { $script:server = Start-Owned 'server' $url $flags } } else { $server = Start-Owned 'server' $url $flags }
        Wait-Log $server 'server' 'QUEST_PILOT_READY Quest=quest/frontier_founder Steps=5'
        Wait-Log $server 'server' 'COMBAT_PILOT_READY Boars=10'
        $client = Start-Owned 'client' ('127.0.0.1:'+$Port) $clientFlags
        if ($Services) {
            # Host-down phase: the first oak harvest is refused as Unavailable before any charge is consumed.
            Wait-Log $server 'server' 'GATHERING_LEDGER_OUTCOME .* Code=14 .* NodeRevision=0 Charges=3'
            $result.hostDown = 'first harvest refused; node revision 0, charges 3'
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
        }
        Wait-Log $server 'server' 'QUEST_PILOT_EVENT Kind=gather Target=item/oak_log .* Result=0 Step=1 '
        Wait-Log $server 'server' 'QUEST_PILOT_EVENT Kind=gather Target=item/iron_ore .* Result=0 Step=2 '
        Wait-Log $server 'server' 'QUEST_PILOT_EVENT Kind=craft Target=item/rough_timber .* Result=0 Step=2 '
        Wait-Log $server 'server' 'QUEST_PILOT_EVENT Kind=craft Target=item/iron_ingot .* Result=0 Step=3 '
        Wait-Log $server 'server' 'QUEST_PILOT_EVENT Kind=build Target=structure_tag/wall .* Result=0 Step=4 '  # a real anchored placement on the Silverbrook claim pad
        Wait-Log $server 'server' 'QUEST_PILOT_EVENT Kind=defeat Target=npc/boar .* Result=0 '
        Wait-Log $server 'server' 'QUEST_PILOT_COMPLETE Quest=quest/frontier_founder'
        Wait-Log $server 'server' 'QUEST_PILOT_REWARD Bundle=reward/frontier_founder_completion'
        Wait-Log $client 'client' 'QUEST_FULL_DONE'
        Start-Sleep -Seconds 3
        if ([regex]::Matches((Read-Log 'server'),'QUEST_PILOT_REWARD Bundle=').Count -ne 1) { throw 'Chain reward was not granted exactly once.' }
        if ((Read-Log 'client') -match 'QUEST_FULL_FAILED') { throw 'Client driver reported a failure.' }
        if ($Services) {
            # The durable stores, read back once the run is idle: one of everything the chain produced.
            $text = Read-Log 'server'
            if ($text -notmatch 'BACKPACK_OUTCOME Label=gather Op=\S+ Outcome=0 Container=(ctn_dev_\w+)') { throw 'No committed backpack harvest.' }
            $container = $Matches[1]; $character = 'chr_dev_' + $container.Substring(8)
            $view = Get-LedgerView $fx $container
            # What the economy leaves after the wall, plus the quest payout lines the service reported once.
            $expected = [ordered]@{ 'item/oak_log'=0; 'item/rough_timber'=2; 'item/iron_ore'=0; 'item/iron_ingot'=0; 'item/iron_nails'=4; 'item/wall_material_bundle'=0 }
            $payout = [regex]::Matches($text,'QUEST_PILOT_PAYOUT Item=(\S+) Quantity=(\d+)')
            if ($payout.Count -lt 1) { throw 'No quest payout lines were granted.' }
            foreach ($line in $payout) { $expected[$line.Groups[1].Value] = [int64]$expected[$line.Groups[1].Value] + [int64]$line.Groups[2].Value }
            foreach ($item in $expected.Keys) { if ([int64]($view.Items[$item]) -ne $expected[$item]) { throw "Ledger $item=$($view.Items[$item]), expected $($expected[$item])." } }
            if ([int64]$view.Items['item/boar_hide'] -lt 2) { throw 'The boar reward hides are missing.' }
            $counts = Invoke-LedgerSql $fx ("SELECT (SELECT count(*) FROM character_state.source_receipts WHERE domain='progression' AND character_id='$character')||','||" +
                "(SELECT count(*) FROM character_state.reward_receipts WHERE character_id='$character' AND state='Complete')||','||" +
                "(SELECT count(*) FROM character_state.reward_receipts WHERE character_id='$character')||','||" +
                "(SELECT count(*) FROM housing.placements WHERE state='Committed')||','||" +
                "(SELECT count(*) FROM character_state.quests WHERE character_id='$character')") $fx.db
            # Progression receipts: oak x2, iron, sawmill x2, forge ingot, forge nails, workbench, boar defeat.
            if ($counts.Trim() -ne '9,1,1,1,1') { throw "Durable counts (progression,payout complete,payouts,placements,quests) = $counts, expected 9,1,1,1,1." }
            $result.ledger = $view.Line; $result.durableCounts = $counts.Trim()
        }
    } else {
        $url = $silverbrook + '?game=/Script/ShatteredFrontier.SFCombatPilotGameMode'
        $server = Start-Owned 'server' $url @('-server',('-port='+$Port),'-multihome=127.0.0.1','-CombatPilot','-CombatPilotScenario','-QuestPilot','-QuestPilotStartStep=defeat_boar')
        Wait-Log $server 'server' 'QUEST_PILOT_READY Quest=quest/frontier_founder Steps=5'
        $client = Start-Owned 'client' ('127.0.0.1:'+$Port) @('-game','-CombatPilotScenario')
        Wait-Log $server 'server' 'QUEST_PILOT_EVENT Kind=defeat Target=npc/boar Op=boar:boar_farm_edge:0:\d+ Result=0'
        Wait-Log $server 'server' 'QUEST_PILOT_COMPLETE Quest=quest/frontier_founder'
        Wait-Log $server 'server' 'QUEST_PILOT_REWARD Bundle=reward/frontier_founder_completion'
        Wait-Log $client 'client' 'QUEST_PILOT_CLIENT_VIEW Step=5 Complete=1'
        Start-Sleep -Seconds 5  # a replayed or second credit would show up now
        $text = Read-Log 'server'
        if ([regex]::Matches($text,'QUEST_PILOT_REWARD Bundle=').Count -ne 1) { throw 'Chain reward was not granted exactly once.' }
    }
    $text = Read-Log 'server'
    if ($text -match 'QUEST_PILOT_DISABLED|Fatal error:|Assertion failed:') { throw 'Quest pilot disabled or server failed.' }
    $result.status = 'Passed'
} catch { $result.status = 'Failed'; $result.error = $_.Exception.Message; throw }
finally {
    foreach ($process in $owned) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
        foreach ($run in $result.runs) { if ($run.pid -eq $process.Id) { $run.exitCode = $process.ExitCode } }
    }
    if ($Services) { Remove-LedgerFixture $fx; $result.cleanup = if ($fx.cleanupFailure) { $fx.cleanupFailure } else { 'removed' } }
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($result.status)"
}
