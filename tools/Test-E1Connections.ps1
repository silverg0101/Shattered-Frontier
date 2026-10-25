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
    [switch]$RestRegenScenario,
    [switch]$FallScenario,
    [switch]$SwimScenario,
    [switch]$CrouchScenario,
    [switch]$BackpedalScenario,
    [switch]$RoadScenario,
    [switch]$SnowScenario,
    [switch]$TravelMountScenario,
    [switch]$GreenlandsEnvironmentNetcheck,
    [switch]$GreenlandsMovementNetcheck,
    [switch]$AssistScenario,
    [switch]$RespawnScenario,
    [switch]$AutoAttackScenario,
    [switch]$UnstuckScenario,
    [switch]$SpiritReleaseScenario,
    [switch]$ResurrectionAcceptScenario,
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
    $deadline = (Get-Date).AddSeconds($(if ($RespawnScenario -or $GreenlandsBoarNetcheck) { 420 } elseif ($EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or $RegenScenario -or $RestRegenScenario -or $AssistScenario) { 240 } else { 120 }))
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
    if ($TravelMountScenario) {
        $otherScenarios = @($PSBoundParameters.Keys | Where-Object {
            $_ -notin @('EngineRoot', 'Port', 'Map', 'Clients', 'TravelMountScenario')
        })
        if ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_TravelMount' -or $Clients -ne 2 -or $otherScenarios.Count) {
            throw 'TravelMountScenario requires SB_TravelMount, two clients, and no other scenario'
        }
    }
    if ($SpiritReleaseScenario) {
        $otherScenarios = @($PSBoundParameters.Keys | Where-Object {
            $_ -notin @('EngineRoot', 'Port', 'Map', 'Clients', 'SpiritReleaseScenario')
        })
        if ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_SpiritRelease' -or $Clients -ne 2 -or $otherScenarios.Count) {
            throw 'SpiritReleaseScenario requires SB_SpiritRelease, two clients, and no other scenario'
        }
    }
    if ($ResurrectionAcceptScenario) {
        $otherScenarios = @($PSBoundParameters.Keys | Where-Object {
            $_ -notin @('EngineRoot', 'Port', 'Map', 'Clients', 'ResurrectionAcceptScenario')
        })
        if ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_ResurrectionAccept' -or $Clients -ne 2 -or $otherScenarios.Count) {
            throw 'ResurrectionAcceptScenario requires SB_ResurrectionAccept, two clients, and no other scenario'
        }
    }
    if ($UnstuckScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_Unstuck' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or
        $RegenScenario -or $RestRegenScenario -or $FallScenario -or $SwimScenario -or $CrouchScenario -or
        $BackpedalScenario -or $RoadScenario -or $SnowScenario -or $AssistScenario -or $RespawnScenario -or
        $AutoAttackScenario -or $GreenlandsBoarNetcheck -or $GreenlandsEnvironmentNetcheck -or $GreenlandsMovementNetcheck)) {
        throw 'UnstuckScenario requires SB_Unstuck, two clients, and no other scenario'
    }
    if ($AutoAttackScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_AutoAttack' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or
        $RegenScenario -or $RestRegenScenario -or $FallScenario -or $SwimScenario -or $CrouchScenario -or
        $BackpedalScenario -or $RoadScenario -or $SnowScenario -or $AssistScenario -or $RespawnScenario -or
        $GreenlandsBoarNetcheck -or $GreenlandsEnvironmentNetcheck -or $GreenlandsMovementNetcheck)) {
        throw 'AutoAttackScenario requires SB_AutoAttack, two clients, and no other scenario'
    }
    if ($SnowScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_DeepSnow' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or
        $RegenScenario -or $RestRegenScenario -or $FallScenario -or $SwimScenario -or $CrouchScenario -or
        $BackpedalScenario -or $RoadScenario -or $AssistScenario -or $RespawnScenario -or
        $GreenlandsBoarNetcheck -or $GreenlandsEnvironmentNetcheck -or $GreenlandsMovementNetcheck)) {
        throw 'SnowScenario requires SB_DeepSnow, two clients, and no other scenario'
    }
    if ($RestRegenScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_RestRegen' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or
        $RegenScenario -or $FallScenario -or $SwimScenario -or $CrouchScenario -or $BackpedalScenario -or
        $RoadScenario -or $AssistScenario -or $RespawnScenario -or $GreenlandsBoarNetcheck -or
        $GreenlandsEnvironmentNetcheck -or $GreenlandsMovementNetcheck)) {
        throw 'RestRegenScenario requires SB_RestRegen, two clients, and no other scenario'
    }
    if ($RoadScenario -and ($Map -ne '/Game/Frontier/Maps/Sandbox/SB_RoadSpeed' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or
        $RegenScenario -or $FallScenario -or $SwimScenario -or $CrouchScenario -or $BackpedalScenario -or
        $AssistScenario -or $RespawnScenario -or $GreenlandsBoarNetcheck -or
        $GreenlandsEnvironmentNetcheck -or $GreenlandsMovementNetcheck)) {
        throw 'RoadScenario requires SB_RoadSpeed, two clients, and no other scenario'
    }
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
            $RegenScenario -or $FallScenario -or $SwimScenario -or $AssistScenario -or $RespawnScenario -or $GreenlandsBoarNetcheck -or $GreenlandsMovementNetcheck) {
            throw 'GreenlandsEnvironmentNetcheck requires Greenlands_Silverbrook, two clients, and no other scenario'
        }
    }
    if ($GreenlandsMovementNetcheck -and ($Map -ne '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook' -or $Clients -ne 2 -or
        $CastingScenario -or $EvadeScenario -or $ThreatScenario -or $AggroScenario -or $MemoryScenario -or
        $RegenScenario -or $FallScenario -or $SwimScenario -or $CrouchScenario -or $BackpedalScenario -or
        $AssistScenario -or $RespawnScenario -or $GreenlandsBoarNetcheck -or $GreenlandsEnvironmentNetcheck)) {
        throw 'GreenlandsMovementNetcheck requires Greenlands_Silverbrook, two clients, and no other scenario'
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
    if ($RestRegenScenario) { $serverArgs += '-RestRegenScenario' }
    if ($FallScenario) { $serverArgs += '-CombatFallScenario' }
    if ($AssistScenario) { $serverArgs += '-CombatAssistScenario' }
    if ($RespawnScenario) { $serverArgs += '-CombatRespawnScenario' }
    if ($AutoAttackScenario) { $serverArgs += '-AutoAttackScenario' }
    if ($UnstuckScenario) { $serverArgs += '-UnstuckScenario' }
    if ($SpiritReleaseScenario) { $serverArgs += '-SpiritReleaseScenario' }
    if ($ResurrectionAcceptScenario) { $serverArgs += '-ResurrectionAcceptScenario' }
    if ($GreenlandsBoarNetcheck) { $serverArgs += '-GreenlandsBoarNetcheck' }
    if ($GreenlandsEnvironmentNetcheck) { $serverArgs += '-GreenlandsEnvironmentNetcheck' }
    if ($GreenlandsMovementNetcheck) { $serverArgs += '-GreenlandsMovementNetcheck' }
    if ($CrouchScenario) { $serverArgs += '-CrouchScenario' }
    if ($BackpedalScenario) { $serverArgs += '-BackpedalScenario' }
    if ($RoadScenario) { $serverArgs += '-RoadScenario' }
    if ($SnowScenario) { $serverArgs += '-SnowScenario' }
    if ($TravelMountScenario) { $serverArgs += '-TravelMountScenario' }
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
        if ($RestRegenScenario) { $clientArgs += @('-RestRegenScenario', ('-RestRegenRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($FallScenario) { $clientArgs += @('-CombatFallScenario', ('-CombatFallRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($SwimScenario) { $clientArgs += @('-SwimScenario', ('-SwimRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($CrouchScenario) { $clientArgs += @('-CrouchScenario', ('-CrouchRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($BackpedalScenario) { $clientArgs += @('-BackpedalScenario', ('-BackpedalRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($RoadScenario) { $clientArgs += @('-RoadScenario', ('-RoadRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($SnowScenario) { $clientArgs += @('-SnowScenario', ('-SnowRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($TravelMountScenario) { $clientArgs += @('-TravelMountScenario', ('-TravelMountRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($AssistScenario) { $clientArgs += @('-CombatAssistScenario', ('-CombatAssistRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($RespawnScenario) { $clientArgs += @('-CombatRespawnScenario', ('-CombatRespawnRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($AutoAttackScenario) { $clientArgs += @('-AutoAttackScenario', ('-AutoAttackRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($UnstuckScenario) { $clientArgs += @('-UnstuckScenario', ('-UnstuckRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($SpiritReleaseScenario) { $clientArgs += @('-SpiritReleaseScenario', ('-SpiritReleaseRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($ResurrectionAcceptScenario) { $clientArgs += @('-ResurrectionAcceptScenario', ('-ResurrectionAcceptRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($GreenlandsBoarNetcheck) { $clientArgs += @('-GreenlandsBoarNetcheck', ('-GreenlandsBoarRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($GreenlandsEnvironmentNetcheck) { $clientArgs += @('-GreenlandsEnvironmentNetcheck', ('-GreenlandsEnvironmentRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
        if ($GreenlandsMovementNetcheck) { $clientArgs += @('-GreenlandsMovementNetcheck', ('-GreenlandsMovementRole=' + $(if ($index -eq 1) { 'Driver' } else { 'Observer' }))) }
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
    if ($UnstuckScenario) {
        Wait-Log (Join-Path $logs 'Client1.log') 'UNSTUCK_CLIENT_MOVED Role=Driver Distance='
        Wait-Log (Join-Path $logs 'Client2.log') 'UNSTUCK_CLIENT_MOVED Role=Observer Distance='
        Wait-Log $serverLog 'UNSTUCK_SERVER_DENIED Reason=StateOrCooldown'
        $accepted = @(Select-String -LiteralPath $serverLog -Pattern 'UNSTUCK_SERVER_ACCEPTED .*Distance=(\d+)')
        $denied = @(Select-String -LiteralPath $serverLog -Pattern 'UNSTUCK_SERVER_DENIED Reason=StateOrCooldown')
        if ($accepted.Count -ne 1 -or $denied.Count -ne 1 -or
            [int]$accepted[0].Matches[0].Groups[1].Value -lt 200 -or
            [int]$accepted[0].Matches[0].Groups[1].Value -gt 3000 -or
            $accepted[0].LineNumber -ge $denied[0].LineNumber) {
            throw 'Unstuck server did not accept one move then deny the cooldown replay'
        }
        Write-Output "PASS: client requested unstuck, authority moved it, both clients observed the move, and replay was denied. Logs: $logs"
    }
    if ($ResurrectionAcceptScenario) {
        $driverLog = Join-Path $logs 'Client1.log'
        $observerLog = Join-Path $logs 'Client2.log'
        Wait-Log $driverLog 'RES_ACCEPT_CLIENT_RESTORED Role=Driver HP=28'
        Wait-Log $observerLog 'RES_ACCEPT_CLIENT_RESTORED Role=Observer HP=28'
        Wait-Log $serverLog 'RES_ACCEPT_SERVER_DENIED' 2
        $offer = Select-String -LiteralPath $serverLog -Pattern 'RES_ACCEPT_SERVER_OFFER Pawn=.* Offer=1 HP=0' | Select-Object -First 1
        $killingHit = Select-String -LiteralPath $serverLog -Pattern 'COMBAT_PILOT_PLAYER_HIT HP=0 By=SFPilotBoar' | Select-Object -First 1
        $accepted = Select-String -LiteralPath $serverLog -Pattern 'RES_ACCEPT_SERVER_ACCEPTED Pawn=.* Offer=1 HP=28' | Select-Object -First 1
        $denied = @(Select-String -LiteralPath $serverLog -Pattern 'RES_ACCEPT_SERVER_DENIED Pawn=')
        if (-not $offer -or -not $killingHit -or -not $accepted -or $denied.Count -ne 2 -or
            $denied[0].LineNumber -ge $killingHit.LineNumber -or $killingHit.LineNumber -ge $offer.LineNumber -or
            $offer.LineNumber -ge $accepted.LineNumber -or
            $accepted.LineNumber -ge $denied[1].LineNumber) {
            throw 'Server missed ordered alive denial, death offer, acceptance and replay denial'
        }
        foreach ($role in @('Driver', 'Observer')) {
            $path = if ($role -eq 'Driver') { $driverLog } else { $observerLog }
            $initial = Select-String -LiteralPath $path -Pattern "RES_ACCEPT_CLIENT_INITIAL Role=$role HP=35" | Select-Object -First 1
            $dead = Select-String -LiteralPath $path -Pattern "RES_ACCEPT_CLIENT_DEAD Role=$role HP=0" | Select-Object -First 1
            $restored = Select-String -LiteralPath $path -Pattern "RES_ACCEPT_CLIENT_RESTORED Role=$role HP=28" | Select-Object -First 1
            if (-not $initial -or -not $dead -or -not $restored -or
                $initial.LineNumber -ge $dead.LineNumber -or $dead.LineNumber -ge $restored.LineNumber) {
                throw "$role missed the replicated 35 to 0 to 28 death and acceptance sequence"
            }
        }
        if ((Get-Content -LiteralPath $serverLog -Raw) -match 'COMBAT_PILOT_PLAYER_RESPAWN') {
            throw 'Automatic respawn raced the accepted resurrection'
        }
        Write-Output "PASS: boar killed driver; dead owner accepted the 60-second offer; both clients saw 35 to 0 to 28 HP; replay denied. Logs: $logs"
    }
    if ($SpiritReleaseScenario) {
        $driverLog = Join-Path $logs 'Client1.log'
        $observerLog = Join-Path $logs 'Client2.log'
        Wait-Log $driverLog 'SPIRIT_RELEASE_CLIENT_RESTORED Role=Driver HP=40 X=3500'
        Wait-Log $observerLog 'SPIRIT_RELEASE_CLIENT_RESTORED Role=Observer HP=40 X=3500'
        Wait-Log $serverLog 'SPIRIT_RELEASE_SERVER_ACCEPTED .*HP=40 X=3500'
        Wait-Log $serverLog 'SPIRIT_RELEASE_SERVER_DENIED' 2
        $dead = Select-String -LiteralPath $serverLog -Pattern 'SPIRIT_RELEASE_SERVER_DEAD Pawn=' | Select-Object -First 1
        $accepted = Select-String -LiteralPath $serverLog -Pattern 'SPIRIT_RELEASE_SERVER_ACCEPTED .*HP=40 X=3500' | Select-Object -First 1
        $denied = @(Select-String -LiteralPath $serverLog -Pattern 'SPIRIT_RELEASE_SERVER_DENIED Pawn=')
        if (-not $dead -or -not $accepted -or $denied.Count -ne 2 -or
            $denied[0].LineNumber -ge $dead.LineNumber -or $dead.LineNumber -ge $accepted.LineNumber -or
            $accepted.LineNumber -ge $denied[1].LineNumber) {
            throw 'Server missed ordered alive denial, boar-caused death, release and repeated denial'
        }
        foreach ($role in @('Driver', 'Observer')) {
            $path = if ($role -eq 'Driver') { $driverLog } else { $observerLog }
            $initial = Select-String -LiteralPath $path -Pattern "SPIRIT_RELEASE_CLIENT_INITIAL Role=$role HP=35" | Select-Object -First 1
            $fallen = Select-String -LiteralPath $path -Pattern "SPIRIT_RELEASE_CLIENT_DEAD Role=$role HP=0" | Select-Object -First 1
            $restored = Select-String -LiteralPath $path -Pattern "SPIRIT_RELEASE_CLIENT_RESTORED Role=$role HP=40 X=3500" | Select-Object -First 1
            if (-not $initial -or -not $fallen -or -not $restored -or
                $initial.LineNumber -ge $fallen.LineNumber -or $fallen.LineNumber -ge $restored.LineNumber) {
                throw "$role missed the replicated 35 to 0 to 40 death/release sequence"
            }
        }
        Write-Output "PASS: boar killed the driver; owning client released at the authored point with 50% max health; both clients saw 35 to 0 to 40, and authority denied alive/repeated requests. Logs: $logs"
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
    if ($RestRegenScenario) {
        Wait-Log $serverLog 'COMBAT_PILOT_PLAYER_HIT HP=([1-9][0-9]*) By=SFPilotBoar'
        Wait-Log $serverLog 'REGEN_COMBAT_EXIT Health='
        Wait-Log $serverLog 'REST_REGEN_TICK Health=\d+ Rest=0'
        Wait-Log $serverLog 'REST_REGEN_TICK Health=\d+ Rest=1' 2
        $ticks = @(Select-String -LiteralPath $serverLog -Pattern 'REST_REGEN_TICK Health=(\d+) Rest=([01])' | ForEach-Object {
            [pscustomobject]@{ HP=[int]$_.Matches[0].Groups[1].Value; Rest=[int]$_.Matches[0].Groups[2].Value }
        })
        $ordinary = @($ticks | Where-Object { $_.Rest -eq 0 })
        $rested = @($ticks | Where-Object { $_.Rest -eq 1 })
        if ($ordinary.Count -lt 2 -or $rested.Count -lt 2) { throw 'Server missed ordinary or rested health ticks' }
        $ordinaryGain = $ordinary[1].HP - $ordinary[0].HP
        $restedGain = $rested[1].HP - $rested[0].HP
        if ($ordinaryGain -lt 2 -or $ordinaryGain -gt 3 -or $restedGain -lt 4 -or $restedGain -gt 5) {
            throw "Server regeneration rates differ from Combat values: ordinary=$ordinaryGain rested=$restedGain"
        }
        foreach ($role in @('Driver', 'Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            Wait-Log $path "REST_REGEN_SAMPLE Role=$role.*Rest=1"
            $s = Get-Content -LiteralPath $path -Raw
            if ($s -notmatch "REST_REGEN_SAMPLE Role=$role.*Health=([1-5][0-9]) Combat=1.*Rest=0" -or
                $s -notmatch "REST_REGEN_SAMPLE Role=$role.*Health=([1-7][0-9]) Combat=0.*Rest=0" -or
                $s -notmatch "REST_REGEN_SAMPLE Role=$role.*Health=([1-7][0-9]) Combat=0.*Rest=1") {
                throw "$role missed the replicated damage, combat exit, or rest-area healing sequence"
            }
        }
        Write-Output "PASS: boar damage, combat exit, delayed ordinary healing ($ordinaryGain HP/tick), and rested healing ($restedGain HP/tick) replicated to both clients. Logs: $logs"
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
    if ($TravelMountScenario) {
        Wait-Log (Join-Path $logs 'Client1.log') 'MOUNT_DRIVER_COMPLETE'
        $serverText = Get-Content -LiteralPath $serverLog -Raw
        $cast = [regex]::Match($serverText, 'MOUNT_SERVER_CAST Pawn=(\S+) T=([\d.]+)')
        if (-not $cast.Success) { throw 'Server missed the owned mount cast' }
        $pawn = $cast.Groups[1].Value
        $mounted = [regex]::Match($serverText, "MOUNT_SERVER_MOUNTED Pawn=$pawn T=([\d.]+)")
        if (-not $mounted.Success -or [double]$mounted.Groups[1].Value - [double]$cast.Groups[2].Value -lt 1.5) {
            throw 'Server mounted before the 1.5 s stationary cast completed'
        }
        if ($serverText -notmatch "MOUNT_SERVER_DENIED Pawn=$pawn Casting=1 Mounted=0" -or
            $serverText -notmatch "MOUNT_SERVER_DISMOUNT Pawn=$pawn Reason=Player") {
            throw 'Server missed repeat-cast denial or voluntary dismount'
        }
        $samples = @(Select-String -LiteralPath $serverLog -Pattern "MOUNT_SERVER_SAMPLE Pawn=$pawn T=([\d.]+) X=([-\d.]+) Speed=([\d.]+) Cap=([\d.]+) Casting=([01]) Mounted=([01])" | ForEach-Object {
            $g = $_.Matches[0].Groups
            [pscustomobject]@{ T=[double]$g[1].Value; X=[double]$g[2].Value; Speed=[double]$g[3].Value;
                Cap=[double]$g[4].Value; Casting=[int]$g[5].Value; Mounted=[int]$g[6].Value }
        })
        $casting = @($samples | Where-Object { $_.Casting -eq 1 -and $_.Mounted -eq 0 -and $_.Cap -eq 0 -and $_.Speed -lt 30 })
        $riding = @($samples | Where-Object { $_.Mounted -eq 1 -and $_.Speed -ge 1050 -and $_.Speed -le 1110 -and $_.Cap -eq 1100 })
        $walking = @($samples | Where-Object { $_.Mounted -eq 0 -and $_.T -gt [double]$mounted.Groups[1].Value + 3 -and $_.Speed -ge 650 -and $_.Speed -le 710 -and $_.Cap -eq 700 })
        if ($casting.Count -lt 2 -or $riding.Count -lt 3 -or $walking.Count -lt 2 -or $riding[0].X -ge $walking[0].X) {
            throw 'Server missed stationary cast, tier-1 travel, or return to foot speed'
        }
        foreach ($role in @('Driver', 'Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            $view = @(Select-String -LiteralPath $path -Pattern "MOUNT_CLIENT_SAMPLE Role=$role T=([\d.]+) X=([-\d.]+) Speed=([\d.]+) Casting=([01]) Mounted=([01])" | ForEach-Object {
                $g = $_.Matches[0].Groups
                [pscustomobject]@{ X=[double]$g[2].Value; Speed=[double]$g[3].Value; Casting=[int]$g[4].Value; Mounted=[int]$g[5].Value }
            })
            $castView = @($view | Where-Object { $_.Casting -eq 1 -and $_.Mounted -eq 0 })
            $rideView = @($view | Where-Object { $_.Mounted -eq 1 -and $_.Speed -ge 1000 -and $_.Speed -le 1150 })
            $footView = @($view | Where-Object { $_.Mounted -eq 0 -and $_.X -gt 3000 -and $_.Speed -ge 600 -and $_.Speed -le 750 })
            if ($castView.Count -lt 2 -or $rideView.Count -lt 2 -or $footView.Count -lt 1) {
                throw "$role missed replicated mount cast, mounted travel or dismount"
            }
        }
        Write-Output "PASS: client cast, denial, 11 m/s mount travel and dismount reached server and both clients. Logs: $logs"
    }
    if ($RoadScenario) {
        Wait-Log (Join-Path $logs 'Client1.log') 'ROAD_DRIVER_COMPLETE'
        $serverSamples = @(Select-String -LiteralPath $serverLog -Pattern 'ROAD_SERVER_SAMPLE Pawn=(\S+) X=([-\d.]+) Speed=([\d.]+) Cap=([\d.]+)' | ForEach-Object {
            $g = $_.Matches[0].Groups
            [pscustomobject]@{ Pawn=$g[1].Value; X=[double]::Parse($g[2].Value,[Globalization.CultureInfo]::InvariantCulture);
                Speed=[double]::Parse($g[3].Value,[Globalization.CultureInfo]::InvariantCulture);
                Cap=[double]::Parse($g[4].Value,[Globalization.CultureInfo]::InvariantCulture) }
        })
        $road = @($serverSamples | Where-Object { $_.X -gt 1600 -and $_.X -lt 2400 -and $_.Speed -ge 750 -and $_.Speed -le 780 -and $_.Cap -ge 769 -and $_.Cap -le 771 })
        if ($road.Count -lt 3) { throw 'Server did not enforce the authored 1.10 road speed' }
        $pawn = $road[0].Pawn
        $before = @($serverSamples | Where-Object { $_.Pawn -eq $pawn -and $_.X -gt 300 -and $_.X -lt 750 -and $_.Speed -ge 680 -and $_.Speed -le 705 -and $_.Cap -ge 699 -and $_.Cap -le 701 })
        $after = @($serverSamples | Where-Object { $_.Pawn -eq $pawn -and $_.X -gt 3500 -and $_.X -lt 4300 -and $_.Speed -ge 680 -and $_.Speed -le 705 -and $_.Cap -ge 699 -and $_.Cap -le 701 })
        if ($before.Count -lt 2 -or $after.Count -lt 2) { throw 'Server missed run to road and return to normal ground' }
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            $samples = @(Select-String -LiteralPath $path -Pattern "ROAD_CLIENT_SAMPLE Role=$role X=([-\d.]+) Speed=([\d.]+)" | ForEach-Object {
                $g = $_.Matches[0].Groups
                [pscustomobject]@{ X=[double]::Parse($g[1].Value,[Globalization.CultureInfo]::InvariantCulture);
                    Speed=[double]::Parse($g[2].Value,[Globalization.CultureInfo]::InvariantCulture) }
            })
            $normalIn = @($samples | Where-Object { $_.X -gt 300 -and $_.X -lt 750 -and $_.Speed -ge 650 -and $_.Speed -le 710 })
            $onRoad = @($samples | Where-Object { $_.X -gt 1600 -and $_.X -lt 2400 -and $_.Speed -ge 730 -and $_.Speed -le 790 })
            $normalOut = @($samples | Where-Object { $_.X -gt 3500 -and $_.X -lt 4300 -and $_.Speed -ge 650 -and $_.Speed -le 710 })
            if ($normalIn.Count -lt 1 -or $onRoad.Count -lt 2 -or $normalOut.Count -lt 2) {
                throw "$role missed ordered normal ground, road boost and normal ground travel"
            }
        }
        Write-Output "PASS: server enforced 7.0, 7.7, 7.0 m/s across the road; driver and observer saw the crossing. Logs: $logs"
    }
    if ($SnowScenario) {
        Wait-Log (Join-Path $logs 'Client1.log') 'SNOW_DRIVER_COMPLETE'
        $serverSamples = @(Select-String -LiteralPath $serverLog -Pattern 'SNOW_SERVER_SAMPLE Pawn=(\S+) X=([-\d.]+) Speed=([\d.]+) Cap=([\d.]+)' | ForEach-Object {
            $g = $_.Matches[0].Groups
            [pscustomobject]@{ Pawn=$g[1].Value; X=[double]::Parse($g[2].Value,[Globalization.CultureInfo]::InvariantCulture);
                Speed=[double]::Parse($g[3].Value,[Globalization.CultureInfo]::InvariantCulture);
                Cap=[double]::Parse($g[4].Value,[Globalization.CultureInfo]::InvariantCulture) }
        })
        $snow = @($serverSamples | Where-Object { $_.X -gt 1600 -and $_.X -lt 2400 -and $_.Speed -ge 545 -and $_.Speed -le 565 -and $_.Cap -ge 559 -and $_.Cap -le 561 })
        if ($snow.Count -lt 3) { throw 'Server did not enforce the authored 0.80 deep snow speed' }
        $pawn = $snow[0].Pawn
        $before = @($serverSamples | Where-Object { $_.Pawn -eq $pawn -and $_.X -gt 300 -and $_.X -lt 750 -and $_.Speed -ge 680 -and $_.Speed -le 705 -and $_.Cap -ge 699 -and $_.Cap -le 701 })
        $after = @($serverSamples | Where-Object { $_.Pawn -eq $pawn -and $_.X -gt 3500 -and $_.X -lt 4300 -and $_.Speed -ge 680 -and $_.Speed -le 705 -and $_.Cap -ge 699 -and $_.Cap -le 701 })
        if ($before.Count -lt 2 -or $after.Count -lt 2 -or $before[-1].X -ge $snow[0].X -or $snow[-1].X -ge $after[0].X) {
            throw 'Server missed ordered normal ground, deep snow and normal ground travel'
        }
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            $samples = @(Select-String -LiteralPath $path -Pattern "SNOW_CLIENT_SAMPLE Role=$role X=([-\d.]+) Speed=([\d.]+)" | ForEach-Object {
                $g = $_.Matches[0].Groups
                [pscustomobject]@{ X=[double]::Parse($g[1].Value,[Globalization.CultureInfo]::InvariantCulture);
                    Speed=[double]::Parse($g[2].Value,[Globalization.CultureInfo]::InvariantCulture) }
            })
            $normalIn = @($samples | Where-Object { $_.X -gt 300 -and $_.X -lt 750 -and $_.Speed -ge 650 -and $_.Speed -le 710 })
            $inSnow = @($samples | Where-Object { $_.X -gt 1600 -and $_.X -lt 2400 -and $_.Speed -ge 525 -and $_.Speed -le 580 })
            $normalOut = @($samples | Where-Object { $_.X -gt 3500 -and $_.X -lt 4300 -and $_.Speed -ge 650 -and $_.Speed -le 710 })
            if ($normalIn.Count -lt 1 -or $inSnow.Count -lt 2 -or $normalOut.Count -lt 2 -or
                $normalIn[-1].X -ge $inSnow[0].X -or $inSnow[-1].X -ge $normalOut[0].X) {
                throw "$role missed ordered normal ground, deep snow and normal ground travel"
            }
        }
        Write-Output "PASS: server enforced 7.0, 5.6, 7.0 m/s across deep snow; driver and observer saw the crossing. Logs: $logs"
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
    if ($GreenlandsMovementNetcheck) {
        Wait-Log $serverLog 'GREENLANDS_MOVEMENT_START X='
        Wait-Log (Join-Path $logs 'Client1.log') 'GREENLANDS_MOVEMENT_COMPLETE'
        Wait-Log $serverLog 'GREENLANDS_ROAD_START X='
        Wait-Log (Join-Path $logs 'Client1.log') 'GREENLANDS_ROAD_COMPLETE'
        $samples = @(Select-String -LiteralPath $serverLog -Pattern 'GREENLANDS_MOVEMENT_SERVER Pawn=(\S+) T=([\d.]+) X=([-\d.]+) C=([01]) Speed=([\d.]+) Dot=([-\d.]+) Cap=([\d.]+)' | ForEach-Object {
            $g = $_.Matches[0].Groups
            [pscustomobject]@{ Pawn=$g[1].Value; T=[double]::Parse($g[2].Value,[Globalization.CultureInfo]::InvariantCulture);
                X=[double]::Parse($g[3].Value,[Globalization.CultureInfo]::InvariantCulture); C=[int]$g[4].Value;
                Speed=[double]::Parse($g[5].Value,[Globalization.CultureInfo]::InvariantCulture);
                Dot=[double]::Parse($g[6].Value,[Globalization.CultureInfo]::InvariantCulture);
                Cap=[double]::Parse($g[7].Value,[Globalization.CultureInfo]::InvariantCulture) }
        })
        $crouch = @($samples | Where-Object { $_.C -eq 1 -and $_.Speed -ge 190 -and $_.Speed -le 201 -and $_.Cap -ge 199 -and $_.Cap -le 201 })
        if ($crouch.Count -lt 3) { throw 'Silverbrook server missed the 2 m/s crouch cap' }
        $pawn = $crouch[0].Pawn
        $pawnSamples = @($samples | Where-Object { $_.Pawn -eq $pawn -and $_.X -gt 49500 })
        $run = @($pawnSamples | Where-Object { $_.T -gt $crouch[-1].T -and $_.C -eq 0 -and $_.Speed -ge 665 -and $_.Speed -le 705 -and $_.Dot -gt 0.9 -and $_.Cap -ge 699 -and $_.Cap -le 701 })
        $reverse = @($pawnSamples | Where-Object { $run.Count -gt 0 -and $_.T -gt $run[0].T -and $_.Speed -ge 390 -and $_.Speed -le 425 -and $_.Dot -lt -0.9 -and $_.Cap -ge 419 -and $_.Cap -le 421 })
        $returned = @($pawnSamples | Where-Object { $reverse.Count -gt 0 -and $_.T -gt $reverse[-1].T -and $_.C -eq 0 -and $_.Speed -ge 665 -and $_.Speed -le 705 -and $_.Dot -gt 0.9 -and $_.Cap -ge 699 -and $_.Cap -le 701 })
        if ($run.Count -lt 3 -or $reverse.Count -lt 3 -or $returned.Count -lt 3 -or
            $crouch[-1].X - $crouch[0].X -lt 300 -or $run[-1].X - $run[0].X -lt 700 -or
            $reverse[0].X - $reverse[-1].X -lt 350 -or $returned[-1].X - $returned[0].X -lt 400) {
            throw 'Silverbrook server missed ordered crouch, run, 0.6x backpedal, run travel'
        }
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            $seen = @(Select-String -LiteralPath $path -Pattern "GREENLANDS_MOVEMENT_CLIENT Role=$role Stage=(\d+) X=([-\d.]+) C=([01]) Speed=([\d.]+) Facing=([-\d.]+)" | ForEach-Object {
                $g = $_.Matches[0].Groups
                [pscustomobject]@{ Stage=[int]$g[1].Value; X=[double]::Parse($g[2].Value,[Globalization.CultureInfo]::InvariantCulture);
                    C=[int]$g[3].Value; Speed=[double]::Parse($g[4].Value,[Globalization.CultureInfo]::InvariantCulture);
                    Facing=[double]::Parse($g[5].Value,[Globalization.CultureInfo]::InvariantCulture) }
            })
            $low = @($seen | Where-Object { $_.Stage -eq 0 -and $_.C -eq 1 -and $_.Speed -ge 175 -and $_.Speed -le 215 })
            $fast = @($seen | Where-Object { $_.Stage -eq 1 -and $_.C -eq 0 -and $_.Speed -ge 600 })
            $back = @($seen | Where-Object { $_.Stage -eq 2 -and $_.C -eq 0 -and $_.Speed -ge 350 -and $_.Speed -le 450 -and $_.Facing -gt 0.9 })
            $again = @($seen | Where-Object { $_.Stage -eq 3 -and $_.C -eq 0 -and $_.Speed -ge 600 })
            if ($low.Count -lt 3 -or $fast.Count -lt 2 -or $back.Count -lt 2 -or $again.Count -lt 2 -or
                $low[-1].X - $low[0].X -lt 250 -or $back[0].X - $back[-1].X -lt 250) {
                throw "$role missed replicated Silverbrook crouch, run and backpedal"
            }
        }
        $roadServer = @(Select-String -LiteralPath $serverLog -Pattern 'GREENLANDS_ROAD_SERVER Pawn=(\S+) X=([-\d.]+) Speed=([\d.]+) Cap=([\d.]+) Road=([01])' | ForEach-Object {
            $g = $_.Matches[0].Groups
            [pscustomobject]@{ Pawn=$g[1].Value; X=[double]::Parse($g[2].Value,[Globalization.CultureInfo]::InvariantCulture);
                Speed=[double]::Parse($g[3].Value,[Globalization.CultureInfo]::InvariantCulture);
                Cap=[double]::Parse($g[4].Value,[Globalization.CultureInfo]::InvariantCulture); Road=[int]$g[5].Value }
        } | Where-Object { $_.Pawn -eq $pawn })
        $onRoad = @($roadServer | Where-Object { $_.Road -eq 1 -and $_.Speed -ge 745 -and $_.Speed -le 775 -and $_.Cap -ge 769 -and $_.Cap -le 771 })
        $beforeRoad = @($roadServer | Where-Object { $_.Road -eq 0 -and $_.X -lt -500 -and $_.Speed -ge 665 -and $_.Speed -le 705 -and $_.Cap -ge 699 -and $_.Cap -le 701 })
        $afterRoad = @($roadServer | Where-Object { $_.Road -eq 0 -and $_.X -gt 1500 -and $_.Speed -ge 665 -and $_.Speed -le 705 -and $_.Cap -ge 699 -and $_.Cap -le 701 })
        if ($beforeRoad.Count -lt 3 -or $onRoad.Count -lt 2 -or $afterRoad.Count -lt 3 -or
            $beforeRoad[-1].X -ge $onRoad[0].X -or $onRoad[-1].X -ge $afterRoad[0].X) {
            throw 'Silverbrook server missed the ordered 7.0 -> 7.7 -> 7.0 m/s road crossing'
        }
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            $roadClient = @(Select-String -LiteralPath $path -Pattern "GREENLANDS_ROAD_CLIENT Role=$role X=([-\d.]+) Speed=([\d.]+) Road=([01])" | ForEach-Object {
                $g = $_.Matches[0].Groups
                [pscustomobject]@{ X=[double]::Parse($g[1].Value,[Globalization.CultureInfo]::InvariantCulture);
                    Speed=[double]::Parse($g[2].Value,[Globalization.CultureInfo]::InvariantCulture); Road=[int]$g[3].Value }
            })
            $before = @($roadClient | Where-Object { $_.Road -eq 0 -and $_.X -lt -500 -and $_.Speed -ge 600 -and $_.Speed -le 715 })
            $during = @($roadClient | Where-Object { $_.Road -eq 1 -and $_.Speed -ge 720 -and $_.Speed -le 790 })
            $after = @($roadClient | Where-Object { $_.Road -eq 0 -and $_.X -gt 1500 -and $_.Speed -ge 600 -and $_.Speed -le 715 })
            if ($before.Count -lt 2 -or $during.Count -lt 2 -or $after.Count -lt 2 -or
                $before[-1].X -ge $during[0].X -or $during[-1].X -ge $after[0].X) {
                throw "$role missed replicated normal-ground, real road and normal-ground travel"
            }
        }
        Write-Output "PASS: Silverbrook server enforced crouch, backpedal and 7.0 -> 7.7 -> 7.0 m/s road travel; both clients saw each phase. Logs: $logs"
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
    if ($AutoAttackScenario) {
        $driverLog = Join-Path $logs 'Client1.log'
        $observerLog = Join-Path $logs 'Client2.log'
        Wait-Log $driverLog 'AUTO_ATTACK_CLIENT_DONE Role=Driver HP=20'
        Wait-Log $observerLog 'AUTO_ATTACK_CLIENT_SAMPLE Role=Observer .*HP=20'
        $toggles = @(Select-String -LiteralPath $serverLog -Pattern 'AUTO_ATTACK_SERVER_TOGGLE Enabled=([01]) T=([\d.]+)')
        $swings = @(Select-String -LiteralPath $serverLog -Pattern 'AUTO_ATTACK_SERVER_SWING T=([\d.]+) Target=\S+ Health=(\d+)')
        if ($toggles.Count -ne 3 -or $swings.Count -ne 2 -or
            $toggles[0].Matches[0].Groups[1].Value -ne '1' -or
            $toggles[1].Matches[0].Groups[1].Value -ne '0' -or
            $toggles[2].Matches[0].Groups[1].Value -ne '1' -or
            $swings[0].Matches[0].Groups[2].Value -ne '40' -or
            $swings[1].Matches[0].Groups[2].Value -ne '20' -or
            $toggles[0].LineNumber -ge $swings[0].LineNumber -or
            $swings[0].LineNumber -ge $toggles[1].LineNumber -or
            $toggles[1].LineNumber -ge $toggles[2].LineNumber -or
            $toggles[2].LineNumber -ge $swings[1].LineNumber) {
            throw 'Server missed ordered enable, swing, stop, resume, swing sequence'
        }
        $number = [Globalization.CultureInfo]::InvariantCulture
        $stoppedFor = [double]::Parse($toggles[2].Matches[0].Groups[2].Value,$number) -
            [double]::Parse($toggles[1].Matches[0].Groups[2].Value,$number)
        if ($stoppedFor -lt 2.4) { throw "Auto attack stop lasted only $stoppedFor seconds" }
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            $samples = @(Select-String -LiteralPath $path -Pattern "AUTO_ATTACK_CLIENT_SAMPLE Role=$role T=([\d.]+) HP=(\d+)")
            $initial = @($samples | Where-Object { $_.Matches[0].Groups[2].Value -eq '60' } | Select-Object -First 1)
            $stopped = @($samples | Where-Object { $_.Matches[0].Groups[2].Value -eq '40' } | Select-Object -First 1)
            $resumed = @($samples | Where-Object { $_.Matches[0].Groups[2].Value -eq '20' } | Select-Object -First 1)
            if ($initial.Count -ne 1 -or $stopped.Count -ne 1 -or $resumed.Count -ne 1 -or
                $initial[0].LineNumber -ge $stopped[0].LineNumber -or $stopped[0].LineNumber -ge $resumed[0].LineNumber) {
                throw "$role missed replicated 60 to 40 to 20 health sequence"
            }
        }
        Write-Output "PASS: authoritative unarmed swings stopped for $stoppedFor s and resumed; both clients saw 60 to 40 to 20 health. Logs: $logs"
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
            'BOAR_GREENLANDS_RESET_REGEN_START', 'BOAR_GREENLANDS_REGEN Health=',
            'BOAR_GREENLANDS_REST_ENTER', 'BOAR_GREENLANDS_REST_EXIT', 'BOAR_GREENLANDS_MEMORY',
            'BOAR_GREENLANDS_AUTO_READY', 'BOAR_GREENLANDS_AUTO_STOP', 'BOAR_GREENLANDS_AUTO_DONE',
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
        $autoReady = Select-String -LiteralPath $serverLog -Pattern 'BOAR_GREENLANDS_AUTO_READY' | Select-Object -First 1
        $autoStopped = Select-String -LiteralPath $serverLog -Pattern 'BOAR_GREENLANDS_AUTO_STOP Health=20 Target=Driver' | Select-Object -First 1
        $autoDone = Select-String -LiteralPath $serverLog -Pattern 'BOAR_GREENLANDS_AUTO_DONE Health=20' | Select-Object -First 1
        $autoToggles = @(Select-String -LiteralPath $serverLog -Pattern 'AUTO_ATTACK_SERVER_TOGGLE Enabled=([01]) T=([\d.]+)')
        $autoSwings = @(Select-String -LiteralPath $serverLog -Pattern 'AUTO_ATTACK_SERVER_SWING T=([\d.]+) Target=\S+ Health=(\d+)')
        if (-not $autoReady -or -not $autoStopped -or -not $autoDone -or $autoToggles.Count -ne 2 -or $autoSwings.Count -ne 2 -or
            $autoToggles[0].Matches[0].Groups[1].Value -ne '1' -or $autoToggles[1].Matches[0].Groups[1].Value -ne '0' -or
            $autoSwings[0].Matches[0].Groups[2].Value -ne '40' -or $autoSwings[1].Matches[0].Groups[2].Value -ne '20' -or
            $autoReady.LineNumber -ge $autoToggles[0].LineNumber -or
            $autoToggles[0].LineNumber -ge $autoSwings[0].LineNumber -or
            $autoSwings[0].LineNumber -ge $autoSwings[1].LineNumber -or
            $autoSwings[1].LineNumber -ge $autoToggles[1].LineNumber -or
            $autoToggles[1].LineNumber -ge $autoStopped.LineNumber -or
            $autoStopped.LineNumber -ge $autoDone.LineNumber) {
            throw 'Greenlands auto attack missed ordered client toggle, two hits, threat and stop'
        }
        $number = [Globalization.CultureInfo]::InvariantCulture
        $swingGap = [double]::Parse($autoSwings[1].Matches[0].Groups[1].Value,$number) -
            [double]::Parse($autoSwings[0].Matches[0].Groups[1].Value,$number)
        $stopGap = [double]::Parse($autoToggles[1].Matches[0].Groups[2].Value,$number) -
            [double]::Parse($autoSwings[1].Matches[0].Groups[1].Value,$number)
        if ($swingGap -lt 1.95 -or $swingGap -gt 2.5 -or $stopGap -gt 1.0) {
            throw "Greenlands unarmed cadence or stop delayed: swing=$swingGap stop=$stopGap"
        }
        $ticks = @([regex]::Matches($serverText, 'BOAR_GREENLANDS_REGEN_TICK Stage=(6|12) Health=(\d+) Rest=([01])') |
            ForEach-Object { [pscustomobject]@{ Stage=[int]$_.Groups[1].Value; HP=[int]$_.Groups[2].Value; Rest=[int]$_.Groups[3].Value } })
        $ordinary = @($ticks | Where-Object { $_.Stage -eq 6 -and $_.Rest -eq 0 })
        $rested = @($ticks | Where-Object { $_.Stage -eq 12 -and $_.Rest -eq 1 })
        if ($ordinary.Count -lt 2 -or $rested.Count -lt 2 -or $ticks.Count -ne $ordinary.Count + $rested.Count) {
            throw 'Server missed ordinary or inn rest ticks on the damaged observer'
        }
        $ordinaryGain = $ordinary[1].HP - $ordinary[0].HP
        $restedGain = $rested[1].HP - $rested[0].HP
        if ($ordinaryGain -lt 2 -or $ordinaryGain -gt 3 -or $restedGain -lt 4 -or $restedGain -gt 5) {
            throw "Server regeneration rates differ from Combat values: ordinary=$ordinaryGain rested=$restedGain"
        }
        if ($serverText -match 'COMBAT_PILOT_SLOT_ERROR|Fatal error:|Assertion failed:') { throw 'Server logged a slot or fatal error' }
        foreach ($role in @('Driver','Observer')) {
            $path = Join-Path $logs ('Client' + $(if ($role -eq 'Driver') { '1' } else { '2' }) + '.log')
            Wait-Log $path "BOAR_GREENLANDS_SAMPLE Role=$role Stage=11 .*LeadHP=60"
            $s = Get-Content -LiteralPath $path -Raw
            $autoSamples = @([regex]::Matches($s,"BOAR_GREENLANDS_SAMPLE Role=$role Stage=(13|14) .*LeadHP=(60|40|20) .*LeadTarget=(Self|Other)") |
                ForEach-Object { [pscustomobject]@{ Stage=[int]$_.Groups[1].Value; HP=[int]$_.Groups[2].Value; Target=$_.Groups[3].Value } })
            $autoTarget = if ($role -eq 'Driver') { 'Self' } else { 'Other' }
            if (@($autoSamples | Where-Object { $_.HP -eq 40 -and $_.Target -eq $autoTarget }).Count -eq 0 -or
                @($autoSamples | Where-Object { $_.HP -eq 20 -and $_.Target -eq $autoTarget }).Count -eq 0) {
                throw "$role missed replicated auto attack damage and threat"
            }
            if ($role -eq 'Driver' -and ($s -notmatch 'BOAR_GREENLANDS_AUTO_CLIENT_START' -or
                $s -notmatch 'BOAR_GREENLANDS_AUTO_CLIENT_STOP Health=20')) {
                throw 'Driver missed local auto attack toggle sequence'
            }
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
            $boarDamage = @([regex]::Matches($s,"BOAR_GREENLANDS_SAMPLE Role=$role Stage=[2345] .*ObserverHP=(\d+) ObserverCombat=1") |
                ForEach-Object { [int]$_.Groups[1].Value } | Where-Object { $_ -gt 0 -and $_ -lt 1000 })
            if ($boarDamage.Count -eq 0) { throw "$role missed the observer taking boar damage" }
            $regen = @([regex]::Matches($s,"BOAR_GREENLANDS_SAMPLE Role=$role Stage=6 .*ObserverHP=(\d+) ObserverCombat=([01])") |
                ForEach-Object { [pscustomobject]@{ HP=[int]$_.Groups[1].Value; Combat=[int]$_.Groups[2].Value } })
            if ($regen.Count -lt 3 -or @($regen | Where-Object { $_.HP -le 60 -and $_.Combat -eq 1 }).Count -eq 0 -or
                @($regen | Where-Object { $_.HP -gt 60 -and $_.Combat -eq 0 }).Count -eq 0) {
                throw "$role missed replicated combat exit and delayed observer healing"
            }
            $rest = @([regex]::Matches($s,"BOAR_GREENLANDS_SAMPLE Role=$role Stage=12 .*ObserverHP=(\d+) ObserverCombat=0 ObserverRest=1") |
                ForEach-Object { [int]$_.Groups[1].Value })
            if ($rest.Count -lt 2 -or $rest[-1] -le $rest[0] -or $rest[0] -le $regen[0].HP) {
                throw "$role missed replicated inn rest healing"
            }
        }
        Write-Output "PASS: Greenlands 2.0 s unarmed auto attack ($swingGap s observed), client stop, boar threat, ordinary healing ($ordinaryGain HP/tick), Golden Stag rest healing ($restedGain HP/tick), client kill and rotated slot respawn on both clients. Logs: $logs"
    }
    Write-Output "PASS: separate editor server and $Clients client(s) connected; $Clients server-authoritative possession(s). Logs: $logs"
    if (-not $CastingScenario -and -not $EvadeScenario -and -not $ThreatScenario -and -not $AggroScenario -and -not $MemoryScenario -and -not $RegenScenario -and -not $RestRegenScenario -and -not $FallScenario -and -not $SwimScenario -and -not $CrouchScenario -and -not $BackpedalScenario -and -not $RoadScenario -and -not $SnowScenario -and -not $TravelMountScenario -and -not $AssistScenario -and -not $RespawnScenario -and -not $AutoAttackScenario -and -not $UnstuckScenario -and -not $SpiritReleaseScenario -and -not $ResurrectionAcceptScenario -and -not $GreenlandsBoarNetcheck -and -not $GreenlandsEnvironmentNetcheck -and -not $GreenlandsMovementNetcheck) { Write-Output 'Headless connection check only: movement, rendering, lifecycle cleanup, authentication and Linux runtime are not certified.' }
} finally {
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
    }
}
