[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17841,
    [ValidateRange(320,1200)][int]$TimeoutSeconds = 600,
    [ValidateRange(1,10)][int]$Clients = 1,
    [switch]$TargetScenario,
    [switch]$Services
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repo 'GameClient\ShatteredFrontier.uproject'
$bridge = Join-Path $repo 'BackendServices\WorldContent.Tool\bin\Debug\net10.0\WorldContent.Tool.dll'
$sdk = Join-Path $repo 'Artifacts\Persistence\Toolchains\dotnet-10.0.402\dotnet.exe'
if ($Services -and $TargetScenario) { throw 'Services ledger assertion requires the combat reward scenario.' }
if ($Clients -gt 1 -and (-not $Services -or $TargetScenario)) { throw 'Group combat requires -Services and does not combine with -TargetScenario.' }
foreach ($required in @($editor,$project,$bridge,$sdk)) {
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
$scope = if ($Clients -gt 1) { "$Clients clients, one boar, one mid-fight reconnect, owner targeting and durable killer reward" }
    elseif ($TargetScenario) { 'One dedicated server, one client; Tab selection, owner replication, death and range drops' }
    elseif ($Services) { 'One dedicated server, one client, durable boar hides in InventoryLedger and WorldContent slot respawn' }
    else { 'One dedicated server, one client; local attack RPC and server-authored WorldContent slots' }
$result = [ordered]@{ status='Running'; map='/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'; scope=$scope; runs=@() }
$wall = [Diagnostics.Stopwatch]::StartNew()
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra) {
    $log = Join-Path $root ($Name+'.log')
    $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $arguments = @(('"'+$project+'"'),$Url,'-unattended','-NullRHI','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    if ($Clients -gt 1 -and $Name -like 'client*') {
        $clientCsvDir = Join-Path $root ($Name+'-csv')
        $null = New-Item -ItemType Directory -Path $clientCsvDir
        $arguments += ('-M3CsvDir="'+$clientCsvDir+'"')
    }
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt'))
    $null = $process.Handle
    $owned.Add($process)
    $result.runs += [ordered]@{ name=$Name; pid=$process.Id; log=$log; arguments=$arguments }
    return $process
}
function Wait-Log([Diagnostics.Process]$Process,[string]$Name,[string]$Pattern) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $log = Join-Path $root ($Name+'.log')
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
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
        $schemas = @('CharacterState.Postgres/Schema.sql','CharacterState.Postgres/Schema.Position.sql','CharacterState.Postgres/Schema.Recipes.sql','CharacterState.Postgres/Schema.Appearance.sql','CharacterState.Postgres/Schema.GatheringSkills.sql','CharacterState.Postgres/Schema.Death.sql','Housing.Postgres/Schema.sql','Exchange.Postgres/Schema.sql','Housing.Postgres/Schema.Gameplay.sql','Housing.Postgres/Schema.Geometry.sql','Housing.Postgres/Schema.Construction.sql') | ForEach-Object { Get-Content (Join-Path $repo "BackendServices/$_") -Raw }
        [void](Invoke-LedgerSql $fx ("SET ROLE ledger_owner;`n" + ($schemas -join "`n")) $fx.db)
        Invoke-LedgerSchemaMigrations $fx @('character_state','housing','exchange')
        [void](Invoke-LedgerSql $fx ('GRANT USAGE ON SCHEMA character_state,housing,exchange TO ledger_svc; ' + ((@('character_state','housing','exchange') | ForEach-Object { "GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA $_ TO ledger_svc;" }) -join ' ')) $fx.db)
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
    $scenarioArg = if ($Clients -gt 1) { '-CombatGroupScenario' } elseif ($TargetScenario) { '-CombatTargetScenario' } else { '-CombatPilotScenario' }
    $serverFlags = @('-server',('-port='+$Port),'-multihome=127.0.0.1',$scenarioArg)
    if ($Clients -gt 3) { $serverFlags += '-CombatGroupCapacitySlots' }
    if ($Clients -gt 1) {
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
    Wait-Log $server 'server' 'COMBAT_PILOT_READY Boars=10'
    $clientUrl = '127.0.0.1:'+$Port + $(if ($Services) { '?SFDevCharacter=alpha' } else { '' })
    if ($Clients -gt 1) {
        $names = @('alpha','beta','gamma','delta','epsilon','zeta','eta','theta','iota','kappa')
        for ($role = 1; $role -le 2; $role++) {
            $identity = $names[$role - 1]
            $null = Start-Owned "client$role" ("127.0.0.1:$Port`?SFDevCharacter=$identity") @('-game',$scenarioArg,"-CombatGroupRole=$role")
            Wait-Log $server 'server' "COMBAT_PILOT_BOAR_HIT Group=boar_farm_edge Slot=0 HP=$(60 - 20*$role) Attacker=chr_dev_$identity"
            Wait-Log $owned[$owned.Count - 1] "client$role" "COMBAT_GROUP_ATTACK Role=$role"
            if ($role -eq 1 -and $Clients -gt 3) {
                for ($observer = 4; $observer -le $Clients; $observer++) {
                    $observerIdentity = $names[$observer - 1]
                    $null = Start-Owned "client$observer" ("127.0.0.1:$Port`?SFDevCharacter=$observerIdentity") @('-game',$scenarioArg,'-CombatGroupRole=0')
                }
            }
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
