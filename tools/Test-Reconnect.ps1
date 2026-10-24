[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17993,
    [ValidateRange(60,900)][int]$TimeoutSeconds = 300,
    [ValidateSet('FullQuest','VitalRestore')][string]$Mode = 'FullQuest'
)
# INT-05 reconnect and character restore on owned PostgreSQL with InventoryLedger.Host + Gameplay.Host. Alpha walks the
# Full quest chain (Test-QuestPilot -Leg Full -Services economy); bravo idles beside it. The server drops alpha's
# connection once with its first harvest's ledger grant in flight; alpha reconnects and is restored. After the iron ingot
# the UE server restarts (hosts and database stay up); both reconnect, alpha's tracker resumes at its step and position,
# and the chain finishes. The durable stores must hold exactly one of everything the chain produced.
# -Mode VitalRestore seeds a non-default face and a skill roll, then travels the client after a real death to check restore and respawn.
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repo 'GameClient\ShatteredFrontier.uproject'
foreach ($required in @($editor,$project)) { if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required input missing: $required" } }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) { throw 'Test port is already in use.' }
$root = Join-Path $repo ('Artifacts\Reconnect\run-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $root
. (Join-Path $PSScriptRoot 'InventoryLedgerHostFixture.ps1')
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$result = [ordered]@{ status='Running'; scope='Development loopback fixture: Win64 editor -server + editor clients, owned PostgreSQL, InventoryLedger.Host + Gameplay.Host, guarded dev identities'; runs=@() }
$fx = @{}
$silverbrook = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
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
# Waits until the log holds at least $Count matches of $Pattern.
function Wait-Log([Diagnostics.Process]$Process,[string]$Name,[string]$Pattern,[int]$Count = 1) {
    $watch = [Diagnostics.Stopwatch]::StartNew(); $log = Join-Path $root ($Name+'.log')
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ((Test-Path -LiteralPath $log) -and @(Select-String -LiteralPath $log -Pattern $Pattern).Count -ge $Count) { return }
        if ($Process.HasExited) { throw "$Name exited before $Pattern." }
        Start-Sleep -Milliseconds 300
    }
    throw "Timed out waiting for $Name ($Pattern)."
}
function Read-Log([string]$Name) { Get-Content -LiteralPath (Join-Path $root ($Name+'.log')) -Raw }
function Stop-Owned([Diagnostics.Process]$Process) { if (-not $Process.HasExited) { Stop-Process -Id $Process.Id; $Process.WaitForExit() } }
function Last-Match([string]$Text,[string]$Pattern) { $m = [regex]::Matches($Text,$Pattern); if ($m.Count) { $m[$m.Count-1] } else { $null } }
try {
    New-LedgerFixture $fx $repo $root
    $schemas = @('CharacterState.Postgres/Schema.sql','CharacterState.Postgres/Schema.Position.sql','CharacterState.Postgres/Schema.Recipes.sql','CharacterState.Postgres/Schema.Appearance.sql','CharacterState.Postgres/Schema.GatheringSkills.sql','CharacterState.Postgres/Schema.Death.sql','Housing.Postgres/Schema.sql','Exchange.Postgres/Schema.sql','Housing.Postgres/Schema.Gameplay.sql','Housing.Postgres/Schema.Geometry.sql','Housing.Postgres/Schema.Construction.sql') | ForEach-Object { Get-Content (Join-Path $repo "BackendServices/$_") -Raw }
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
    if ($Mode -eq 'VitalRestore') { $gameplayEnv.GAMEPLAY_FIXTURE_MODE='reconnect'; $gameplayEnv.GAMEPLAY_FIXTURE_SKILL_SEED='14' }
    Invoke-WithEnv $gameplayEnv { $script:gameplay = Start-Process -FilePath $fx.dotnet -ArgumentList @(('"'+(Join-Path $gameplayOut 'Gameplay.Host.dll')+'"')) -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $root 'gameplay.stdout.txt') -RedirectStandardError (Join-Path $root 'gameplay.stderr.txt') }
    $null = $gameplay.Handle; $owned.Add($gameplay)
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while (-not ((Test-Path (Join-Path $root 'gameplay.stdout.txt')) -and (Get-Content (Join-Path $root 'gameplay.stdout.txt') -Raw) -match 'GAMEPLAY_HOST_READY')) {
        if ($gameplay.HasExited -or $watch.Elapsed.TotalSeconds -gt 60) { throw 'Gameplay host did not become ready.' }
        Start-Sleep -Milliseconds 200
    }
    if ($Mode -eq 'VitalRestore') {
        [void](Invoke-FxOk $fx $fx.dotnet @((Join-Path $gameplayOut 'Gameplay.Host.dll'),'fixture-reconnect-appearance') $gameplayEnv)
    }
    $ledgerIni = '-ini:Game:[/Script/ShatteredFrontier.SFInventoryLedgerSettings]'; $servicesIni = '-ini:Game:[/Script/ShatteredFrontier.SFGameplayServicesSettings]'
    $serverFlags = @('-server',('-port='+$Port),'-multihome=127.0.0.1','-QuestFullScenario','-SFDevIdentity',
        ($ledgerIni+':bEnabled=True'),($ledgerIni+':Endpoint=127.0.0.1:'+$fx.hostPort),($servicesIni+':bEnabled=True'),($servicesIni+':Endpoint=127.0.0.1:'+$gameplayPort),
        ($servicesIni+':bRestoreOnConnect=True'),($servicesIni+':PositionSaveIntervalSeconds=5'))
    $serverEnv = @{ SF_INVENTORY_LEDGER_TOKEN=$fx.token; SF_GAMEPLAY_TOKEN=$gameplayToken }
    $alphaUrl = "127.0.0.1:$Port"+'?SFDevCharacter=alpha'; $bravoUrl = "127.0.0.1:$Port"+'?SFDevCharacter=bravo'
    $alphaFlags = @('-game','-QuestFullScenario','-QuestFullEconomy')

    if ($Mode -eq 'VitalRestore') {
        $combatIni = '-ini:Game:[/Script/ShatteredFrontier.SFCombatPilotSettings]:PlayerRespawnDelaySeconds=45'
        Invoke-WithEnv $serverEnv { $script:server = Start-Owned 'server' $silverbrook ($serverFlags + @('-ReconnectDeathScenario',$combatIni)) }
        Wait-Log $server 'server' 'QUEST_PILOT_READY Quests='
        $alpha1 = Start-Owned 'alpha1' $alphaUrl ($alphaFlags + "-ReconnectUrl=$alphaUrl")
        Wait-Log $server 'server' 'CHARACTER_FACE_APPLIED Character=chr_dev_alpha Face=Aelfwyn'
        Wait-Log $server 'server' 'RECONNECT_DEATH_TRIGGERED Character=chr_dev_alpha Logging=[1-9]'
        Wait-Log $server 'server' 'COMBAT_DEATH_RECORDED Character=chr_dev_alpha'
        Wait-Log $alpha1 'alpha1' 'RECONNECT_CLIENT_TRAVEL Face=Aelfwyn'
        Wait-Log $server 'server' 'CHARACTER_RESTORED Character=chr_dev_alpha Logging=[1-9]'
        Wait-Log $server 'server' 'CHARACTER_FACE_APPLIED Character=chr_dev_alpha Face=Aelfwyn' 2
        Wait-Log $server 'server' 'COMBAT_DEATH_RESTORED Event='
        $serverText = Read-Log 'server'
        $before = Last-Match $serverText 'RECONNECT_DEATH_TRIGGERED Character=chr_dev_alpha Logging=(\d+) Face=(\S+)'
        $recorded = Last-Match $serverText 'COMBAT_DEATH_RECORDED Character=chr_dev_alpha Event=(\S+) RemainingMs=(\d+)'
        $death = Last-Match $serverText 'COMBAT_DEATH_RESTORED Event=(\S+) RemainingMs=(\d+)'
        if (-not $before -or -not $recorded -or -not $death -or $death.Groups[1].Value -ne $recorded.Groups[1].Value -or
            [int64]$death.Groups[2].Value -le 0 -or [int64]$death.Groups[2].Value -gt [int64]$recorded.Groups[2].Value) {
            throw 'Reconnect did not restore the recorded death with remaining respawn time.'
        }
        $dead = (Invoke-LedgerSql $fx "SELECT is_dead FROM character_state.deaths WHERE character_id='chr_dev_alpha'" $fx.db).Trim()
        if ($dead -ne 't') { throw "Restored death was not durable: $dead" }
        $skill = (Invoke-LedgerSql $fx "SELECT COALESCE((state->'Skills'->>'skill/logging')::int,0) FROM character_state.progression WHERE character_id='chr_dev_alpha'" $fx.db).Trim()
        if ([int]$skill -lt 1 -or [int]$skill -ne [int]$before.Groups[1].Value -or $before.Groups[2].Value -ne 'Aelfwyn' -or
            $serverText -notmatch "CHARACTER_RESTORED Character=chr_dev_alpha Logging=$skill ") { throw "Persisted logging skill $skill was not restored." }
        Wait-Log $server 'server' 'COMBAT_PILOT_PLAYER_RESPAWN X='
        $alive = (Invoke-LedgerSql $fx "SELECT is_dead FROM character_state.deaths WHERE character_id='chr_dev_alpha'" $fx.db).Trim()
        if ($alive -ne 'f') { throw "Respawn did not persist alive state: $alive" }
        if ((Read-Log 'server') -match 'DEV_IDENTITY_IGNORED|GATHERING_LEDGER_PROJECTION_MISMATCH|Fatal error:|Assertion failed:') { throw 'Server reported an identity, projection or engine failure.' }
        $result.loggingSkill=$skill; $result.faceId='Aelfwyn'; $result.restoredDeathEvent=$death.Groups[1].Value
        $result.remainingRespawnMs=[int64]$death.Groups[2].Value; $result.status='Passed'
        return
    }

    # Phase 1: a drop during the first harvest's commit, then a reconnect.
    Invoke-WithEnv $serverEnv { $script:server1 = Start-Owned 'server1' $silverbrook ($serverFlags + '-SFDropDuringCommit') }
    Wait-Log $server1 'server1' 'QUEST_PILOT_READY Quests='
    $alpha1 = Start-Owned 'alpha1' $alphaUrl $alphaFlags
    $bravo1 = Start-Owned 'bravo1' $bravoUrl @('-game')
    Wait-Log $server1 'server1' 'RECONNECT_DROP_DURING_COMMIT Op=gather/'
    Stop-Owned $alpha1
    $alpha2 = Start-Owned 'alpha2' $alphaUrl $alphaFlags
    Wait-Log $server1 'server1' 'CHARACTER_RESTORED Character=chr_dev_alpha' 2
    # Stop right after a receipt: the next action is still walking or refining (3 s), so nothing is mid-commit.
    # A server killed mid-commit can lose that action's progression receipt (INT-05 known gap: no durable outbox).
    Wait-Log $server1 'server1' 'QUEST_PILOT_EVENT Kind=craft Target=item/iron_ingot .* Result=0'
    $text1 = Read-Log 'server1'
    $result.drained = $text1 -match 'CHARACTER_DRAINED Character=chr_dev_alpha'
    $step = (Last-Match $text1 'QUEST_PILOT_EVENT Kind=\S+ Target=\S+ Op=\S+ Result=0 Step=(\d+)').Groups[1].Value
    Stop-Owned $alpha2; Stop-Owned $bravo1; Stop-Owned $server1
    $saved = Last-Match (Read-Log 'server1') 'CHARACTER_POSITION_SAVED Character=chr_dev_alpha Ok=1 .* Location=X=(\S+) Y=(\S+) Z=(\S+)'

    # Phase 2: a fresh UE server; both characters reconnect and are restored from the services.
    Invoke-WithEnv $serverEnv { $script:server2 = Start-Owned 'server2' $silverbrook $serverFlags }
    Wait-Log $server2 'server2' 'QUEST_PILOT_READY Quests='
    $alpha3 = Start-Owned 'alpha3' $alphaUrl $alphaFlags
    $bravo2 = Start-Owned 'bravo2' $bravoUrl @('-game')
    Wait-Log $server2 'server2' 'CHARACTER_RESTORED Character=chr_dev_alpha .* Position=saved'
    Wait-Log $server2 'server2' 'CHARACTER_RESTORED Character=chr_dev_bravo'
    Wait-Log $server2 'server2' "QUEST_PILOT_RESTORED Quest=quest/frontier_founder Step=$step "
    Wait-Log $server2 'server2' 'QUEST_PILOT_COMPLETE Quest=quest/frontier_founder'
    Wait-Log $alpha3 'alpha3' 'QUEST_FULL_DONE'
    Start-Sleep -Seconds 3
    $boarGrants = [regex]::Matches((Read-Log 'server2'), 'COMBAT_PILOT_REWARD Death=\S+ Source=service Outcome=0 Granted=1 Code=loot_drop Item=item/boar_hide Quantity=(\d+)')
    if ($boarGrants.Count -ne 1) { throw "Expected one committed boar reward, saw $($boarGrants.Count)." }
    $boarQuantity = [int64]$boarGrants[0].Groups[1].Value
    $beforeCombatReconnect = Get-LedgerView $fx 'ctn_dev_alpha'
    if ($boarQuantity -lt 2 -or [int64]$beforeCombatReconnect.Items['item/boar_hide'] -ne $boarQuantity) { throw "Boar hides missing before reconnect: $($beforeCombatReconnect.Line)" }
    Stop-Owned $alpha3; Stop-Owned $bravo2; Stop-Owned $server2
    Invoke-WithEnv $serverEnv { $script:server3 = Start-Owned 'server3' $silverbrook $serverFlags }
    Wait-Log $server3 'server3' 'QUEST_PILOT_READY Quests='
    $alpha4 = Start-Owned 'alpha4' $alphaUrl @('-game')
    Wait-Log $server3 'server3' 'CHARACTER_RESTORED Character=chr_dev_alpha'
    $afterCombatReconnect = Get-LedgerView $fx 'ctn_dev_alpha'
    if ([int64]$afterCombatReconnect.Items['item/boar_hide'] -ne $boarQuantity) { throw "Boar hides did not survive reconnect: $($afterCombatReconnect.Line)" }
    $result.boarHidesAfterReconnect = [int64]$afterCombatReconnect.Items['item/boar_hide']

    $text = (Read-Log 'server1') + (Read-Log 'server2')
    if ($text -match 'DEV_IDENTITY_IGNORED|GATHERING_LEDGER_PROJECTION_MISMATCH|HOUSING_PROJECTION_MISMATCH|Fatal error:|Assertion failed:') { throw 'Server reported an identity, projection or engine failure.' }
    if ([regex]::Matches($text,'QUEST_PILOT_REWARD Bundle=').Count -ne 1) { throw 'Chain reward was not granted exactly once.' }
    foreach ($client in 'alpha1','alpha2','alpha3') { if ((Read-Log $client) -match 'QUEST_FULL_FAILED') { throw "$client driver reported a failure." } }
    $restored = [regex]::Match((Read-Log 'server2'),'CHARACTER_RESTORED Character=chr_dev_alpha .* Location=X=(\S+) Y=(\S+) Z=(\S+)')
    if (-not $saved -or -not $restored.Success) { throw 'Saved or restored position missing.' }
    $dx = [double]$saved.Groups[1].Value - [double]$restored.Groups[1].Value; $dy = [double]$saved.Groups[2].Value - [double]$restored.Groups[2].Value
    if ([Math]::Sqrt($dx*$dx + $dy*$dy) -gt 100) { throw "Restored position is $([Math]::Sqrt($dx*$dx+$dy*$dy)) cm from the last save." }

    # Exactly once in the durable stores: the Full chain's ledger result and one of each receipt.
    $view = Get-LedgerView $fx 'ctn_dev_alpha'
    $expected = [ordered]@{ 'item/oak_log'=0; 'item/rough_timber'=2; 'item/iron_ore'=0; 'item/iron_ingot'=0; 'item/iron_nails'=4; 'item/wall_material_bundle'=0; 'item/boar_hide'=$boarQuantity }
    foreach ($line in [regex]::Matches($text,'QUEST_PILOT_PAYOUT Item=(\S+) Quantity=(\d+)')) { $expected[$line.Groups[1].Value] = [int64]$expected[$line.Groups[1].Value] + [int64]$line.Groups[2].Value }
    foreach ($item in $expected.Keys) {
        [int64]$actual = 0
        foreach ($entry in $view.Items.GetEnumerator()) {
            if ($entry.Key -eq $item -or $entry.Key.StartsWith($item + '/')) { $actual += [int64]$entry.Value }
        }
        if ($actual -ne $expected[$item]) { throw "Ledger $item family=$actual, expected $($expected[$item])." }
    }
    foreach ($entry in $view.Items.GetEnumerator()) {
        if ([int64]$entry.Value -le 0) { continue }
        $families = @($expected.Keys | Where-Object { $entry.Key -eq $_ -or $entry.Key.StartsWith($_ + '/') })
        if ($families.Count -ne 1) { throw "Unexpected ledger item $($entry.Key)=$($entry.Value)." }
    }
    $counts = (Invoke-LedgerSql $fx ("SELECT (SELECT count(*) FROM character_state.source_receipts WHERE domain='progression' AND character_id='chr_dev_alpha')||','||" +
        "(SELECT count(*) FROM character_state.reward_receipts WHERE character_id='chr_dev_alpha' AND state='Complete')||','||" +
        "(SELECT count(*) FROM character_state.reward_receipts WHERE character_id='chr_dev_alpha')||','||" +
        "(SELECT count(*) FROM housing.placements WHERE state='Committed')||','||" +
        "(SELECT count(*) FROM character_state.quests WHERE character_id='chr_dev_alpha')||','||" +
        "(SELECT count(*) FROM character_state.positions WHERE character_id='chr_dev_alpha')") $fx.db).Trim()
    # Progression receipts: oak x2, iron, sawmill x2, forge ingot, forge nails, workbench, boar defeat; one saved position.
    if ($counts -ne '9,1,1,1,1,1') { throw "Durable counts (progression,payout complete,payouts,placements,quests,positions) = $counts, expected 9,1,1,1,1,1." }
    $result.ledger = $view.Line; $result.durableCounts = $counts; $result.resumedStep = $step
    $result.status = 'Passed'
} catch { $result.status = 'Failed'; $result.error = $_.Exception.Message; throw }
finally {
    foreach ($process in $owned) { if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() } }
    Remove-LedgerFixture $fx; $result.cleanup = if ($fx.cleanupFailure) { $fx.cleanupFailure } else { 'removed' }
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($result.status)"
}
