[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(1024, 65535)][int]$Port = 17777,
    [ValidatePattern('^/Game/Frontier/[A-Za-z0-9_/]+$')][string]$Map = '/Game/Frontier/Maps/Greenlands_E1',
    [ValidateRange(1, 2)][int]$Clients = 2,
    [switch]$CastingScenario,
    [switch]$EvadeScenario,
    [switch]$ThreatScenario,
    [switch]$AggroScenario,
    [switch]$MemoryScenario,
    [switch]$RegenScenario,
    [switch]$FallScenario,
    [switch]$SwimScenario,
    [switch]$CrouchScenario,
    [switch]$BackpedalScenario,
    [switch]$GreenlandsEnvironmentNetcheck,
    [switch]$AssistScenario,
    [switch]$RespawnScenario,
    [switch]$GreenlandsBoarNetcheck
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logs = Join-Path $repoRoot ('GameClient\Saved\Logs\E1-Smoke-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$null = New-Item -ItemType Directory -Path $logs
$ownedProcesses = [Collections.Generic.List[Diagnostics.Process]]::new()
function Launch([string[]]$LaunchArguments) {
    $process = Start-Process -FilePath $editor -ArgumentList (@('"' + $project + '"') + $LaunchArguments) -WindowStyle Hidden -PassThru
    $null = $process.Handle  # PS 5.1 keeps ExitCode only if the handle is opened before exit
    $ownedProcesses.Add($process)
    Write-Output "Started PID $($process.Id)"
}
function Wait-Log([string]$Path, [string]$Pattern, [int]$Count = 1) {
    $deadline = (Get-Date).AddSeconds($(if ($RespawnScenario -or $GreenlandsBoarNetcheck) { 420 } elseif ($EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or $RegenScenario -or $AssistScenario) { 240 } else { 120 }))
    while ((Get-Date) -lt $deadline) {
        foreach ($process in $ownedProcesses) { if ($process.HasExited) { throw "Network process $($process.Id) exited before $Pattern" } }
        if (Test-Path -LiteralPath $Path) {
            $matchesFound = @(Select-String -LiteralPath $Path -Pattern $Pattern)
            if ($matchesFound.Count -ge $Count) { return }
        }
        Start-Sleep -Milliseconds 500
    }
    throw "Timed out waiting for $Count matches of $Pattern in $Path"
}
function Assert-EvadeClient([string]$Path, [string]$Role) {
    $pattern = 'BOAR_EVADE_SAMPLE Role=' + $Role + ' T=([\d.]+) X=([-\d.]+) Y=([-\d.]+) HP=(\d+) Returning=([01]) PawnX=([-\d.]+) Target=([01])'
    $samples = @(Select-String -LiteralPath $Path -Pattern $pattern | ForEach-Object {
        $g = $_.Matches[0].Groups
        [pscustomobject]@{ T=[double]::Parse($g[1].Value,[Globalization.CultureInfo]::InvariantCulture)
            X=[double]::Parse($g[2].Value,[Globalization.CultureInfo]::InvariantCulture)
            Y=[double]::Parse($g[3].Value,[Globalization.CultureInfo]::InvariantCulture)
            HP=[int]$g[4].Value; Returning=[int]$g[5].Value; PawnX=[double]::Parse($g[6].Value,[Globalization.CultureInfo]::InvariantCulture)
            Target=[int]$g[7].Value }
    })
    $hurt = @($samples | Where-Object { $_.HP -gt 0 -and $_.HP -lt 60 -and $_.Returning -eq 0 } | Select-Object -First 1)
    $return = @($samples | Where-Object { $_.Returning -eq 1 -and $_.X -gt 2300 })
    if ($hurt.Count -ne 1 -or $return.Count -lt 10) { throw "$Role missed damage or replicated return" }
    $reset = @($samples | Where-Object { $_.Returning -eq 0 -and $_.HP -eq 60 -and $_.T -gt $return[-1].T -and [math]::Abs($_.X-1800) -lt 120 -and [math]::Abs($_.Y) -lt 120 } | Select-Object -First 1)
    if ($reset.Count -ne 1) { throw "$Role missed full health and spawn reset" }
    $reengaged = @($samples | Where-Object { $_.T -gt $reset[0].T -and $_.HP -gt 0 -and $_.HP -lt 60 } | Select-Object -First 1)
    if ($reengaged.Count -ne 1) { throw "$Role missed re-engagement" }
    if (@($return | Select-Object -Skip 2 | Where-Object { $_.Target -ne 0 -or $_.HP -ne $hurt[0].HP }).Count -gt 0) {
        throw "$Role saw threat or health change during immune return"
    }
    $travel = $return[0].X - $return[-1].X
    $duration = $return[-1].T - $return[0].T
    $speed = $travel / $duration
    if ($duration -lt 5 -or $travel -lt 2500 -or $speed -lt 450 -or $speed -gt 600) {
        throw "$Role return speed $speed cm/s outside 1.5x chase-speed acceptance"
    }
    return [math]::Round($speed,1)
}
try {
    if ($BackpedalScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_Backpedal' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or
        $RegenScenario -or $FallScenario -or $SwimScenario -or $CrouchScenario -or $AssistScenario -or
        $RespawnScenario -or $GreenlandsBoarNetcheck -or $GreenlandsEnvironmentNetcheck)) {
        throw 'BackpedalScenario requires SB_Backpedal, two clients, and no other scenario'
    }
    if ($CrouchScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_Crouch' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or
        $RegenScenario -or $FallScenario -or $SwimScenario -or $AssistScenario -or $RespawnScenario -or
        $GreenlandsBoarNetcheck -or $GreenlandsEnvironmentNetcheck)) {
        throw 'CrouchScenario requires SB_Crouch, two clients, and no other scenario'
    }
    if ($GreenlandsEnvironmentNetcheck) {
        if ($Map -ne '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook' -or $Clients -ne 2 -or
            $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or
            $RegenScenario -or $FallScenario -or $SwimScenario -or $AssistScenario -or $RespawnScenario -or $GreenlandsBoarNetcheck) {
            throw 'GreenlandsEnvironmentNetcheck requires Greenlands_Silverbrook, two clients, and no other scenario'
        }
    }
    if ($GreenlandsBoarNetcheck) {
        if ($Map -ne '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook' -or $Clients -ne 2 -or
            $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or
            $RegenScenario -or $AssistScenario -or $RespawnScenario) {
            throw 'GreenlandsBoarNetcheck requires Greenlands_Silverbrook, two clients, and no other scenario'
        }
        foreach ($required in @((Join-Path $repoRoot 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'),
                (Join-Path $repoRoot 'BackendServices/WorldContent.Tool/bin/Debug/net10.0/WorldContent.Tool.dll'))) {
            if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing WorldContent bridge: $required" }
        }
    }
    if ($CastingScenario -and $Map -ne '/Game/Frontier/Maps/Sandbox/SB_Casting') { throw 'CastingScenario requires SB_Casting' }
    if ($EvadeScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_Evade' -or $Clients -ne 2 -or $CastingScenario)) {
        throw 'EvadeScenario requires SB_Evade, two clients, and no CastingScenario'
    }
    if ($ThreatScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_Threat' -or $Clients -ne 2 -or $CastingScenario -or $EvadeScenario)) {
        throw 'ThreatScenario requires SB_Threat, two clients, and no other scenario'
    }
    if ($AggroScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_Aggro' -or $Clients -ne 2 -or $CastingScenario -or $EvadeScenario -or $ThreatScenario)) {
        throw 'AggroScenario requires SB_Aggro, two clients, and no other scenario'
    }
    if ($MemoryScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_AttackerMemory' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario)) {
        throw 'MemoryScenario requires SB_AttackerMemory, two clients, and no other scenario'
    }
    if ($RegenScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_Regeneration' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario)) {
        throw 'RegenScenario requires SB_Regeneration, two clients, and no other scenario'
    }
    if ($FallScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_FallDamage' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or
        $RegenScenario -or $AssistScenario -or $RespawnScenario -or $GreenlandsBoarNetcheck)) {
        throw 'FallScenario requires SB_FallDamage, two clients, and no other scenario'
    }
    if ($SwimScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_Swimming' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or
        $RegenScenario -or $FallScenario -or $AssistScenario -or $RespawnScenario -or $GreenlandsBoarNetcheck)) {
        throw 'SwimScenario requires SB_Swimming, two clients, and no other scenario'
    }
    if ($AssistScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_Assist' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or $RegenScenario)) {
        throw 'AssistScenario requires SB_Assist, two clients, and no other scenario'
    }
    if ($RespawnScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_Respawn' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or $RegenScenario -or $AssistScenario)) {
        throw 'RespawnScenario requires SB_Respawn, two clients, and no other scenario'
    }
    if (Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue) { throw "UDP $Port is already in use" }
    $serverLog = Join-Path $logs 'Server.log'
    $serverArgs = @($Map, '-server', '-NullRHI', '-unattended', '-nosound', '-nop4', "-port=$Port", '-MULTIHOME=127.0.0.1', ('-abslog="' + $serverLog + '"'))
    if ($EvadeScenario) { $serverArgs += '-CombatEvadeScenario' }
    if ($ThreatScenario) { $serverArgs += '-CombatThreatScenario' }
    if ($AggroScenario) { $serverArgs += '-CombatAggroScenario' }
    if ($MemoryScenario) { $serverArgs += '-CombatMemoryScenario' }
    if ($RegenScenario) { $serverArgs += '-CombatRegenScenario' }
    if ($FallScenario) { $serverArgs += '-CombatFallScenario' }
    if ($AssistScenario) { $serverArgs += '-CombatAssistScenario' }
    if ($RespawnScenario) { $serverArgs += '-CombatRespawnScenario' }
    if ($GreenlandsBoarNetcheck) { $serverArgs += '-GreenlandsBoarNetcheck' }
    if ($GreenlandsEnvironmentNetcheck) { $serverArgs += '-GreenlandsEnvironmentNetcheck' }
    if ($CrouchScenario) { $serverArgs += '-CrouchScenario' }
    if ($BackpedalScenario) { $serverArgs += '-BackpedalScenario' }
    Launch $serverArgs
    Wait-Log $serverLog 'E1_WORLD_READY.*NetMode=1'
    foreach ($index in 1..$Clients) {
        $clientLog = Join-Path $logs "Client$index.log"
        $clientArgs = @("127.0.0.1:$Port", '-game', '-NullRHI', '-unattended', '-nosound', '-nop4', ('-abslog="' + $clientLog + '"'))
        if ($CastingScenario -and $index -eq 1) { $clientArgs += '-CastingScenario' }
        if ($EvadeScenario) { $clientArgs += @('-CombatEvadeScenario', ('-CombatEvadeRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($ThreatScenario) { $clientArgs += @('-CombatThreatScenario', ('-CombatThreatRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($AggroScenario) { $clientArgs += @('-CombatAggroScenario', ('-CombatAggroRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($MemoryScenario) { $clientArgs += @('-CombatMemoryScenario', ('-CombatMemoryRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($RegenScenario) { $clientArgs += @('-CombatRegenScenario', ('-CombatRegenRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($FallScenario) { $clientArgs += @('-CombatFallScenario', ('-CombatFallRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($SwimScenario) { $clientArgs += @('-SwimScenario', ('-SwimRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($CrouchScenario) { $clientArgs += @('-CrouchScenario', ('-CrouchRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($BackpedalScenario) { $clientArgs += @('-BackpedalScenario', ('-BackpedalRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($AssistScenario) { $clientArgs += @('-CombatAssistScenario', ('-CombatAssistRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($RespawnScenario) { $clientArgs += @('-CombatRespawnScenario', ('-CombatRespawnRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($GreenlandsBoarNetcheck) { $clientArgs += @('-GreenlandsBoarNetcheck', ('-GreenlandsBoarRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($GreenlandsEnvironmentNetcheck) { $clientArgs += @('-GreenlandsEnvironmentNetcheck', ('-GreenlandsEnvironmentRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        Launch $clientArgs
        Wait-Log $clientLog 'Welcomed by server'
    }
    Wait-Log $serverLog 'E1_POSSESSION.*Pawn=SFE1Character.*Authority=1' $Clients
    $possessions = @(Select-String -LiteralPath $serverLog -Pattern 'E1_POSSESSION Controller=(\S+) Pawn=(SFE1Character\S*) Authority=1')
    $controllers = @($possessions | ForEach-Object { $_.Matches[0].Groups[1].Value } | Select-Object -Unique)
    $pawns = @($possessions | ForEach-Object { $_.Matches[0].Groups[2].Value } | Select-Object -Unique)
    if ($controllers.Count -ne $Clients -or $pawns.Count -ne $Clients) { throw "Expected $Clients distinct authoritative controller/pawn pairs" }
    if (@(Select-String -LiteralPath $serverLog -Pattern "E1_PLAYER_JOIN.*Players=$Clients.*Authority=1").Count -lt 1) { throw "Server did not report $Clients players" }
    if ($CastingScenario) {
        Wait-Log (Join-Path $logs 'Client1.log') 'CASTING_SCENARIO_PASS'
        Wait-Log $serverLog 'CASTING_SERVER_QUEUE Ability=2'
        Wait-Log $serverLog 'CASTING_SERVER_COMPLETE Ability=2'
        Write-Output "PASS: server-authoritative cast, queued channel and replicated training-target damage. Logs: $logs"
    }
    if ($EvadeScenario) {
        $driverLog = Join-Path $logs 'Client1.log'
        $observerLog = Join-Path $logs 'Client2.log'
        Wait-Log $driverLog 'BOAR_EVADE_REENGAGED HP='
        Wait-Log $observerLog 'BOAR_EVADE_SAMPLE Role=Observer.*HP=40 Returning=0'
        Wait-Log $serverLog 'BOAR_EVADE_START Group=evade_sandbox'
        Wait-Log $serverLog 'BOAR_EVADE_RESET Group=evade_sandbox'
        $crossed = Select-String -LiteralPath $driverLog -Pattern 'BOAR_EVADE_LEASH_CROSSED PawnX=([\d.]+)' | Select-Object -First 1
        if (-not $crossed -or [double]::Parse($crossed.Matches[0].Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture) -le 7800) {
            throw 'Driver did not travel beyond the 60 m spawn leash'
        }
        $driverSpeed = Assert-EvadeClient $driverLog 'Driver'
        $observerSpeed = Assert-EvadeClient $observerLog 'Observer'
        Write-Output "PASS: boar evade aggro, >60m leash, replicated 1.5x return, threat clear, full reset and re-engagement on both clients ($driverSpeed/$observerSpeed cm/s). Logs: $logs"
    }
    if ($ThreatScenario) {
        $driverLog = Join-Path $logs 'Client1.log'
        $observerLog = Join-Path $logs 'Client2.log'
        Wait-Log $driverLog 'BOAR_THREAT_STICK Role=Driver HP=20'
        Wait-Log $observerLog 'BOAR_THREAT_STICK Role=Observer HP=20'
        Wait-Log $serverLog 'BOAR_THREAT_TARGET Group=threat_sandbox' 2
        $hits = @(Select-String -LiteralPath $serverLog -Pattern 'COMBAT_PILOT_BOAR_HIT Group=threat_sandbox Slot=0 HP=(40|20) Attacker=(\S+)')
        if ($hits.Count -ne 2 -or @($hits | ForEach-Object { $_.Matches[0].Groups[2].Value } | Select-Object -Unique).Count -ne 2) {
            throw 'Two distinct authoritative clients did not land the expected damage sequence'
        }
        foreach ($role in @('Driver','Observer')) {
            $path = $(if ($role -eq 'Driver') { $driverLog } else { $observerLog })
            $initial = @(Select-String -LiteralPath $path -Pattern "BOAR_THREAT_INITIAL Role=$role HP=60")
            $switch = @(Select-String -LiteralPath $path -Pattern "BOAR_THREAT_SWITCH Role=$role HP=40")
            $stick = @(Select-String -LiteralPath $path -Pattern "BOAR_THREAT_STICK Role=$role HP=20")
            if ($initial.Count -ne 1 -or $switch.Count -ne 1 -or $stick.Count -ne 1 -or
                $initial[0].LineNumber -ge $switch[0].LineNumber -or $switch[0].LineNumber -ge $stick[0].LineNumber) {
                throw "$role did not observe ordered pull, switch and retained target"
            }
        }
        Write-Output "PASS: two clients pulled, landed distinct server-accepted hits, observed threat switch and retained target. Logs: $logs"
    }
    if ($AggroScenario) {
        $driverLog = Join-Path $logs 'Client1.log'
        $observerLog = Join-Path $logs 'Client2.log'
        Wait-Log $driverLog 'BOAR_AGGRO_ACQUIRED DistanceCm='
        Wait-Log $observerLog 'BOAR_AGGRO_OBSERVER_ACQUIRED'
        Wait-Log $serverLog 'BOAR_AGGRO_DETECTED Group=aggro_sandbox'
        $blocked = Select-String -LiteralPath $driverLog -Pattern 'BOAR_AGGRO_OCCLUDED DistanceCm=([\d.]+)' | Select-Object -First 1
        $acquired = Select-String -LiteralPath $driverLog -Pattern 'BOAR_AGGRO_ACQUIRED DistanceCm=([\d.]+)' | Select-Object -First 1
        $clear = Select-String -LiteralPath $observerLog -Pattern 'BOAR_AGGRO_OBSERVER_CLEAR' | Select-Object -First 1
        $observed = Select-String -LiteralPath $observerLog -Pattern 'BOAR_AGGRO_OBSERVER_ACQUIRED' | Select-Object -First 1
        $serverDetect = Select-String -LiteralPath $serverLog -Pattern 'BOAR_AGGRO_DETECTED Group=aggro_sandbox Character=\S+ Level=(\d+) DistanceCm=([\d.]+) RadiusCm=([\d.]+)' | Select-Object -First 1
        if (-not $blocked -or -not $acquired -or $blocked.LineNumber -ge $acquired.LineNumber -or
            -not $clear -or -not $observed -or $clear.LineNumber -ge $observed.LineNumber -or -not $serverDetect) {
            throw 'Two clients did not observe the ordered occluded, clear-sight, and replicated target sequence'
        }
        $number = [Globalization.CultureInfo]::InvariantCulture
        $blockedDistance = [double]::Parse($blocked.Matches[0].Groups[1].Value, $number)
        $detectedDistance = [double]::Parse($serverDetect.Matches[0].Groups[2].Value, $number)
        $radius = [double]::Parse($serverDetect.Matches[0].Groups[3].Value, $number)
        if ($blockedDistance -ge $radius -or $detectedDistance -gt $radius -or $detectedDistance -lt 150) {
            throw "Aggro range or obstruction was not exercised: blocked=$blockedDistance detected=$detectedDistance radius=$radius"
        }
        if (@(Select-String -LiteralPath $driverLog -Pattern 'BOAR_AGGRO_OCCLUSION_FAILED').Count -gt 0) { throw 'Sight wall did not block detection' }
        Write-Output "PASS: client 1 stayed undetected behind a wall inside $radius cm, then cleared sight and was acquired at $detectedDistance cm; client 2 observed target replication. Logs: $logs"
    }
    if ($MemoryScenario) {
        $driverLog = Join-Path $logs 'Client1.log'
        $observerLog = Join-Path $logs 'Client2.log'
        Wait-Log $driverLog 'BOAR_MEMORY_REENGAGED HP=60'
        Wait-Log $observerLog 'BOAR_MEMORY_OBSERVED_REENGAGE'
        Wait-Log $serverLog 'BOAR_MEMORY_REACQUIRED Group=memory_sandbox'
        $driverSteps = @('BOAR_MEMORY_INITIAL_NEUTRAL', 'BOAR_MEMORY_DAMAGED HP=40',
            'BOAR_MEMORY_LEASH_CROSSED', 'BOAR_MEMORY_FULL_RESET', 'BOAR_MEMORY_REENGAGED HP=60')
        $lines = @($driverSteps | ForEach-Object {
            $match = Select-String -LiteralPath $driverLog -Pattern $_ | Select-Object -First 1
            if (-not $match) { throw "Driver missed $_" }
            $match.LineNumber
        })
        for ($index = 1; $index -lt $lines.Count; $index++) {
            if ($lines[$index] -le $lines[$index-1]) { throw 'Driver sequence is out of order' }
        }
        $neutral = Select-String -LiteralPath $observerLog -Pattern 'BOAR_MEMORY_NEUTRAL DistanceCm=([\d.]+)' | Select-Object -First 1
        $observed = Select-String -LiteralPath $observerLog -Pattern 'BOAR_MEMORY_OBSERVED_REENGAGE' | Select-Object -First 1
        if (-not $neutral -or $neutral.LineNumber -ge $observed.LineNumber) { throw 'Observer missed neutral-control then reacquisition sequence' }
        $distance = [double]::Parse($neutral.Matches[0].Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture)
        if ($distance -ge 1200 -or $distance -le 150) { throw "Neutral control was outside sight range: $distance cm" }
        if (@(Select-String -LiteralPath $driverLog,$observerLog -Pattern 'BOAR_MEMORY_(PREATTACK_TARGET|NEUTRAL_FAILED)').Count -gt 0) {
            throw 'Defensive boar targeted a character before the remembered attack'
        }
        $hit = Select-String -LiteralPath $serverLog -Pattern 'COMBAT_PILOT_BOAR_HIT Group=memory_sandbox Slot=0 HP=40 Attacker=(\S+)' | Select-Object -First 1
        $reset = Select-String -LiteralPath $serverLog -Pattern 'BOAR_MEMORY_RESET Group=memory_sandbox Attackers=1 RetentionSeconds=60' | Select-Object -First 1
        $reacquired = Select-String -LiteralPath $serverLog -Pattern 'BOAR_MEMORY_REACQUIRED Group=memory_sandbox Character=(\S+) Threat=1' | Select-Object -First 1
        if (-not $hit -or -not $reset -or -not $reacquired -or $hit.LineNumber -ge $reset.LineNumber -or
            $reset.LineNumber -ge $reacquired.LineNumber -or $hit.Matches[0].Groups[1].Value -ne $reacquired.Matches[0].Groups[1].Value) {
            throw 'Server did not preserve the actual attacker across evade reset'
        }
        Write-Output "PASS: server accepted damage, leashed and reset, ignored visible bystander at $distance cm, then gave the original attacker threat 1; both clients observed the sequence. Logs: $logs"
    }
    if ($RegenScenario) {
        Wait-Log $serverLog 'COMBAT_PILOT_PLAYER_HIT HP=([1-9][0-9]*) By=SFPilotBoar'
        Wait-Log $serverLog 'BOAR_EVADE_RESET Group=regeneration_sandbox'
        Wait-Log $serverLog 'REGEN_COMBAT_EXIT Health='
        Wait-Log $serverLog 'REGEN_HEALTH_TICK Health=80 '
        $invariant = [Globalization.CultureInfo]::InvariantCulture
        foreach ($role in @('Driver', 'Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            Wait-Log $path "REGEN_SAMPLE Role=$role.*Health=80 Combat=0.*X=8"
            $samples = @(Select-String -LiteralPath $path -Pattern "REGEN_SAMPLE Role=$role Time=([\d.]+) Health=(\d+) Combat=([01]) X=([-\d.]+)" | ForEach-Object {
                $g = $_.Matches[0].Groups
                [pscustomobject]@{ T=[double]::Parse($g[1].Value,$invariant); HP=[int]$g[2].Value; Combat=[int]$g[3].Value;
                    X=[double]::Parse($g[4].Value,$invariant) }
            })
            $hurt = @($samples | Where-Object { $_.HP -lt 80 -and $_.HP -gt 0 -and $_.Combat -eq 1 } | Select-Object -First 1)
            if ($hurt.Count -ne 1) { throw "$role did not observe replicated combat damage" }
            $exit = @($samples | Where-Object { $_.T -gt $hurt[0].T -and $_.Combat -eq 0 -and $_.HP -eq $hurt[0].HP } | Select-Object -First 1)
            if ($exit.Count -ne 1 -or $exit[0].T - $hurt[0].T -lt 4.0) { throw "$role did not observe five-second combat exit without early healing" }
            $healed = @($samples | Where-Object { $_.T -gt $exit[0].T -and $_.HP -gt $exit[0].HP } | Select-Object -First 1)
            if ($healed.Count -ne 1 -or $healed[0].T - $exit[0].T -lt 4.0) { throw "$role did not observe delayed regeneration" }
            if (@($samples | Where-Object { $_.T -lt $healed[0].T -and $_.T -ge $hurt[0].T -and $_.HP -gt $hurt[0].HP }).Count -gt 0) {
                throw "$role observed health increase during combat or delay"
            }
        }
        Write-Output "PASS: boar damaged client 1; threat cleared on evade, then both clients observed five-second combat exit, five-second regeneration delay, and one-second health ticks to full. Logs: $logs"
    }
    if ($FallScenario) {
        Wait-Log $serverLog 'FALL_DAMAGE ImpactMps='
        $hit = Select-String -LiteralPath $serverLog -Pattern 'FALL_DAMAGE ImpactMps=([\d.]+) Damage=(\d+) HealthBefore=(\d+) Pawn=(\S+)' | Select-Object -First 1
        $g = $hit.Matches[0].Groups
        $impact = [double]::Parse($g[1].Value,[Globalization.CultureInfo]::InvariantCulture)
        $damage = [int]$g[2].Value
        $before = [int]$g[3].Value
        $rawCoefficient = ($impact - 12.0) / 20.0
        $coefficient = [math]::Min(1.2, [math]::Max(0.0, $rawCoefficient))
        $expected = [int][math]::Floor($before * $coefficient + 0.5)
        if ($impact -le 12 -or $damage -ne $expected -or $damage -ge $before) {
            throw "Fall calculation invalid: impact=$impact damage=$damage expected=$expected before=$before"
        }
        Wait-Log $serverLog "COMBAT_PILOT_PLAYER_HIT HP=$($before-$damage) By=None"
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            Wait-Log $path "FALL_SAMPLE Role=$role .*Health=$($before-$damage) "
            $samples = @(Select-String -LiteralPath $path -Pattern "FALL_SAMPLE Role=$role Time=([\d.]+) Health=(\d+) X=([-\d.]+) Z=([-\d.]+)" | ForEach-Object {
                $m = $_.Matches[0].Groups
                [pscustomobject]@{ HP=[int]$m[2].Value; Z=[double]::Parse($m[4].Value,[Globalization.CultureInfo]::InvariantCulture) }
            })
            if (@($samples | Where-Object { $_.HP -eq $before -and $_.Z -gt 1800 }).Count -eq 0 -or
                @($samples | Where-Object { $_.HP -eq ($before-$damage) -and $_.Z -lt 300 }).Count -eq 0) {
                throw "$role missed the elevated start or replicated post-landing health"
            }
        }
        Write-Output "PASS: server measured $impact m/s landing, applied $damage physical environment damage; both clients observed the elevated start and landed health. Logs: $logs"
    }
    if ($CrouchScenario) {
        Wait-Log (Join-Path $logs 'Client1.log') 'CROUCH_DRIVER_COMPLETE'
        $serverSamples = @(Select-String -LiteralPath $serverLog -Pattern 'CROUCH_SERVER_SAMPLE Pawn=(\S+) T=([\d.]+) X=([-\d.]+) C=([01]) Speed=([\d.]+)' | ForEach-Object {
            $g = $_.Matches[0].Groups
            [pscustomobject]@{ Pawn=$g[1].Value; X=[double]::Parse($g[3].Value,[Globalization.CultureInfo]::InvariantCulture);
                C=[int]$g[4].Value; Speed=[double]::Parse($g[5].Value,[Globalization.CultureInfo]::InvariantCulture) }
        })
        $crouched = @($serverSamples | Where-Object { $_.C -eq 1 -and $_.Speed -ge 190 -and $_.Speed -le 201 })
        if ($crouched.Count -lt 3) { throw 'Server did not sustain the 2 m/s crouch cap' }
        $movingPawn = $crouched[0].Pawn
        $pawnSamples = @($serverSamples | Where-Object { $_.Pawn -eq $movingPawn })
        $running = @($pawnSamples | Where-Object { $_.C -eq 0 -and $_.Speed -ge 665 -and $_.Speed -le 705 -and $_.X -gt $crouched[-1].X })
        if ($running.Count -lt 3 -or $crouched[-1].X - $crouched[0].X -lt 350 -or $running[-1].X - $crouched[-1].X -lt 900) {
            throw 'Server did not travel crouched then return to 7 m/s run'
        }
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            $samples = @(Select-String -LiteralPath $path -Pattern "CROUCH_CLIENT_SAMPLE Role=$role X=([-\d.]+) C=([01]) Speed=([\d.]+)" | ForEach-Object {
                $g = $_.Matches[0].Groups
                [pscustomobject]@{ X=[double]::Parse($g[1].Value,[Globalization.CultureInfo]::InvariantCulture); C=[int]$g[2].Value }
            })
            $low = @($samples | Where-Object { $_.C -eq 1 })
            $high = @($samples | Where-Object { $_.C -eq 0 -and $low.Count -gt 0 -and $_.X -gt $low[-1].X + 900 })
            if ($low.Count -lt 3 -or $low[-1].X - $low[0].X -lt 300 -or $high.Count -lt 2) {
                throw "$role missed replicated crouch travel and standing run return"
            }
        }
        Write-Output "PASS: server enforced 2 m/s crouch then 7 m/s run; both clients observed stance and travel. Logs: $logs"
    }
    if ($BackpedalScenario) {
        Wait-Log (Join-Path $logs 'Client1.log') 'BACKPEDAL_DRIVER_COMPLETE'
        $serverSamples = @(Select-String -LiteralPath $serverLog -Pattern 'BACKPEDAL_SERVER_SAMPLE Pawn=(\S+) T=([\d.]+) X=([-\d.]+) Speed=([\d.]+) Dot=([-\d.]+) Cap=([\d.]+)' | ForEach-Object {
            $g = $_.Matches[0].Groups
            [pscustomobject]@{ Pawn=$g[1].Value; T=[double]::Parse($g[2].Value,[Globalization.CultureInfo]::InvariantCulture);
                X=[double]::Parse($g[3].Value,[Globalization.CultureInfo]::InvariantCulture);
                Speed=[double]::Parse($g[4].Value,[Globalization.CultureInfo]::InvariantCulture);
                Dot=[double]::Parse($g[5].Value,[Globalization.CultureInfo]::InvariantCulture);
                Cap=[double]::Parse($g[6].Value,[Globalization.CultureInfo]::InvariantCulture) }
        })
        $reverse = @($serverSamples | Where-Object { $_.Speed -ge 390 -and $_.Speed -le 425 -and $_.Dot -lt -0.9 -and $_.Cap -ge 419 -and $_.Cap -le 421 })
        if ($reverse.Count -lt 3) { throw 'Server did not enforce sustained 0.6x reverse movement' }
        $pawn = $reverse[0].Pawn
        $forwardBefore = @($serverSamples | Where-Object { $_.Pawn -eq $pawn -and $_.T -lt $reverse[0].T -and $_.Speed -ge 665 -and $_.Speed -le 705 -and $_.Dot -gt 0.9 })
        $forwardAfter = @($serverSamples | Where-Object { $_.Pawn -eq $pawn -and $_.T -gt $reverse[-1].T -and $_.Speed -ge 665 -and $_.Speed -le 705 -and $_.Dot -gt 0.9 })
        if ($forwardBefore.Count -lt 3 -or $forwardAfter.Count -lt 3 -or
            $forwardBefore[-1].X - $forwardBefore[0].X -lt 500 -or
            $reverse[0].X - $reverse[-1].X -lt 400 -or
            $forwardAfter[-1].X - $forwardAfter[0].X -lt 500) {
            throw 'Server missed ordered forward, reverse, forward travel'
        }
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            $samples = @(Select-String -LiteralPath $path -Pattern "BACKPEDAL_CLIENT_SAMPLE Role=$role Stage=(\d+) X=([-\d.]+) Speed=([\d.]+) Facing=([-\d.]+)" | ForEach-Object {
                $g = $_.Matches[0].Groups
                [pscustomobject]@{ Stage=[int]$g[1].Value; X=[double]::Parse($g[2].Value,[Globalization.CultureInfo]::InvariantCulture);
                    Speed=[double]::Parse($g[3].Value,[Globalization.CultureInfo]::InvariantCulture);
                    Facing=[double]::Parse($g[4].Value,[Globalization.CultureInfo]::InvariantCulture) }
            })
            $a = @($samples | Where-Object { $_.Stage -eq 0 -and $_.Speed -ge 600 -and $_.Facing -gt 0.9 })
            $b = @($samples | Where-Object { $_.Stage -eq 1 -and $_.Speed -ge 350 -and $_.Speed -le 450 -and $_.Facing -gt 0.9 })
            $c = @($samples | Where-Object { $_.Stage -eq 2 -and $_.Speed -ge 600 -and $_.Facing -gt 0.9 })
            if ($a.Count -lt 2 -or $b.Count -lt 2 -or $c.Count -lt 2 -or $b[0].X - $b[-1].X -lt 250) {
                throw "$role missed ordered forward, slower reverse, forward observation"
            }
        }
        Write-Output "PASS: server capped reverse travel at 0.6x run; driver and observer saw ordered forward, reverse, forward movement. Logs: $logs"
    }
    if ($SwimScenario) {
        Wait-Log $serverLog 'SWIM_SERVER_MODE Swimming=1'
        Wait-Log $serverLog 'SWIM_SERVER_MODE Swimming=0'
        Wait-Log (Join-Path $logs 'Client1.log') 'SWIM_DRIVER_COMPLETE'
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            Wait-Log $path "SWIM_SAMPLE Role=$role .*Mode=4"
            $samples = @(Select-String -LiteralPath $path -Pattern "SWIM_SAMPLE Role=$role Stage=\d+ X=([-\d.]+) Mode=(\d+) Speed=([\d.]+)" | ForEach-Object {
                $g = $_.Matches[0].Groups
                [pscustomobject]@{ X=[double]::Parse($g[1].Value,[Globalization.CultureInfo]::InvariantCulture); Mode=[int]$g[2].Value;
                    Speed=[double]::Parse($g[3].Value,[Globalization.CultureInfo]::InvariantCulture) }
            })
            $walk = @($samples | Where-Object { $_.Mode -eq 1 -and $_.X -lt 500 } | Select-Object -First 1)
            $swim = @($samples | Where-Object { $_.Mode -eq 4 -and $_.X -gt 1600 -and $_.X -lt 2200 -and $_.Speed -ge 290 -and $_.Speed -le 310 } | Select-Object -First 1)
            $returned = @($samples | Where-Object { $_.Mode -eq 1 -and $_.X -gt 500 -and $_.X -lt 1000 } | Select-Object -Last 1)
            if ($walk.Count -ne 1 -or $swim.Count -ne 1 -or $returned.Count -ne 1) {
                throw "$role missed walk-to-swim-to-walk, or the 3 m/s swim cap"
            }
        }
        Write-Output "PASS: server switched at authored deep water; both clients saw walking, capped swimming, and walking after exit. Logs: $logs"
    }
    if ($GreenlandsEnvironmentNetcheck) {
        Wait-Log $serverLog 'GREENLANDS_ENV_DROP X='
        Wait-Log $serverLog 'FALL_DAMAGE ImpactMps='
        Wait-Log $serverLog 'SWIM_SERVER_MODE Swimming=1'
        Wait-Log $serverLog 'SWIM_SERVER_MODE Swimming=0'
        Wait-Log (Join-Path $logs 'Client1.log') 'GREENLANDS_ENV_COMPLETE'
        $hit = Select-String -LiteralPath $serverLog -Pattern 'FALL_DAMAGE ImpactMps=([\d.]+) Damage=(\d+) HealthBefore=(\d+) Pawn=(\S+)' | Select-Object -First 1
        $g = $hit.Matches[0].Groups
        $impact = [double]::Parse($g[1].Value,[Globalization.CultureInfo]::InvariantCulture)
        $damage = [int]$g[2].Value
        $before = [int]$g[3].Value
        $expected = [int][math]::Floor($before * [math]::Min(1.2,[math]::Max(0.0,($impact - 12.0) / 20.0)) + 0.5)
        if ($impact -le 12 -or $damage -ne $expected -or $damage -ge $before) {
            throw "Greenlands fall calculation invalid: impact=$impact damage=$damage expected=$expected before=$before"
        }
        Wait-Log $serverLog "COMBAT_PILOT_PLAYER_HIT HP=$($before-$damage) By=None"
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            $samples = @(Select-String -LiteralPath $path -Pattern "GREENLANDS_ENV_SAMPLE Role=$role HP=(\d+) X=([-\d.]+) Y=([-\d.]+) Z=([-\d.]+) Mode=(\d+) Speed=([\d.]+)" | ForEach-Object {
                $m = $_.Matches[0].Groups
                [pscustomobject]@{ HP=[int]$m[1].Value; X=[double]::Parse($m[2].Value,[Globalization.CultureInfo]::InvariantCulture);
                    Z=[double]::Parse($m[4].Value,[Globalization.CultureInfo]::InvariantCulture); Mode=[int]$m[5].Value;
                    Speed=[double]::Parse($m[6].Value,[Globalization.CultureInfo]::InvariantCulture) }
            })
            $raised = @($samples | Where-Object { $_.HP -eq $before -and $_.X -gt 50000 -and $_.X -lt 52000 -and $_.Z -gt 3500 })
            $landed = @($samples | Where-Object { $_.HP -eq ($before-$damage) -and $_.X -gt 50000 -and $_.X -lt 53000 -and $_.Z -lt 2700 -and $_.Mode -eq 1 })
            $swimming = @($samples | Where-Object { $_.Mode -eq 4 -and $_.X -gt 54500 -and $_.X -lt 56500 -and $_.Speed -ge 285 -and $_.Speed -le 315 })
            $returned = @($samples | Where-Object { $_.Mode -eq 1 -and $_.X -le 51500 } | Select-Object -Last 1)
            if ($raised.Count -eq 0 -or $landed.Count -eq 0 -or $swimming.Count -eq 0 -or $returned.Count -eq 0) {
                throw "$role missed the fall, replicated damage, 3.0 m/s swimming or dry-land return"
            }
        }
        Write-Output "PASS: Silverbrook fall $impact m/s caused expected $damage damage; both clients saw 3.0 m/s swimming in Eastwood Pond and dry-land return. Logs: $logs"
    }
    if ($AssistScenario) {
        $driverLog = Join-Path $logs 'Client1.log'
        $observerLog = Join-Path $logs 'Client2.log'
        Wait-Log $serverLog 'BOAR_PACK_ASSIST_TIMING Group=assist_sandbox'
        Wait-Log $driverLog 'BOAR_ASSIST_PACK_JOINED Role=Driver'
        Wait-Log $observerLog 'BOAR_ASSIST_PACK_JOINED Role=Observer'
        $timing = Select-String -LiteralPath $serverLog -Pattern 'BOAR_PACK_ASSIST_TIMING Group=assist_sandbox Elapsed=([\d.]+) DistanceCm=([\d.]+)' | Select-Object -First 1
        $elapsed = [double]::Parse($timing.Matches[0].Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture)
        $distance = [double]::Parse($timing.Matches[0].Groups[2].Value,[Globalization.CultureInfo]::InvariantCulture)
        if ($elapsed -lt 2.0 -or $distance -gt 800) { throw "Assist timing or distance violated: $elapsed seconds, $distance cm" }
        if (@(Select-String -LiteralPath $driverLog,$observerLog -Pattern 'BOAR_ASSIST_EARLY_TARGET').Count) { throw 'Defensive boar engaged before lead' }
        foreach ($role in @('Driver','Observer')) {
            $path = $(if ($role -eq 'Driver') { $driverLog } else { $observerLog })
            $lead = Select-String -LiteralPath $path -Pattern "BOAR_ASSIST_LEAD_ENGAGED Role=$role" | Select-Object -First 1
            $joined = Select-String -LiteralPath $path -Pattern "BOAR_ASSIST_PACK_JOINED Role=$role" | Select-Object -First 1
            if (-not $lead -or -not $joined -or $lead.LineNumber -ge $joined.LineNumber) { throw "$role missed ordered lead and pack engagement" }
            if (@(Select-String -LiteralPath $path -Pattern "BOAR_ASSIST_SAMPLE Role=$role .*Lead=0 Mate=1").Count) {
                throw "$role saw the defensive packmate engage before the lead"
            }
        }
        Write-Output "PASS: server linked idle defensive packmate after $elapsed s within $distance cm; both clients observed lead then shared-target engagement. Logs: $logs"
    }
    if ($RespawnScenario) {
        Wait-Log $serverLog 'BOAR_RESPAWN_ACTIVE Cycle=1 Point=1'
        $dead = Select-String -LiteralPath $serverLog -Pattern 'BOAR_RESPAWN_DEAD Cycle=1 Players=(\d+) Delay=([\d.]+) Due=([\d.]+)' | Select-Object -First 1
        $active = Select-String -LiteralPath $serverLog -Pattern 'BOAR_RESPAWN_ACTIVE Cycle=1 Point=1 T=([\d.]+) HP=60 X=2600 Y=0' | Select-Object -First 1
        $hit = Select-String -LiteralPath $serverLog -Pattern 'COMBAT_PILOT_BOAR_HIT Group=respawn_sandbox Slot=0 HP=0 Attacker=\S+' | Select-Object -First 1
        if (-not $dead -or -not $active -or -not $hit -or $hit.LineNumber -ge $dead.LineNumber -or $dead.LineNumber -ge $active.LineNumber) {
            throw 'Server missed ordered client kill, timed slot and fresh spawn'
        }
        $number = [Globalization.CultureInfo]::InvariantCulture
        $delay = [double]::Parse($dead.Matches[0].Groups[2].Value,$number)
        $due = [double]::Parse($dead.Matches[0].Groups[3].Value,$number)
        $at = [double]::Parse($active.Matches[0].Groups[1].Value,$number)
        if ([int]$dead.Matches[0].Groups[1].Value -ne 2 -or $delay -lt (300/1.15*0.9) - 0.01 -or
            $delay -gt (300/1.15*1.1) + 0.01 -or $at -lt $due - 0.01 -or $at -gt $due + 5) {
            throw "Server timer violated authored 2-player range or due time: players=$($dead.Matches[0].Groups[1].Value) delay=$delay due=$due active=$at"
        }
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            Wait-Log $path "BOAR_RESPAWN_RETURNED Role=$role"
            $initial = Select-String -LiteralPath $path -Pattern "BOAR_RESPAWN_INITIAL Role=$role HP=60 X=1800" | Select-Object -First 1
            $absent = Select-String -LiteralPath $path -Pattern "BOAR_RESPAWN_ABSENT Role=$role T=([\d.]+)" | Select-Object -First 1
            $returned = Select-String -LiteralPath $path -Pattern "BOAR_RESPAWN_RETURNED Role=$role T=([\d.]+) Elapsed=([\d.]+) HP=60 X=2600 Y=0" | Select-Object -First 1
            if (-not $initial -or -not $absent -or -not $returned -or
                $initial.LineNumber -ge $absent.LineNumber -or $absent.LineNumber -ge $returned.LineNumber) {
                throw "$role missed ordered initial, death absence and fresh alternate-point creature"
            }
            $elapsed = [double]::Parse($returned.Matches[0].Groups[2].Value,$number)
            if ($elapsed -lt $delay - 2 -or $elapsed -gt $delay + 10) { throw "$role observed return outside timer: $elapsed s vs $delay s" }
        }
        Write-Output "PASS: two clients observed client-caused boar death, $([math]::Round($delay,1)) s authoritative timer and fresh full-health boar at alternate point. Logs: $logs"
    }
    if ($GreenlandsBoarNetcheck) {
        Wait-Log $serverLog 'BOAR_GREENLANDS_RESPAWN LeadSlot='
        $serverText = Get-Content -LiteralPath $serverLog -Raw
        $ordered = @('BOAR_GREENLANDS_BEGIN', 'BOAR_GREENLANDS_AGGRO',
            'BOAR_GREENLANDS_SWITCH_ASSIST', 'BOAR_GREENLANDS_LEASH_PULL', 'BOAR_GREENLANDS_EVADE',
            'BOAR_GREENLANDS_RESET_REGEN_START', 'BOAR_GREENLANDS_REGEN', 'BOAR_GREENLANDS_MEMORY',
            'BOAR_GREENLANDS_KILL', 'BOAR_GREENLANDS_RESPAWN')
        $lastLine = 0
        foreach ($marker in $ordered) {
            $found = Select-String -LiteralPath $serverLog -Pattern $marker | Select-Object -First 1
            if (-not $found -or $found.LineNumber -le $lastLine) { throw "Server missed ordered $marker" }
            $lastLine = $found.LineNumber
        }
        foreach ($pattern in @('BOAR_AGGRO_DETECTED Group=boar_farm_edge',
            'BOAR_THREAT_TARGET Group=boar_farm_edge', 'BOAR_PACK_ASSIST_TIMING Group=boar_farm_edge',
            'BOAR_EVADE_RESET Group=boar_farm_edge', 'BOAR_MEMORY_REACQUIRED Group=boar_farm_edge',
            'COMBAT_PILOT_BOAR_HIT Group=boar_farm_edge Slot=\d+ HP=0',
            'COMBAT_PILOT_BOAR_SPAWN Group=boar_farm_edge Slot=\d+ Cycle=1')) {
            if ($serverText -notmatch $pattern) { throw "Server missed $pattern" }
        }
        $leadSlot = [regex]::Match($serverText, 'BOAR_GREENLANDS_BEGIN LeadSlot=(\d+)').Groups[1].Value
        $firstPoint = [regex]::Match($serverText, "COMBAT_PILOT_BOAR_SPAWN Group=boar_farm_edge Slot=$leadSlot Cycle=0 Point=(\S+)").Groups[1].Value
        $nextPoint = [regex]::Match($serverText, "COMBAT_PILOT_BOAR_SPAWN Group=boar_farm_edge Slot=$leadSlot Cycle=1 Point=(\S+)").Groups[1].Value
        if (-not $firstPoint -or -not $nextPoint -or $firstPoint -eq $nextPoint) {
            throw "World slot $leadSlot did not rotate to a fresh respawn point"
        }
        $leashDistance = [regex]::Match($serverText, 'BOAR_GREENLANDS_LEASH_PULL DistanceCm=(\d+)').Groups[1].Value
        if (-not $leashDistance -or [int]$leashDistance -lt 6000) { throw 'Leash probe did not cross 60 m' }
        if ($serverText -match 'COMBAT_PILOT_SLOT_ERROR|Fatal error:|Assertion failed:') { throw 'Server logged a slot or fatal error' }
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            Wait-Log $path "BOAR_GREENLANDS_SAMPLE Role=$role Stage=11 .*LeadHP=60"
            $s = Get-Content -LiteralPath $path -Raw
            $driverTarget = if ($role -eq 'Driver') { 'Self' } else { 'Other' }
            $observerTarget = if ($role -eq 'Observer') { 'Self' } else { 'Other' }
            foreach ($pattern in @(
                "BOAR_GREENLANDS_SAMPLE Role=$role Stage=1 .*LeadHP=60 .*LeadTarget=$driverTarget .*MateTarget=None",
                "BOAR_GREENLANDS_SAMPLE Role=$role Stage=3 .*LeadHP=40 .*LeadTarget=$observerTarget .*MateHP=59 .*MateTarget=$observerTarget",
                "BOAR_GREENLANDS_SAMPLE Role=$role Stage=4 .*LeadReturn=1 .*MateReturn=1",
                "BOAR_GREENLANDS_SAMPLE Role=$role Stage=6 .*LeadHP=60 .*MateHP=60",
                "BOAR_GREENLANDS_SAMPLE Role=$role Stage=7 .*MateTarget=$observerTarget",
                "BOAR_GREENLANDS_SAMPLE Role=$role Stage=10 .*LeadHP=-1",
                "BOAR_GREENLANDS_SAMPLE Role=$role Stage=11 .*LeadHP=60")) {
                if ($s -notmatch $pattern) { throw "$role missed replicated sequence: $pattern" }
            }
            $regen = @([regex]::Matches($s,"BOAR_GREENLANDS_SAMPLE Role=$role Stage=6 .*ObserverHP=(\d+) ObserverCombat=([01])") |
                ForEach-Object { [pscustomobject]@{ HP=[int]$_.Groups[1].Value; Combat=[int]$_.Groups[2].Value } })
            if ($regen.Count -lt 3 -or @($regen | Where-Object { $_.HP -le 60 -and $_.Combat -eq 1 }).Count -eq 0 -or
                @($regen | Where-Object { $_.HP -gt 60 -and $_.Combat -eq 0 }).Count -eq 0) {
                throw "$role missed replicated combat exit and delayed observer healing"
            }
        }
        Write-Output "PASS: Greenlands aggro, threat switch, pack assist, leash/evade, attacker memory, regeneration, client kill and rotated slot respawn on both clients. Logs: $logs"
    }
    Write-Output "PASS: separate editor server and $Clients client(s) connected; $Clients server-authoritative possession(s). Logs: $logs"
    if (-not $CastingScenario -and -not $EvadeScenario -and -not $ThreatScenario -and -not $AggroScenario -and -not $MemoryScenario -and -not $RegenScenario -and -not $FallScenario -and -not $SwimScenario -and -not $CrouchScenario -and -not $BackpedalScenario -and -not $AssistScenario -and -not $RespawnScenario -and -not $GreenlandsBoarNetcheck -and -not $GreenlandsEnvironmentNetcheck) { Write-Output 'Headless connection check only: movement, rendering, lifecycle cleanup, authentication and Linux runtime are not certified.' }
} finally {
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
    }
}
