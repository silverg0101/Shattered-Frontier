[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17841,
    [ValidateRange(320,600)][int]$TimeoutSeconds = 390,
    [ValidateRange(1,3)][int]$Clients = 1,
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
try {
    if ($Services) {
        New-LedgerFixture $fx $repo $root
        $schemas = @('CharacterState.Postgres/Schema.sql','CharacterState.Postgres/Schema.Position.sql','CharacterState.Postgres/Schema.Recipes.sql','CharacterState.Postgres/Schema.Appearance.sql','CharacterState.Postgres/Schema.GatheringSkills.sql','CharacterState.Postgres/Schema.Death.sql','Housing.Postgres/Schema.sql','Exchange.Postgres/Schema.sql','Housing.Postgres/Schema.Gameplay.sql','Housing.Postgres/Schema.Geometry.sql','Housing.Postgres/Schema.Construction.sql') | ForEach-Object { Get-Content (Join-Path $repo "BackendServices/$_") -Raw }
        [void](Invoke-LedgerSql $fx ("SET ROLE ledger_owner;`n" + ($schemas -join "`n")) $fx.db)
        [void](Invoke-LedgerSql $fx ('GRANT USAGE ON SCHEMA character_state,housing,exchange TO ledger_svc; ' + ((@('character_state','housing','exchange') | ForEach-Object { "GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA $_ TO ledger_svc;" }) -join ' ')) $fx.db)
        $gameplayProject = Join-Path $repo 'BackendServices/Gameplay.Host/Gameplay.Host.csproj'
        $gameplayOut = Join-Path $fx.base 'gameplay-host'
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
    }
    $url = $result.map
    $scenarioArg = if ($Clients -gt 1) { '-CombatGroupScenario' } elseif ($TargetScenario) { '-CombatTargetScenario' } else { '-CombatPilotScenario' }
    $serverFlags = @('-server',('-port='+$Port),'-multihome=127.0.0.1',$scenarioArg)
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
        $names = @('alpha','beta','gamma')
        for ($role = 1; $role -le 2; $role++) {
            $identity = $names[$role - 1]
            $null = Start-Owned "client$role" ("127.0.0.1:$Port`?SFDevCharacter=$identity") @('-game',$scenarioArg,"-CombatGroupRole=$role")
            Wait-Log $server 'server' "COMBAT_PILOT_BOAR_HIT Group=boar_farm_edge Slot=0 HP=$(60 - 20*$role) Attacker=chr_dev_$identity"
            Wait-Log $owned[$owned.Count - 1] "client$role" "COMBAT_GROUP_ATTACK Role=$role"
        }
        $second = $owned[$owned.Count - 1]
        Stop-Process -Id $second.Id
        $second.WaitForExit()
        $rejoinRole = if ($Clients -eq 2) { 2 } else { 0 }
        $rejoined = Start-Owned 'client2-rejoin' ("127.0.0.1:$Port`?SFDevCharacter=beta") @('-game',$scenarioArg,"-CombatGroupRole=$rejoinRole")
        $rejoinWatch = [Diagnostics.Stopwatch]::StartNew()
        while ([regex]::Matches((Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw),'COMBAT_GROUP_READY Pawn=').Count -lt 3) {
            if ($rejoined.HasExited -or $rejoinWatch.Elapsed.TotalSeconds -gt $TimeoutSeconds) { throw 'Beta did not reconnect during the fight.' }
            Start-Sleep -Milliseconds 300
        }
        if ($Clients -eq 3) {
            $third = Start-Owned 'client3' ("127.0.0.1:$Port`?SFDevCharacter=gamma") @('-game',$scenarioArg,'-CombatGroupRole=3')
            Wait-Log $third 'client3' 'COMBAT_GROUP_ATTACK Role=3'
        } else { Wait-Log $rejoined 'client2-rejoin' 'COMBAT_GROUP_ATTACK Role=2' }
        Wait-Log $server 'server' 'COMBAT_PILOT_BOAR_DEAD Group=boar_farm_edge Slot=0'
        Wait-Log $server 'server' 'COMBAT_PILOT_REWARD Death=boar_farm_edge:0:\d+ Source=service Outcome=0 Granted=1 Code=loot_drop Item=item/boar_hide Quantity=[2-9]'
        $serverText = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
        $hits = [regex]::Matches($serverText,'COMBAT_PILOT_BOAR_HIT Group=boar_farm_edge Slot=0 HP=(\d+) Attacker=(\S+)')
        $expectedAttackers = if ($Clients -eq 3) { 'chr_dev_alpha,chr_dev_beta,chr_dev_gamma' } else { 'chr_dev_alpha,chr_dev_beta,chr_dev_beta' }
        if ($hits.Count -ne 3 -or (@($hits | ForEach-Object { $_.Groups[1].Value }) -join ',') -ne '40,20,0' -or
            (@($hits | ForEach-Object { $_.Groups[2].Value }) -join ',') -ne $expectedAttackers -or
            [regex]::Matches($serverText,'COMBAT_PILOT_BOAR_DEAD Group=boar_farm_edge Slot=0').Count -ne 1 -or
            [regex]::Matches($serverText,'COMBAT_PILOT_REWARD .* Source=service .* Granted=1').Count -ne 1 -or
            $serverText -match 'COMBAT_PILOT_SLOT_ERROR|Fatal error:|Assertion failed:') { throw 'Group hit, death, or reward evidence incomplete.' }
        for ($role = 1; $role -le $Clients; $role++) {
            $identity = $names[$role - 1]
            $name = "client$role"
            $clientText = Get-Content -LiteralPath (Join-Path $root ($name+'.log')) -Raw
            $selected = [regex]::Match($clientText,"COMBAT_GROUP_TARGET Role=$role Target=(\S+)")
            if (-not $selected.Success -or $selected.Groups[1].Value -eq 'None' -or
                $serverText -notmatch "COMBAT_GROUP_SERVER_TARGET Character=chr_dev_$identity Target=SFPilotBoar_") {
                throw "Owner target replication missing for $identity."
            }
            $view = Get-LedgerView $fx "ctn_dev_$identity"
            $expected = if ($role -eq $Clients) { 2 } else { 0 }
            if ([int]$view.items['item/boar_hide'] -ne $expected) { throw "Unexpected ledger hides for $identity`: $($view.line)" }
            $result["ledger_$identity"] = $view.line
        }
        $rejoinText = Get-Content -LiteralPath (Join-Path $root 'client2-rejoin.log') -Raw
        if (($Clients -eq 3 -and $rejoinText -match 'COMBAT_GROUP_ATTACK Role=2') -or $rejoined.HasExited -or
            ($Clients -eq 3 -and $third.HasExited) -or $server.HasExited) {
            throw 'Mid-fight reconnect replayed an attack or a process exited.'
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
