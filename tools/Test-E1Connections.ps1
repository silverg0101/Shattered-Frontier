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
    [switch]$MemoryScenario
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
    $deadline = (Get-Date).AddSeconds($(if ($EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario) { 240 } else { 120 }))
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
    if (Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue) { throw "UDP $Port is already in use" }
    $serverLog = Join-Path $logs 'Server.log'
    $serverArgs = @($Map, '-server', '-NullRHI', '-unattended', '-nosound', '-nop4', "-port=$Port", '-MULTIHOME=127.0.0.1', ('-abslog="' + $serverLog + '"'))
    if ($EvadeScenario) { $serverArgs += '-CombatEvadeScenario' }
    if ($ThreatScenario) { $serverArgs += '-CombatThreatScenario' }
    if ($AggroScenario) { $serverArgs += '-CombatAggroScenario' }
    if ($MemoryScenario) { $serverArgs += '-CombatMemoryScenario' }
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
    Write-Output "PASS: separate editor server and $Clients client(s) connected; $Clients server-authoritative possession(s). Logs: $logs"
    if (-not $CastingScenario -and -not $EvadeScenario -and -not $ThreatScenario -and -not $AggroScenario -and -not $MemoryScenario) { Write-Output 'Headless connection check only: movement, rendering, lifecycle cleanup, authentication and Linux runtime are not certified.' }
} finally {
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
    }
}
