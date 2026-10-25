[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17841,
    [ValidateRange(320,1200)][int]$TimeoutSeconds = 600,
    [ValidateRange(1,10)][int]$Clients = 1,
    [ValidatePattern('^/Game/Frontier/[A-Za-z0-9_/]+$')][string]$Map = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook',
    [switch]$GreenlandsDeathNetcheck,
    [switch]$GreenlandsUnstuckNetcheck,
    [switch]$AttributeSnapshotNetcheck,
    [ValidateSet('Release','Resurrection')][string]$DeathResolution = 'Release',
    [switch]$RestartMidDeath,
    [switch]$TargetScenario,
    [switch]$PartyScenario,
    [switch]$PartyXpScenario,
    [switch]$Services,
    [string]$ServerExe,
    [switch]$SpawnOnly
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repo 'GameClient\ShatteredFrontier.uproject'
$bridge = Join-Path $repo 'BackendServices\WorldContent.Tool\bin\Debug\net10.0\WorldContent.Tool.dll'
$sdk = Join-Path $repo 'Artifacts\Persistence\Toolchains\dotnet-10.0.402\dotnet.exe'
if ($Services -and $TargetScenario) { throw 'Services ledger assertion requires the combat reward scenario.' }
if ($AttributeSnapshotNetcheck -and (-not $Services -or $Clients -ne 2 -or
    $Map -ne '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook' -or
    $GreenlandsDeathNetcheck -or $GreenlandsUnstuckNetcheck -or $PartyXpScenario -or $PartyScenario -or $SpawnOnly)) {
    throw 'AttributeSnapshotNetcheck requires Silverbrook, two clients and Gameplay Services.'
}
if ($GreenlandsDeathNetcheck -and (-not $Services -or $Clients -ne 2 -or $Map -ne '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook' -or
    $TargetScenario -or $PartyScenario -or $PartyXpScenario -or $SpawnOnly)) { throw 'GreenlandsDeathNetcheck requires Silverbrook, two clients and Gameplay Services.' }
if ($GreenlandsUnstuckNetcheck -and (-not $Services -or $Clients -ne 2 -or $Map -ne '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook' -or
    $GreenlandsDeathNetcheck -or $TargetScenario -or $PartyScenario -or $PartyXpScenario -or $SpawnOnly)) {
    throw 'GreenlandsUnstuckNetcheck requires Silverbrook, two clients and Gameplay Services.'
}
if ($RestartMidDeath -and -not $GreenlandsDeathNetcheck) { throw 'RestartMidDeath requires GreenlandsDeathNetcheck.' }
if ($SpawnOnly -and (-not $Services -or -not $ServerExe)) { throw '-SpawnOnly requires -Services and -ServerExe.' }
if ($PartyScenario -and ($Clients -ne 3 -or $Services -or $TargetScenario -or $SpawnOnly)) { throw '-PartyScenario requires -Clients 3 without services or other scenarios.' }
if ($PartyXpScenario -and (-not $Services -or $PartyScenario -or $TargetScenario -or $SpawnOnly -or
    -not (($Clients -eq 3 -and $Map -eq '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook') -or
          ($Clients -eq 2 -and $Map -eq '/Game/Frontier/Maps/Sandbox/SB_PersonalLoot')))) { throw '-PartyXpScenario requires three Silverbrook clients or two SB_PersonalLoot clients with -Services.' }
if ($Clients -gt 1 -and -not $PartyScenario -and (-not $Services -or $TargetScenario)) { throw 'Group combat requires -Services and does not combine with -TargetScenario.' }
$requiredInputs = @($editor,$project,$ServerExe) | Where-Object { $_ }
if (-not $Services) { $requiredInputs += @($bridge,$sdk) }
foreach ($required in $requiredInputs) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required pilot input missing: $required" }
}
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) {
    throw 'Test port is already in use.'
}
$root = Join-Path $repo ('Artifacts\CombatPilot\network-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $root
if ($Services) { . (Join-Path $PSScriptRoot 'InventoryLedgerHostFixture.ps1') }
$fx = @{}
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$scope = if ($AttributeSnapshotNetcheck) { 'Greenlands two-client authoritative attribute restore and combat' }
    elseif ($GreenlandsUnstuckNetcheck) { 'Greenlands two-client Unstuck, token replay, and restart cooldown' }
    elseif ($GreenlandsDeathNetcheck) { "Greenlands two-client durable boar death and $DeathResolution" }
    elseif ($PartyXpScenario) { 'Three-client party: server restart, two in-range personal loot and XP recipients, out-of-range exclusion' }
    elseif ($PartyScenario) { 'Three clients: invite, decline, accept, leader handoff, kick, leave and party HUD' }
    elseif ($SpawnOnly) { 'Packaged dedicated server, Gameplay.Host, five boars in each authored group' }
    elseif ($Clients -gt 1) { "$Clients clients, one boar, one mid-fight reconnect, owner targeting and durable killer reward" }
    elseif ($TargetScenario) { 'One dedicated server, one client; Tab selection, owner replication, death and range drops' }
    elseif ($Services) { 'One dedicated server, one client, durable boar hides in InventoryLedger and WorldContent slot respawn' }
    else { 'One dedicated server, one client; local attack RPC and server-authored WorldContent slots' }
$result = [ordered]@{ status='Running'; map=$Map; scope=$scope; runs=@() }
$wall = [Diagnostics.Stopwatch]::StartNew()
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra) {
    $log = Join-Path $root ($Name+'.log')
    $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $executable = if ($Name -eq 'server' -and $ServerExe) { $ServerExe } else { $editor }
    $projectArg = if ($executable -eq $editor) { @(('"'+$project+'"')) } else { @() }
    $arguments = @($projectArg)+@($Url,'-unattended','-NullRHI','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    if ($Clients -gt 1 -and -not $PartyScenario -and -not $PartyXpScenario -and $Name -like 'client*') {
        $clientCsvDir = Join-Path $root ($Name+'-csv')
        $null = New-Item -ItemType Directory -Path $clientCsvDir
        $arguments += ('-M3CsvDir="'+$clientCsvDir+'"')
    }
    $process = Start-Process -FilePath $executable -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt'))
    $null = $process.Handle
    $owned.Add($process)
    $result.runs += [ordered]@{ name=$Name; pid=$process.Id; log=$log; arguments=$arguments }
    return $process
}
function Wait-Log([Diagnostics.Process]$Process,[string]$Name,[string]$Pattern,[int]$LimitSeconds = 0) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $log = Join-Path $root ($Name+'.log')
    $limit = if ($LimitSeconds -gt 0) { $LimitSeconds } else { $TimeoutSeconds }
    while ($watch.Elapsed.TotalSeconds -lt $limit) {
        if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern $Pattern -Quiet)) { return }
        if ($Process.HasExited) { throw "$Name exited before $Pattern." }
        Start-Sleep -Milliseconds 300
    }
    throw "Timed out waiting for $Name ($Pattern)."
}
function Wait-M3Csv([Diagnostics.Process]$Process,[string]$Path) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if (Test-Path -LiteralPath $Path -PathType Leaf) {
            try { $last = Get-Content -LiteralPath $Path -Tail 1 -ErrorAction Stop }
            catch { $last = '' } # Unreal may hold an exclusive lock while the CSV writer finalizes.
            if ($last -match '^\[HasHeaderRowAtEnd\],1,') { return }
        }
        if ($Process.HasExited) { throw "Process exited before CSV finalized: $Path" }
        Start-Sleep -Milliseconds 300
    }
    throw "Timed out waiting for CSV: $Path"
}
function Get-M3FrameStats([string]$Path) {
    $parser = [Microsoft.VisualBasic.FileIO.TextFieldParser]::new($Path)
    $parser.SetDelimiters(',')
    $parser.HasFieldsEnclosedInQuotes = $true
    $rows = [Collections.Generic.List[object]]::new()
    try { while (-not $parser.EndOfData) { $rows.Add($parser.ReadFields()) } }
    finally { $parser.Close() }
    if ($rows.Count -lt 12 -or $rows[$rows.Count - 1][0] -ne '[HasHeaderRowAtEnd]') { throw "Incomplete frame CSV: $Path" }
    $frameIndex = [Array]::IndexOf([string[]]$rows[$rows.Count - 2], 'FrameTime')
    if ($frameIndex -lt 0) { throw "FrameTime missing from CSV: $Path" }
    $values = [Collections.Generic.List[double]]::new()
    for ($i = 1; $i -lt $rows.Count - 2; $i++) {
        $fields = [string[]]$rows[$i]
        if ($fields.Length -le $frameIndex) { continue }
        $ms = 0.0
        if ([double]::TryParse($fields[$frameIndex], [Globalization.NumberStyles]::Float,
            [Globalization.CultureInfo]::InvariantCulture, [ref]$ms) -and [double]::IsFinite($ms) -and $ms -gt 0) {
            $values.Add($ms)
        }
    }
    if ($values.Count -lt 10) { throw "Too few positive frame samples in $Path" }
    $sorted = $values.ToArray(); [Array]::Sort($sorted)
    $sum = 0.0; foreach ($value in $values) { $sum += $value }
    return [ordered]@{ samples=$values.Count; avg=[math]::Round($sum/$values.Count,3)
        p95=[math]::Round($sorted[[math]::Ceiling(0.95*$sorted.Length)-1],3); max=[math]::Round($sorted[-1],3) }
}
try {
    if ($Services) {
        New-LedgerFixture $fx $repo $root
        $schemaFiles = @('CharacterState.Postgres/Schema.sql','CharacterState.Postgres/Schema.Position.sql','CharacterState.Postgres/Schema.Recipes.sql','CharacterState.Postgres/Schema.Appearance.sql','CharacterState.Postgres/Schema.GatheringSkills.sql','CharacterState.Postgres/Schema.Death.sql','CharacterState.Postgres/Schema.Rested.sql','Housing.Postgres/Schema.sql','Exchange.Postgres/Schema.sql','Housing.Postgres/Schema.Gameplay.sql','Housing.Postgres/Schema.Geometry.sql','Housing.Postgres/Schema.Construction.sql')
        if ($PartyXpScenario) { $schemaFiles += 'Group.Postgres/Schema.sql' }
        $schemas = $schemaFiles | ForEach-Object { Get-Content (Join-Path $repo "BackendServices/$_") -Raw }
        [void](Invoke-LedgerSql $fx ("SET ROLE ledger_owner;`n" + ($schemas -join "`n")) $fx.db)
        $migrationSchemas = @('character_state','housing','exchange')
        if ($PartyXpScenario) { $migrationSchemas += 'group' }
        Invoke-LedgerSchemaMigrations $fx $migrationSchemas
        $grants = 'GRANT USAGE ON SCHEMA character_state,housing,exchange TO ledger_svc; ' + ((@('character_state','housing','exchange') | ForEach-Object { "GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA $_ TO ledger_svc;" }) -join ' ')
        if ($PartyXpScenario) { $grants += ' GRANT USAGE ON SCHEMA "group" TO ledger_svc; GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA "group" TO ledger_svc;' }
        [void](Invoke-LedgerSql $fx $grants $fx.db)
        $gameplayProject = Join-Path $repo 'BackendServices/Gameplay.Host/Gameplay.Host.csproj'
        $gameplayOut = Join-Path $fx.base 'gameplay-host'
        [void](Invoke-FxOk $fx $fx.dotnet @('restore',$gameplayProject,'--locked-mode','--packages',$fx.nuget,'--verbosity','quiet'))
        [void](Invoke-FxOk $fx $fx.dotnet @('build',$gameplayProject,'--no-restore','--configuration','Release','--verbosity','quiet','--output',$gameplayOut))
        $fx.hostPort = Get-FreeTcpPort; $gameplayPort = Get-FreeTcpPort
        while ($gameplayPort -eq $fx.hostPort) { $gameplayPort = Get-FreeTcpPort }
        $gameplayToken = New-HexSecret; $fx.secrets += $gameplayToken
        Start-LedgerHost $fx ''
        $gameplayEnv = @{ GAMEPLAY_HOST_PORT="$gameplayPort"; SF_GAMEPLAY_TOKEN=$gameplayToken; GAMEPLAY_LEDGER_CONNECTION=$fx.connection; GAMEPLAY_CHARACTER_CONNECTION=$fx.connection
            GAMEPLAY_HOUSING_CONNECTION=$fx.connection; GAMEPLAY_EXCHANGE_CONNECTION=$fx.connection; GAMEPLAY_CONTENT_ROOT=(Join-Path $repo 'content') }
        if ($PartyXpScenario) { $gameplayEnv.GAMEPLAY_GROUP_CONNECTION = $fx.connection }
        Invoke-WithEnv $gameplayEnv { $script:gameplay = Start-Process -FilePath $fx.dotnet -ArgumentList @(('"'+(Join-Path $gameplayOut 'Gameplay.Host.dll')+'"')) -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $root 'gameplay.stdout.txt') -RedirectStandardError (Join-Path $root 'gameplay.stderr.txt') }
        $null = $gameplay.Handle; $owned.Add($gameplay)
        $watch = [Diagnostics.Stopwatch]::StartNew()
        while (-not ((Test-Path (Join-Path $root 'gameplay.stdout.txt')) -and (Get-Content (Join-Path $root 'gameplay.stdout.txt') -Raw) -match 'GAMEPLAY_HOST_READY')) {
            if ($gameplay.HasExited -or $watch.Elapsed.TotalSeconds -gt 60) { throw 'Gameplay host did not become ready.' }
            Start-Sleep -Milliseconds 200
        }
    }
    $url = $result.map
    $scenarioArg = if ($GreenlandsUnstuckNetcheck) { '-GreenlandsUnstuckNetcheck' } elseif ($GreenlandsDeathNetcheck) { '-GreenlandsDeathNetcheck' } elseif ($PartyScenario) { '-PartyScenario' } elseif ($Clients -gt 1) { '-CombatGroupScenario' } elseif ($TargetScenario) { '-CombatTargetScenario' } else { '-CombatPilotScenario' }
    $serverFlags = @('-server',('-port='+$Port),'-multihome=127.0.0.1',$scenarioArg)
    if ($GreenlandsDeathNetcheck) { $serverFlags += "-DeathResolution=$DeathResolution" }
    if ($PartyScenario) { $serverFlags += '-SFDevIdentity' }
    if ($PartyXpScenario) { $serverFlags += '-PartyXpScenario' }
    if ($PartyXpScenario -and $Clients -eq 2) { $serverFlags += '-PersonalLootScenario' }
    if ($Clients -gt 3) { $serverFlags += '-CombatGroupCapacitySlots' }
    if ($Clients -gt 1 -and -not $PartyScenario -and -not $PartyXpScenario) {
        $csvDir = Join-Path $root 'server-csv'
        $null = New-Item -ItemType Directory -Path $csvDir
        $serverFlags += ('-M3CsvDir="'+$csvDir+'"')
    }
    if ($Services) {
        $ledgerIni = '-ini:Game:[/Script/ShatteredFrontier.SFInventoryLedgerSettings]'; $servicesIni = '-ini:Game:[/Script/ShatteredFrontier.SFGameplayServicesSettings]'
        $serverFlags += @('-SFDevIdentity',($ledgerIni+':bEnabled=True'),($ledgerIni+':Endpoint=127.0.0.1:'+$fx.hostPort),
            ($servicesIni+':bEnabled=True'),($servicesIni+':Endpoint=127.0.0.1:'+$gameplayPort))
        $serverEnv = @{ SF_INVENTORY_LEDGER_TOKEN=$fx.token; SF_GAMEPLAY_TOKEN=$gameplayToken }
        Invoke-WithEnv $serverEnv { $script:server = Start-Owned 'server' $url $serverFlags }
    } else { $server = Start-Owned 'server' $url $serverFlags }
    if ($PartyXpScenario -and $Clients -eq 2) {
        Wait-Log $server 'server' 'COMBAT_SANDBOX_READY Map=/Game/Frontier/Maps/Sandbox/SB_PersonalLoot Boars=2'
        foreach ($role in 1..2) {
            $identity = @('alpha','beta')[$role - 1]
            $null = Start-Owned "client$role" ("127.0.0.1:$Port`?SFDevCharacter=$identity") @('-game','-CombatGroupScenario','-PartyXpScenario','-PersonalLootScenario',"-PartyXpScenarioRole=$role","-CombatGroupRole=$role")
        }
        foreach ($role in 1..2) {
            $process = @($owned | Where-Object { $_.Id -eq (@($result.runs | Where-Object name -eq "client$role")[0].pid) })[0]
            Wait-Log $process "client$role" "PARTY_XP_READY Role=$role Members=2"
        }
        Wait-Log $server 'server' 'COMPANION_SANDBOX_BOAR_DEAD Slot=[01]' 90
        Wait-Log $server 'server' 'SHARED_LOOT_CREATED Event=.* Creature=npc/bandit_captain Eligible=2 Items=1' 90
        Wait-Log $server 'server' 'SHARED_LOOT_CLAIM Event=.* Character=chr_dev_alpha Won=1 Code=0 Items=1' 60
        Wait-Log $server 'server' 'SHARED_LOOT_CLAIM Event=.* Character=chr_dev_beta Won=0 Code=6' 60
        $winnerClient = @($owned | Where-Object { $_.Id -eq (@($result.runs | Where-Object name -eq 'client1')[0].pid) })[0]
        Wait-Log $winnerClient 'client1' 'PERSONAL_LOOT_CLIENT_INVENTORY Item=item/(studded_jerkin|reinforced_greaves|captains_longsword) Name=.+ Count=1' 60
        $rows = ''
        for ($attempt = 0; $attempt -lt 50; $attempt++) {
            $rows = Invoke-LedgerSql $fx "SELECT string_agg(recipient_id,',' ORDER BY recipient_id) FROM inventory.defeat_reward_intents WHERE state='Complete'" $fx.db
            if ($rows -eq 'chr_dev_alpha,chr_dev_beta') { break }
            Start-Sleep -Milliseconds 200
        }
        if ($rows -ne 'chr_dev_alpha,chr_dev_beta') { throw "Personal loot recipients incorrect: $rows" }
        $shared = Invoke-LedgerSql $fx "SELECT creature_id||'|'||table_id||'|'||array_to_string(ARRAY(SELECT unnest(eligible_ids) ORDER BY 1),',')||'|'||claimant_id||'|'||state FROM inventory.shared_defeat_drops" $fx.db
        if ($shared -ne 'npc/bandit_captain|loot/boss_greenlands_bandit_captain|chr_dev_alpha,chr_dev_beta|chr_dev_alpha|Complete') { throw "Shared drop snapshot/claim incorrect: $shared" }
        foreach ($identity in @('alpha','beta')) {
            $view = Get-LedgerView $fx "ctn_dev_$identity"
            $amount = [int]$view.items['item/boar_hide']
            if ($amount -lt 2 -or $amount -gt 3) { throw "Personal loot missing for $identity`: $amount; ledger get exit=$($view.exitCode) line=$($view.line)" }
            $bossItems = @('item/studded_jerkin','item/reinforced_greaves','item/captains_longsword') | ForEach-Object { [int]$view.items[$_] } | Measure-Object -Sum
            if ($bossItems.Sum -ne [int]($identity -eq 'alpha')) { throw "Boss set claim incorrect for $identity`: $($bossItems.Sum)" }
        }
        $result.status = 'Passed'
        $result.lootRecipients = $rows
        return
    }
    Wait-Log $server 'server' 'COMBAT_PILOT_READY Boars=10'
    if ($PartyXpScenario) {
        $names = @('alpha','beta','gamma')
        for ($role = 1; $role -le 3; $role++) {
            $identity = $names[$role - 1]
            $null = Start-Owned "client$role" ("127.0.0.1:$Port`?SFDevCharacter=$identity") @('-game','-CombatGroupScenario','-PartyXpScenario','-PartyXpHoldAttack',"-PartyXpScenarioRole=$role",("-CombatGroupRole=" + $(if ($role -eq 3) { 0 } else { $role })))
        }
        for ($role = 1; $role -le 3; $role++) {
            $process = @($owned | Where-Object { $_.Id -eq (@($result.runs | Where-Object name -eq "client$role")[0].pid) })[0]
            Wait-Log $process "client$role" "PARTY_XP_READY Role=$role Members=3"
        }
        $before = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
        if ($before -notmatch 'PARTY_ACTION Actor=gamma Action=2 Target=alpha Accepted=1 Service=0') { throw 'Party did not persist before restart.' }
        $leaderBefore = Invoke-LedgerSql $fx 'SELECT leader_character_id FROM "group".parties WHERE state=''active''' $fx.db
        if ($leaderBefore -ne 'chr_dev_alpha') { throw "Unexpected durable leader before restart: $leaderBefore" }
        Stop-Process -Id $server.Id -Force; $server.WaitForExit()
        foreach ($name in @('client1','client2','client3')) {
            $p = @($owned | Where-Object { $_.Id -eq (@($result.runs | Where-Object name -eq $name)[0].pid) })[0]
            if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; $p.WaitForExit() }
        }
        Invoke-WithEnv $serverEnv { $script:server = Start-Owned 'server2' $url $serverFlags }
        Wait-Log $server 'server2' 'COMBAT_PILOT_READY Boars=10'
        for ($role = 1; $role -le 3; $role++) {
            $identity = $names[$role - 1]
            $null = Start-Owned "client$role-rejoin" ("127.0.0.1:$Port`?SFDevCharacter=$identity") @('-game','-CombatGroupScenario','-PartyXpScenario','-PartyXpRestoreOnly',"-PartyXpScenarioRole=$role",("-CombatGroupRole=" + $(if ($role -eq 3) { 0 } else { $role })))
        }
        for ($role = 1; $role -le 3; $role++) {
            $name = "client$role-rejoin"
            $p = @($owned | Where-Object { $_.Id -eq (@($result.runs | Where-Object name -eq $name)[0].pid) })[0]
            Wait-Log $p $name "PARTY_XP_READY Role=$role Members=3"
        }
        Wait-Log $server 'server2' 'COMBAT_PILOT_BOAR_DEAD Group=boar_farm_edge Slot=0' 90
        Wait-Log $server 'server2' 'COMBAT_PILOT_REWARD Death=boar_farm_edge:0:\d+ Source=service Outcome=0 Granted=1' 90
        Wait-Log $server 'server2' 'PARTY_XP_RECEIPT Character=chr_dev_alpha Xp=[1-9]' 90
        Wait-Log $server 'server2' 'PARTY_XP_RECEIPT Character=chr_dev_beta Xp=[1-9]' 90
        $text = Get-Content -LiteralPath (Join-Path $root 'server2.log') -Raw
        $far = [regex]::Match($text,'PARTY_XP_RANGE Character=chr_dev_gamma DistanceCm=(\d+)')
        if (-not $far.Success -or [int]$far.Groups[1].Value -le 6000) { throw 'Gamma was not outside the 60 m XP radius.' }
        if ([regex]::Matches($text,'PARTY_RESTORED Character=chr_dev_\w+ Party=pty_').Count -lt 3 -or
            [regex]::Matches($text,'PARTY_XP_RECEIPT Character=chr_dev_(alpha|beta) Xp=[1-9]\d* PartySize=2 Applied=1').Count -ne 2 -or
            $text -match 'PARTY_XP_RECEIPT Character=chr_dev_gamma|Fatal error:|Assertion failed:') { throw 'Party restore or two-in-range XP evidence failed.' }
        $xpRows = Invoke-LedgerSql $fx "SELECT string_agg(DISTINCT character_id,',' ORDER BY character_id) FROM character_state.source_receipts WHERE domain='progression' AND operation_id LIKE 'boar:%'" $fx.db
        if ($xpRows -notmatch 'chr_dev_alpha,chr_dev_beta' -or $xpRows -match 'gamma') { throw "Durable XP recipients incorrect: $xpRows" }
        $leaderAfter = Invoke-LedgerSql $fx 'SELECT leader_character_id FROM "group".parties WHERE state=''active''' $fx.db
        if ($leaderAfter -ne 'chr_dev_alpha') { throw "Durable leader changed across restart: $leaderAfter" }
        $hides = 0
        foreach ($identity in @('alpha','beta')) {
            $view = Get-LedgerView $fx "ctn_dev_$identity"
            $amount = [int]$view.items['item/boar_hide']
            if ($amount -lt 2 -or $amount -gt 3) { throw "Personal hides missing for $identity`: $amount" }
            $hides += $amount
        }
        $farBag = Get-LedgerView $fx 'ctn_dev_gamma'
        if ([int]$farBag.items['item/boar_hide'] -ne 0) { throw 'Out-of-range gamma received personal loot.' }
        $grantRows = Invoke-LedgerSql $fx "SELECT string_agg(recipient_id,',' ORDER BY recipient_id) FROM inventory.defeat_reward_intents WHERE state='Complete'" $fx.db
        if ($grantRows -ne 'chr_dev_alpha,chr_dev_beta') { throw "Durable loot recipients incorrect: $grantRows" }
        $result.status = 'Passed'
        $result.xpRecipients = $xpRows
        $result.hidesTotal = $hides
        return
    }
    if ($PartyScenario) {
        $names = @('alpha','beta','gamma')
        for ($role = 1; $role -le 3; $role++) {
            $identity = $names[$role - 1]
            $null = Start-Owned "client$role" ("127.0.0.1:$Port`?SFDevCharacter=$identity") @('-game',"-PartyScenarioRole=$role")
        }
        for ($role = 1; $role -le 3; $role++) {
            $run = @($result.runs | Where-Object name -eq "client$role")[0]
            $clientProcess = @($owned | Where-Object Id -eq $run.pid)[0]
            Wait-Log $clientProcess "client$role" "PARTY_SCENARIO_DONE Role=$role"
        }
        $serverText = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
        foreach ($action in @(1,2,3,4,5,6)) {
            if ($serverText -notmatch "PARTY_ACTION Actor=\w+ Action=$action Target=\w* Accepted=1") { throw "Party action $action missing." }
        }
        if ($serverText -match 'Fatal error:|Assertion failed:') { throw 'Party server failed.' }
        for ($role = 1; $role -le 3; $role++) {
            $clientText = Get-Content -LiteralPath (Join-Path $root "client$role.log") -Raw
            if ($clientText -notmatch 'PARTY_HUD Members=3 Visible=3') { throw "Client $role did not show all three party frames." }
        }
        $result.status='Passed'
        return
    }
    if ($SpawnOnly) {
        $serverText = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
        foreach ($group in @('boar_farm_edge','boar_forest_edge')) {
            if ([regex]::Matches($serverText,"COMBAT_PILOT_BOAR_SPAWN Group=$group Slot=").Count -ne 5) {
                throw "Packaged $group did not spawn all five boars."
            }
        }
        if ($serverText -match 'COMBAT_PILOT_SLOT_ERROR|Fatal error:|Assertion failed:') { throw 'Packaged combat logged a slot or fatal error.' }
        $result.status='Passed'
        return
    }
    if ($GreenlandsUnstuckNetcheck) {
        $names = @('alpha','beta')
        for ($role = 1; $role -le 2; $role++) {
            $label = if ($role -eq 1) { 'Driver' } else { 'Observer' }
            $null = Start-Owned "client$role" ("127.0.0.1:$Port`?SFDevCharacter=$($names[$role - 1])") @('-game','-UnstuckScenario','-GreenlandsUnstuckNetcheck',"-UnstuckRole=$label")
        }
        Wait-Log $server 'server' 'UNSTUCK_SERVER_ACCEPTED .*Source=NavMesh'
        Wait-Log $server 'server' 'UNSTUCK_TOKEN_CONSUMED Sequence='
        Wait-Log $server 'server' 'UNSTUCK_TOKEN_DENIED Sequence='
        Wait-Log $server 'server' 'UNSTUCK_SERVER_DENIED Reason=StateOrCooldown'
        foreach ($role in 1..2) {
            $label = if ($role -eq 1) { 'Driver' } else { 'Observer' }
            $process = @($owned | Where-Object Id -eq (@($result.runs | Where-Object name -eq "client$role")[0].pid))[0]
            Wait-Log $process "client$role" "UNSTUCK_CLIENT_MOVED Role=$label Distance="
        }
        $first = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
        if ([regex]::Matches($first,'UNSTUCK_SERVER_ACCEPTED').Count -ne 1 -or
            [regex]::Matches($first,'UNSTUCK_TOKEN_CONSUMED').Count -ne 1 -or
            [regex]::Matches($first,'UNSTUCK_TOKEN_DENIED').Count -ne 1 -or
            [regex]::Matches($first,'UNSTUCK_SERVER_ACCEPTED .*Source=NavMesh').Count -ne 1) {
            throw 'Initial navmesh landing or single-use token count invalid.'
        }
        $cooldown = Invoke-LedgerSql $fx "SELECT unstuck_at_unix_ms FROM character_state.positions WHERE character_id='chr_dev_alpha'" $fx.db
        if ([long]$cooldown -le 0) { throw 'Unstuck cooldown was not durably committed.' }
        foreach ($name in @('client1','client2')) {
            $p = @($owned | Where-Object Id -eq (@($result.runs | Where-Object name -eq $name)[0].pid))[0]
            if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; $p.WaitForExit() }
        }
        if (-not $server.HasExited) { Stop-Process -Id $server.Id -Force; $server.WaitForExit() }
        Invoke-WithEnv $serverEnv { $script:server = Start-Owned 'server2' $url ($serverFlags + '-UnstuckRestoreCheck') }
        Wait-Log $server 'server2' 'COMBAT_PILOT_READY Boars=10'
        for ($role = 1; $role -le 2; $role++) {
            $label = if ($role -eq 1) { 'Driver' } else { 'Observer' }
            $null = Start-Owned "client$role-rejoin" ("127.0.0.1:$Port`?SFDevCharacter=$($names[$role - 1])") @('-game','-UnstuckScenario','-GreenlandsUnstuckNetcheck','-UnstuckRestoreCheck',"-UnstuckRole=$label")
        }
        Wait-Log $server 'server2' 'CHARACTER_RESTORED Character=chr_dev_alpha'
        Wait-Log $server 'server2' 'UNSTUCK_SERVER_DENIED Reason=StateOrCooldown'
        $again = Get-Content -LiteralPath (Join-Path $root 'server2.log') -Raw
        if ($again -match 'UNSTUCK_SERVER_ACCEPTED' -or
            [regex]::Matches($again,'UNSTUCK_SERVER_DENIED Reason=StateOrCooldown').Count -ne 1) {
            throw 'Cooldown failed after the server restarted.'
        }
        $cooldownAfter = Invoke-LedgerSql $fx "SELECT unstuck_at_unix_ms FROM character_state.positions WHERE character_id='chr_dev_alpha'" $fx.db
        if ($cooldownAfter -ne $cooldown) { throw 'Denied restart request changed the durable cooldown.' }
        $result.status='Passed'
        return
    }
    if ($GreenlandsDeathNetcheck) {
        $expected = if ($DeathResolution -eq 'Release') { 40 } else { 28 }
        $names = @('alpha','beta')
        for ($role = 1; $role -le 2; $role++) {
            $identity = $names[$role - 1]
            $label = if ($role -eq 1) { 'Driver' } else { 'Observer' }
            $null = Start-Owned "client$role" ("127.0.0.1:$Port`?SFDevCharacter=$identity") @('-game','-GreenlandsDeathNetcheck',"-GreenlandsDeathRole=$label","-DeathResolution=$DeathResolution")
            if ($role -eq 1) { Wait-Log $server 'server' 'CHARACTER_RESTORED Character=chr_dev_alpha' }
        }
        Wait-Log $server 'server' 'COMBAT_DEATH_RECORDED Character=chr_dev_alpha'
        $deadRow = Invoke-LedgerSql $fx "SELECT is_dead,death_event_id FROM character_state.deaths WHERE character_id='chr_dev_alpha'" $fx.db
        if ($deadRow -notmatch 't\|[0-9a-f-]{36}') { throw "Durable boar death missing: $deadRow" }
        if ($RestartMidDeath) {
            foreach ($p in @($owned | Where-Object { $_.Id -in @($server.Id,(@($result.runs | Where-Object name -eq 'client1')[0].pid),(@($result.runs | Where-Object name -eq 'client2')[0].pid)) })) {
                if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; $p.WaitForExit() }
            }
            Invoke-WithEnv $serverEnv { $script:server = Start-Owned 'server2' $url $serverFlags }
            Wait-Log $server 'server2' 'COMBAT_PILOT_READY Boars=10'
            for ($role = 1; $role -le 2; $role++) {
                $identity = $names[$role - 1]
                $label = if ($role -eq 1) { 'Driver' } else { 'Observer' }
                $null = Start-Owned "client$role-rejoin" ("127.0.0.1:$Port`?SFDevCharacter=$identity") @('-game','-GreenlandsDeathNetcheck','-DeathRestoreOnly',"-GreenlandsDeathRole=$label","-DeathResolution=$DeathResolution")
            }
            Wait-Log $server 'server2' 'COMBAT_DEATH_RESTORED Event='
        }
        $serverName = if ($RestartMidDeath) { 'server2' } else { 'server' }
        $clientSuffix = if ($RestartMidDeath) { '-rejoin' } else { '' }
        Wait-Log $server $serverName "COMBAT_DEATH_RESOLVED Event=.* HP=$expected Mode=$DeathResolution"
        Wait-Log $server $serverName $(if ($DeathResolution -eq 'Release') { 'SPIRIT_RELEASE_SERVER_DENIED' } else { 'RES_ACCEPT_SERVER_DENIED' })
        $serverText = Get-Content -LiteralPath (Join-Path $root "$serverName.log") -Raw
        $serverPosition = [regex]::Match($serverText,"COMBAT_DEATH_RESOLVED Event=.* HP=$expected Mode=$DeathResolution X=(-?\d+) Y=(-?\d+)")
        if (-not $serverPosition.Success) { throw 'Authoritative resolution position missing' }
        for ($role = 1; $role -le 2; $role++) {
            $label = if ($role -eq 1) { 'Driver' } else { 'Observer' }
            $name = "client$role$clientSuffix"
            $process = @($owned | Where-Object { $_.Id -eq (@($result.runs | Where-Object name -eq $name)[0].pid) })[0]
            Wait-Log $process $name "GREENLANDS_DEATH_CLIENT_REPLAY_SAFE Role=$label HP=\d+"
            $lines = Get-Content -LiteralPath (Join-Path $root "$name.log") -Raw
            if ($lines -notmatch "GREENLANDS_DEATH_CLIENT_DEAD Role=$label HP=0" -or
                $lines -notmatch "GREENLANDS_DEATH_CLIENT_RESTORED Role=$label Mode=$DeathResolution HP=$expected") {
                throw "$label did not observe boar death and $DeathResolution health"
            }
            $clientPosition = [regex]::Match($lines,"GREENLANDS_DEATH_CLIENT_REPLAY_SAFE Role=$label HP=(\d+) X=(-?\d+) Y=(-?\d+)")
            if (-not $clientPosition.Success -or
                [int]$clientPosition.Groups[1].Value -gt $expected -or
                [math]::Abs([int]$clientPosition.Groups[2].Value - [int]$serverPosition.Groups[1].Value) -gt 200 -or
                [math]::Abs([int]$clientPosition.Groups[3].Value - [int]$serverPosition.Groups[2].Value) -gt 200) {
                throw "$label missed the authoritative $DeathResolution position"
            }
        }
        $killText = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
        if ($killText -notmatch 'COMBAT_PILOT_PLAYER_HIT HP=0 By=SFPilotBoar' -or
            $killText -notmatch 'COMBAT_DEATH_RECORDED Character=chr_dev_alpha' -or
            $serverText -notmatch "COMBAT_DEATH_RESOLVED Event=.* HP=$expected Mode=$DeathResolution" -or
            $serverText -notmatch $(if ($DeathResolution -eq 'Release') { 'SPIRIT_RELEASE_SERVER_DENIED' } else { 'RES_ACCEPT_SERVER_DENIED' })) {
            throw 'Death, durable clear, or replay denial evidence missing'
        }
        if ($DeathResolution -eq 'Resurrection') {
            $badEvent = $serverText.IndexOf('Reason=DeathEvent')
            $resolvedAt = $serverText.IndexOf('Mode=Resurrection')
            $replayAt = if ($resolvedAt -ge 0) { $serverText.IndexOf('RES_ACCEPT_SERVER_DENIED Pawn=', $resolvedAt) } else { -1 }
            if ($badEvent -lt 0 -or $resolvedAt -le $badEvent -or $replayAt -le $resolvedAt) {
                throw 'Mismatched death event or accepted-offer replay was not denied in order'
            }
        }
        $aliveRow = Invoke-LedgerSql $fx "SELECT is_dead FROM character_state.deaths WHERE character_id='chr_dev_alpha'" $fx.db
        if ($aliveRow -ne 'f') { throw "Death was not durably cleared: $aliveRow" }
        $positionRow = Invoke-LedgerSql $fx "SELECT round(p.x)::int||'|'||round(p.y)::int||'|'||round(d.$(if ($DeathResolution -eq 'Release') { 'respawn_x' } else { 'death_x' }))::int||'|'||round(d.$(if ($DeathResolution -eq 'Release') { 'respawn_y' } else { 'death_y' }))::int FROM character_state.positions p JOIN character_state.deaths d USING (character_id) WHERE p.character_id='chr_dev_alpha'" $fx.db
        $positionValues = $positionRow -split '\|'
        if ($positionValues.Count -ne 4 -or $positionValues[0] -ne $positionValues[2] -or
            $positionValues[1] -ne $positionValues[3] -or
            [math]::Abs([int]$positionValues[0] - [int]$serverPosition.Groups[1].Value) -gt 200 -or
            [math]::Abs([int]$positionValues[1] - [int]$serverPosition.Groups[2].Value) -gt 200) {
            throw "Durable $DeathResolution position did not match the authored death record: $positionRow"
        }
        $result.status='Passed'
        return
    }
    $clientUrl = '127.0.0.1:'+$Port + $(if ($Services) { '?SFDevCharacter=alpha' } else { '' })
    if ($Clients -gt 1) {
        $names = @('alpha','beta','gamma','delta','epsilon','zeta','eta','theta','iota','kappa')
        for ($role = 1; $role -le 2; $role++) {
            $identity = $names[$role - 1]
            $null = Start-Owned "client$role" ("127.0.0.1:$Port`?SFDevCharacter=$identity") @('-game',$scenarioArg,"-CombatGroupRole=$role")
            if ($AttributeSnapshotNetcheck) {
                Wait-Log $server 'server' "COMBAT_PILOT_BOAR_HIT Group=boar_farm_edge Slot=0 HP=\d+ Attacker=chr_dev_$identity"
            } else {
                Wait-Log $server 'server' "COMBAT_PILOT_BOAR_HIT Group=boar_farm_edge Slot=0 HP=$(60 - 20*$role) Attacker=chr_dev_$identity"
            }
            Wait-Log $owned[$owned.Count - 1] "client$role" "COMBAT_GROUP_ATTACK Role=$role"
            if ($role -eq 1 -and $Clients -gt 3) {
                for ($observer = 4; $observer -le $Clients; $observer++) {
                    $observerIdentity = $names[$observer - 1]
                    $null = Start-Owned "client$observer" ("127.0.0.1:$Port`?SFDevCharacter=$observerIdentity") @('-game',$scenarioArg,'-CombatGroupRole=0')
                }
            }
        }
        if ($AttributeSnapshotNetcheck) {
            $serverText = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
            if ([regex]::Matches($serverText, 'COMBAT_ATTRIBUTE_SNAPSHOT Archetype=fighter Level=1 Str=30 Dex=15 Con=25 Int=10 Wis=10 Wil=10').Count -lt 2 -or
                $serverText -notmatch 'CHARACTER_RESTORED Character=chr_dev_alpha' -or
                $serverText -notmatch 'CHARACTER_RESTORED Character=chr_dev_beta' -or
                $serverText -match 'COMBAT_PILOT_SLOT_ERROR|Fatal error:|Assertion failed:') {
                throw 'Two-client attribute snapshot or combat evidence incomplete.'
            }
            $result.status='Passed'
            return
        }
        $second = $owned[$owned.Count - 1]
        Stop-Process -Id $second.Id
        $second.WaitForExit()
        $rejoinRole = if ($Clients -eq 2) { 2 } else { 0 }
        $rejoined = Start-Owned 'client2-rejoin' ("127.0.0.1:$Port`?SFDevCharacter=beta") @('-game',$scenarioArg,"-CombatGroupRole=$rejoinRole")
        if ($Clients -gt 3) {
            $third = Start-Owned 'client3' ("127.0.0.1:$Port`?SFDevCharacter=gamma") @('-game',$scenarioArg,'-CombatGroupRole=3','-CombatGroupAttackDelaySeconds=30')
            Wait-Log $rejoined 'client2-rejoin' 'M3_CLIENT_CSV_START Role=0'
        }
        $rejoinWatch = [Diagnostics.Stopwatch]::StartNew()
        while ($Clients -le 3 -and [regex]::Matches((Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw),'COMBAT_GROUP_READY Pawn=').Count -lt 3) {
            if ($rejoined.HasExited -or $rejoinWatch.Elapsed.TotalSeconds -gt $TimeoutSeconds) { throw 'Beta did not reconnect during the fight.' }
            Start-Sleep -Milliseconds 300
        }
        if ($Clients -ge 3) {
            if ($Clients -eq 3) { $third = Start-Owned 'client3' ("127.0.0.1:$Port`?SFDevCharacter=gamma") @('-game',$scenarioArg,'-CombatGroupRole=3') }
            Wait-Log $third 'client3' 'COMBAT_GROUP_ATTACK Role=3'
        } else { Wait-Log $rejoined 'client2-rejoin' 'COMBAT_GROUP_ATTACK Role=2' }
        Wait-Log $server 'server' 'COMBAT_PILOT_BOAR_DEAD Group=boar_farm_edge Slot=0'
        Wait-Log $server 'server' 'COMBAT_PILOT_REWARD Death=boar_farm_edge:0:\d+ Source=service Outcome=0 Granted=1 Code=loot_drop Item=item/boar_hide Quantity=[2-9]'
        $serverText = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
        $hits = [regex]::Matches($serverText,'COMBAT_PILOT_BOAR_HIT Group=boar_farm_edge Slot=0 HP=(\d+) Attacker=(\S+)')
        $reward = [regex]::Match($serverText,'COMBAT_PILOT_REWARD Death=boar_farm_edge:0:\d+ Source=service Outcome=0 Granted=1 Code=loot_drop Item=item/boar_hide Quantity=(\d+)')
        $expectedAttackers = if ($Clients -ge 3) { 'chr_dev_alpha,chr_dev_beta,chr_dev_gamma' } else { 'chr_dev_alpha,chr_dev_beta,chr_dev_beta' }
        if ($hits.Count -ne 3 -or (@($hits | ForEach-Object { $_.Groups[1].Value }) -join ',') -ne '40,20,0' -or
            (@($hits | ForEach-Object { $_.Groups[2].Value }) -join ',') -ne $expectedAttackers -or
            [regex]::Matches($serverText,'COMBAT_PILOT_BOAR_DEAD Group=boar_farm_edge Slot=0').Count -ne 1 -or
            [regex]::Matches($serverText,'COMBAT_PILOT_REWARD .* Source=service .* Granted=1').Count -ne 1 -or
            -not $reward.Success -or
            $serverText -match 'COMBAT_PILOT_SLOT_ERROR|Fatal error:|Assertion failed:') { throw 'Group hit, death, or reward evidence incomplete.' }
        for ($role = 1; $role -le [math]::Min($Clients,3); $role++) {
            $identity = $names[$role - 1]
            $name = "client$role"
            $clientText = Get-Content -LiteralPath (Join-Path $root ($name+'.log')) -Raw
            $selected = [regex]::Match($clientText,"COMBAT_GROUP_TARGET Role=$role Target=(\S+)")
            if (-not $selected.Success -or $selected.Groups[1].Value -eq 'None' -or
                $serverText -notmatch "COMBAT_GROUP_SERVER_TARGET Character=chr_dev_$identity Target=SFPilotBoar_") {
                throw "Owner target replication missing for $identity."
            }
            $view = Get-LedgerView $fx "ctn_dev_$identity"
            $expected = if ($role -eq [math]::Min($Clients,3)) { [int]$reward.Groups[1].Value } else { 0 }
            if ([int]$view.items['item/boar_hide'] -ne $expected) { throw "Unexpected ledger hides for $identity`: $($view.line)" }
            $result["ledger_$identity"] = $view.line
        }
        $rejoinText = Get-Content -LiteralPath (Join-Path $root 'client2-rejoin.log') -Raw
        if (($Clients -ge 3 -and $rejoinText -match 'COMBAT_GROUP_ATTACK Role=2') -or $rejoined.HasExited -or
            ($Clients -ge 3 -and $third.HasExited) -or $server.HasExited) {
            throw 'Mid-fight reconnect replayed an attack or a process exited.'
        }
        if ($Clients -gt 1) {
            Wait-Log $server 'server' 'M3_SERVER_NET DurationSec='
            $liveClients = @('client1','client2-rejoin')
            if ($Clients -ge 3) { $liveClients += 'client3' }
            for ($role = 4; $role -le $Clients; $role++) { $liveClients += "client$role" }
            foreach ($name in $liveClients) {
                $run = @($result.runs | Where-Object name -eq $name)[0]
                $process = @($owned | Where-Object Id -eq $run.pid)[0]
                Wait-Log $process $name 'M3_CLIENT_CSV_STOP Role='
            }
            $serverCsv = Join-Path $csvDir 'server-m3.csv'
            Wait-M3Csv $server $serverCsv
            $clientFrames = [ordered]@{}
            foreach ($name in $liveClients) {
                $run = @($result.runs | Where-Object name -eq $name)[0]
                $process = @($owned | Where-Object Id -eq $run.pid)[0]
                $path = Join-Path $root ($name+'-csv/client-m3.csv')
                Wait-M3Csv $process $path
                $clientFrames[$name] = Get-M3FrameStats $path
            }
            $serverText = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
            $start = [regex]::Match($serverText,'M3_CAPTURE_START TimeSec=([0-9.]+)')
            $net = [regex]::Match($serverText,'M3_SERVER_NET DurationSec=([0-9.]+) InBps=([0-9.]+) OutBps=([0-9.]+) Connections=([^\r\n]+)')
            if (-not $start.Success -or -not $net.Success) { throw 'Server M3 capture markers missing.' }
            $culture = [Globalization.CultureInfo]::InvariantCulture
            $captureStart = [double]::Parse($start.Groups[1].Value,$culture)
            $connections = @($net.Groups[4].Value -split ';' | ForEach-Object {
                $parts = $_ -split ':'
                if ($parts.Count -ne 4) { throw "Malformed M3 connection sample: $_" }
                [ordered]@{ id=[int]$parts[0]; seconds=[double]::Parse($parts[1],$culture)
                    inBps=[double]::Parse($parts[2],$culture); outBps=[double]::Parse($parts[3],$culture) }
            })
            if ($connections.Count -ne ($Clients + 1)) { throw "Expected $($Clients + 1) connection lifetimes, got $($connections.Count)." }
            $rpcNames = @('GrantDefeatReward','ApplyReceipt','GetCharacterState','CreateCharacter','LearnRecipe','ClaimPlot','GetClaim','Place','Remove','Craft','ListStructures','Grant','SavePosition','ReportDeath','ClearDeath','ExchangeWallet','OfferTrade','AcceptTrade','CancelTrade','ListItem','BuyListing','CancelListing','ListListings')
            $rpcSamples = @{}
            foreach ($match in [regex]::Matches($serverText,'M3_GAMEPLAY_SERVICE Rpc=(\d+) StartedSec=([0-9.]+) LatencyMs=([0-9.]+) Status=(\d+)')) {
                if ([double]::Parse($match.Groups[2].Value,$culture) -lt $captureStart) { continue }
                $rpc = [int]$match.Groups[1].Value
                if (-not $rpcSamples.ContainsKey($rpc)) { $rpcSamples[$rpc] = [Collections.Generic.List[double]]::new() }
                $rpcSamples[$rpc].Add([double]::Parse($match.Groups[3].Value,$culture))
            }
            if ($rpcSamples.Count -eq 0) { throw 'No gameplay service latency samples in the capture window.' }
            $serviceLatency = @($rpcSamples.Keys | Sort-Object | ForEach-Object {
                $values = $rpcSamples[$_].ToArray(); [Array]::Sort($values)
                $sum = 0.0; foreach ($value in $values) { $sum += $value }
                $rpcLabel = if ($_ -lt $rpcNames.Count) { $rpcNames[$_] } else { "Rpc$_" }
                [ordered]@{ rpc=$rpcLabel; count=$values.Length; avg=[math]::Round($sum/$values.Length,3)
                    p95=[math]::Round($values[[math]::Ceiling(0.95*$values.Length)-1],3) }
            })
            $metrics = [ordered]@{ schema=1; scenario='CombatGroup'; clients=$Clients; nullRhi=$true
                note='Loopback headless -NullRHI clients; no representative GPU or real-network cost.'
                server=[ordered]@{ frameMs=(Get-M3FrameStats $serverCsv)
                    net=[ordered]@{ seconds=[double]::Parse($net.Groups[1].Value,$culture)
                        inBps=[double]::Parse($net.Groups[2].Value,$culture)
                        outBps=[double]::Parse($net.Groups[3].Value,$culture); perConnection=$connections }
                    gameplayServiceMs=$serviceLatency }
                clientFrameMs=$clientFrames }
            $evidencePath = if ($Clients -eq 3) { Join-Path $repo 'docs/evidence/m3-combat-group.json' }
                else { Join-Path $root 'metrics.json' }
            $json = ($metrics | ConvertTo-Json -Depth 9) -replace "`r`n", "`n"
            [IO.File]::WriteAllText($evidencePath, ($json + "`n"), [Text.UTF8Encoding]::new($false))
            $result.metricsEvidence = $evidencePath
            Write-Output ('M3_SERVER_METRICS '+($metrics.server | ConvertTo-Json -Depth 8 -Compress))
            Write-Output ('M3_CLIENT_FRAME_MS '+($clientFrames | ConvertTo-Json -Depth 4 -Compress))
        }
    } else {
    $client = Start-Owned 'client' $clientUrl @('-game',$scenarioArg)
    if ($TargetScenario) {
        Wait-Log $server 'server' 'TARGET_PILOT_READY First='
        Wait-Log $client 'client' 'TARGET_PILOT_CLIENT_SELECT Step=2'
        Wait-Log $client 'client' 'TARGET_PILOT_CLIENT_DROP Cause=Death'
        Wait-Log $server 'server' 'TARGET_PILOT_SERVER_DROP Cause=Range'
        Wait-Log $client 'client' 'TARGET_PILOT_CLIENT_DROP Cause=Range'
        $serverText = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
        $clientText = Get-Content -LiteralPath (Join-Path $root 'client.log') -Raw
        $selected = [regex]::Matches($clientText, 'TARGET_PILOT_CLIENT_SELECT Step=[12] Target=(\S+)')
        $serverSelected = [regex]::Matches($serverText, 'TARGET_PILOT_SERVER_SELECT Target=(\S+)')
        if ($selected.Count -ne 2 -or $selected[0].Groups[1].Value -eq $selected[1].Groups[1].Value -or
            $serverSelected.Count -lt 3 -or $serverSelected[0].Groups[1].Value -eq $serverSelected[1].Groups[1].Value -or
            $serverText -notmatch 'TARGET_PILOT_SERVER_DROP Cause=Death Target=None' -or
            $serverText -notmatch 'TARGET_PILOT_SERVER_MOVE DistanceCm=6000' -or
            $serverText -match 'COMBAT_PILOT_SLOT_ERROR|Fatal error:|Assertion failed:' -or
            $server.HasExited -or $client.HasExited) { throw 'Target scenario evidence incomplete or a process failed.' }
    } else {
        Wait-Log $client 'client' 'COMBAT_PILOT_CLIENT_ATTACK Index=3'
        Wait-Log $server 'server' 'COMBAT_PILOT_BOAR_DEAD Group=boar_farm_edge Slot=0'
        if ($Services) {
            Wait-Log $server 'server' 'COMBAT_PILOT_REWARD Death=boar_farm_edge:0:\d+ Source=service Outcome=0 Granted=1 Code=loot_drop Item=item/boar_hide Quantity=[2-9]'
            $view = Get-LedgerView $fx 'ctn_dev_alpha'
            if ($view.items['item/boar_hide'] -lt 2) { throw "Boar hides missing from durable ledger: $($view.line)" }
            $result.ledger = $view.line
        } else {
            Wait-Log $server 'server' 'COMBAT_PILOT_REWARD Death=boar_farm_edge:0:\d+ Hides=2 Granted=1 Total=2'
            Wait-Log $client 'client' 'COMBAT_PILOT_CLIENT_HIDES Count=2'
        }
        Wait-Log $server 'server' 'COMBAT_PILOT_PLAYER_DEAD'
        Wait-Log $server 'server' 'COMBAT_PILOT_PLAYER_RESPAWN X=-35970 Y=-14180 HP=80'
        Wait-Log $server 'server' 'COMBAT_PILOT_SAFE_CHECK Protected=1 PlayerHP=80 BoarHP=60 Reward=0 Passed=1'
        Wait-Log $server 'server' 'COMBAT_PILOT_BOAR_SPAWN Group=boar_farm_edge Slot=0 Cycle=1'
        $serverText = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
        $clientText = Get-Content -LiteralPath (Join-Path $root 'client.log') -Raw
        if ([regex]::Matches($serverText,'COMBAT_PILOT_BOAR_HIT Group=boar_farm_edge Slot=0').Count -ne 3 -or
            [regex]::Matches($clientText,'COMBAT_PILOT_CLIENT_ATTACK Index=').Count -ne 3 -or
            [regex]::Matches($serverText,'COMBAT_PILOT_REWARD .* Granted=1').Count -ne 1 -or
            (-not $Services -and [regex]::Matches($clientText,'COMBAT_PILOT_CLIENT_HIDES Count=[1-9]').Count -ne 1) -or
            $serverText -match 'COMBAT_PILOT_SLOT_ERROR|Fatal error:|Assertion failed:') { throw 'Combat evidence incomplete or server failed.' }
        if ($server.HasExited -or $client.HasExited) { throw 'A network process exited before the final respawn barrier.' }
    }
    }
    $result.status='Passed'
} catch { $result.status='Failed'; $result.error=$_.Exception.Message; throw }
finally {
    $wall.Stop(); $result.wallSeconds = [math]::Round($wall.Elapsed.TotalSeconds,1)
    foreach ($process in $owned) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
        foreach ($run in $result.runs) { if ($run.pid -eq $process.Id) { $run.exitCode=$process.ExitCode } }
    }
    if ($Services) { Remove-LedgerFixture $fx }
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($result.status)"
}
