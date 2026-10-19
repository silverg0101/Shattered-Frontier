[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(1024, 65535)][int]$Port = 17779,
    [ValidateRange(30, 300)][int]$TimeoutSeconds = 120
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logs = Join-Path $repoRoot ('GameClient\Saved\Logs\E1-Lifecycle-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$ownedProcesses = [Collections.Generic.List[Diagnostics.Process]]::new()
$seenControllers = [Collections.Generic.HashSet[string]]::new()
$seenPawns = [Collections.Generic.HashSet[string]]::new()
$results = [Collections.Generic.List[object]]::new()
$script:server = $null
$script:serverLog = Join-Path $logs 'Server.log'

function Launch([string]$Label, [string[]]$LaunchArguments) {
    $process = Start-Process -FilePath $editor -ArgumentList (@('"' + $project + '"') + $LaunchArguments) -WindowStyle Hidden -PassThru
    $null = $process.Handle  # PS 5.1 keeps ExitCode only if the handle is opened before exit
    $ownedProcesses.Add($process)
    Write-Host "Started $Label PID $($process.Id)"
    return $process
}

function Read-Log([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return '' }
    if ((Get-Item -LiteralPath $Path).Length -gt 16MB) { throw "Log exceeds bounded 16 MiB limit: $Path" }
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    $reader = [IO.StreamReader]::new($stream)
    try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
}

function Wait-Until([string]$Description, [scriptblock]$Condition, [Diagnostics.Process]$Client = $null) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($script:server -and $script:server.HasExited) { throw "Server exited while waiting for $Description" }
        if ($Client -and $Client.HasExited) { throw "Client PID $($Client.Id) exited while waiting for $Description" }
        $result = & $Condition
        if ($result) { return $result }
        Start-Sleep -Milliseconds 250
    }
    throw "Timed out after ${TimeoutSeconds}s waiting for $Description. Logs: $logs"
}

function Read-States {
    $content = Read-Log $script:serverLog
    $statePattern = 'E1_LIFECYCLE_STATE Audit=(\d+) Event=(Join|Leave) Controller=(\S+) Pawn=(\S+) DepartedControllerActive=(\d+) DepartedPawnActive=(\d+) DepartedPawnPossessed=(\d+) Players=(\d+) ActiveControllers=(\d+) ActivePawns=(\d+) PossessedPawns=(\d+) Authority=(\d+)'
    foreach ($match in [regex]::Matches($content, $statePattern)) {
        $audit = [int]$match.Groups[1].Value
        $pairs = @([regex]::Matches($content, "E1_LIFECYCLE_PAIR Audit=$audit Controller=(\S+) Pawn=(\S+) Authority=(\d+)") | ForEach-Object {
            [pscustomobject]@{ Controller = $_.Groups[1].Value; Pawn = $_.Groups[2].Value; Authority = [int]$_.Groups[3].Value }
        })
        [pscustomobject]@{
            Audit = $audit; Event = $match.Groups[2].Value; Controller = $match.Groups[3].Value; Pawn = $match.Groups[4].Value
            DepartedControllerActive = [int]$match.Groups[5].Value; DepartedPawnActive = [int]$match.Groups[6].Value
            DepartedPawnPossessed = [int]$match.Groups[7].Value; Players = [int]$match.Groups[8].Value
            ActiveControllers = [int]$match.Groups[9].Value; ActivePawns = [int]$match.Groups[10].Value
            PossessedPawns = [int]$match.Groups[11].Value; Authority = [int]$match.Groups[12].Value; Pairs = $pairs
        }
    }
}

function Assert-State($State) {
    $count = $State.Pairs.Count
    if ($State.Authority -ne 1 -or $State.Players -ne $count -or $State.ActiveControllers -ne $count -or
        $State.ActivePawns -ne $count -or $State.PossessedPawns -ne $count) {
        throw "Audit $($State.Audit) has inconsistent authoritative player/controller/pawn/possession counts"
    }
    if (@($State.Pairs | Where-Object { $_.Authority -ne 1 -or $_.Pawn -notlike 'SFE1Character*' }).Count -gt 0 -or
        @($State.Pairs.Controller | Select-Object -Unique).Count -ne $count -or
        @($State.Pairs.Pawn | Select-Object -Unique).Count -ne $count) {
        throw "Audit $($State.Audit) contains invalid or duplicate authoritative pairs"
    }
}

function Has-Pair($State, $Pair) {
    return @($State.Pairs | Where-Object { $_.Controller -eq $Pair.Controller -and $_.Pawn -eq $Pair.Pawn }).Count -eq 1
}

function Assert-Survivors($State, [object[]]$Survivors) {
    foreach ($pair in $Survivors) {
        if (-not (Has-Pair $State $pair)) { throw "Audit $($State.Audit) changed surviving pair $($pair.Controller)/$($pair.Pawn)" }
    }
}

function Last-Audit {
    $states = @(Read-States)
    if ($states.Count -eq 0) { return 0 }
    return $states[-1].Audit
}

function Start-Client([string]$Label, [object[]]$Survivors) {
    $before = Last-Audit
    $clientLog = Join-Path $logs "$Label.log"
    $process = Launch $Label @("127.0.0.1:$Port", '-game', '-NullRHI', '-unattended', '-nosound', '-nop4', ('-abslog="' + $clientLog + '"'))
    $null = Wait-Until "$Label welcome" { (Read-Log $clientLog) -match 'Welcomed by server' } $process
    $state = Wait-Until "$Label authoritative join snapshot" {
        @(Read-States | Where-Object { $_.Audit -gt $before -and $_.Event -eq 'Join' -and $_.Pairs.Count -eq $_.ActiveControllers } | Select-Object -Last 1)
    } $process
    Assert-State $state
    Assert-Survivors $state $Survivors
    $knownControllers = @($Survivors | ForEach-Object { $_.Controller })
    $newPairs = @($state.Pairs | Where-Object { $_.Controller -notin $knownControllers })
    if ($newPairs.Count -ne 1 -or $state.Controller -ne $newPairs[0].Controller) { throw "$Label did not produce exactly one new authoritative pair" }
    $pair = $newPairs[0]
    if (-not $seenControllers.Add($pair.Controller) -or -not $seenPawns.Add($pair.Pawn)) { throw "$Label reused a prior controller/pawn name during this server lifetime" }
    $results.Add([pscustomobject]@{ Step = "$Label join"; Audit = $state.Audit; PID = $process.Id; Controller = $pair.Controller; Pawn = $pair.Pawn; Players = $state.Players; ActivePawns = $state.ActivePawns; PossessedPawns = $state.PossessedPawns })
    Write-Host "Verified $Label join audit $($state.Audit): $($pair.Controller)/$($pair.Pawn)"
    return [pscustomobject]@{ Label = $Label; Process = $process; Controller = $pair.Controller; Pawn = $pair.Pawn }
}

function Exit-Client($Client) {
    $before = Last-Audit
    if ($Client.Process.HasExited) { throw "$($Client.Label) already exited before its lifecycle step" }
    # Deliberate abrupt process exit exercises server-side disconnect detection and cleanup.
    Stop-Process -Id $Client.Process.Id
    if (-not $Client.Process.WaitForExit(10000)) { throw "Owned client PID $($Client.Process.Id) did not exit" }
    return $before
}

function Wait-Leave($Departed, [int]$AfterAudit, [object[]]$Survivors) {
    $state = Wait-Until "$($Departed.Label) cleanup snapshot" {
        @(Read-States | Where-Object { $_.Audit -gt $AfterAudit -and $_.Event -eq 'Leave' -and $_.Controller -eq $Departed.Controller -and $_.Pairs.Count -eq $_.ActiveControllers } | Select-Object -First 1)
    }
    Assert-State $state
    Assert-Survivors $state $Survivors
    if ($state.Pawn -ne $Departed.Pawn) {
        throw "Audit $($state.Audit) cannot correlate departed pawn: expected $($Departed.Pawn), observed $($state.Pawn)"
    }
    if ($state.DepartedControllerActive -ne 0 -or
        $state.DepartedPawnActive -ne 0 -or $state.DepartedPawnPossessed -ne 0 -or
        @($state.Pairs | Where-Object { $_.Controller -eq $Departed.Controller -or $_.Pawn -eq $Departed.Pawn }).Count -ne 0) {
        throw "Audit $($state.Audit) retains departed controller/pawn/possession for $($Departed.Label)"
    }
    $allowed = @($Survivors | ForEach-Object { $_.Controller })
    if (@($state.Pairs | Where-Object { $_.Controller -notin $allowed }).Count -gt 0) { throw "Audit $($state.Audit) contains an unexpected live pair" }
    foreach ($survivor in $Survivors) { if ($survivor.Process.HasExited) { throw "Surviving client $($survivor.Label) exited unexpectedly" } }
    $results.Add([pscustomobject]@{ Step = "$($Departed.Label) cleanup"; Audit = $state.Audit; Controller = $Departed.Controller; Pawn = $Departed.Pawn; Players = $state.Players; ActivePawns = $state.ActivePawns; PossessedPawns = $state.PossessedPawns; DepartedControllerActive = 0; DepartedPawnActive = 0; DepartedPawnPossessed = 0 })
    Write-Host "Verified $($Departed.Label) cleanup audit $($state.Audit); no departed pawn/possession survives"
}

function Assert-FinalPairs([object[]]$Expected) {
    $states = @(Read-States)
    $state = $states[-1]
    Assert-State $state
    Assert-Survivors $state $Expected
    if ($state.Pairs.Count -ne $Expected.Count) { throw "Final audit $($state.Audit) has unexpected live connections" }
}

try {
    if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Editor executable missing: $editor" }
    if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { throw "Project missing: $project" }
    if (Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue) { throw "UDP $Port is already in use" }
    $null = New-Item -ItemType Directory -Path $logs
    $script:server = Launch 'server' @('/Game/Frontier/Maps/Greenlands_E1', '-server', '-NullRHI', '-unattended', '-nosound', '-nop4', "-port=$Port", '-MULTIHOME=127.0.0.1', ('-abslog="' + $script:serverLog + '"'))
    $null = Wait-Until 'dedicated editor server readiness' { (Read-Log $script:serverLog) -match 'E1_WORLD_READY.*NetMode=1' }
    $a = Start-Client 'ClientA1' @()
    $b = Start-Client 'ClientB1' @($a)
    $before = Exit-Client $a
    Wait-Leave $a $before @($b)
    $a = Start-Client 'ClientA2' @($b)
    $before = Exit-Client $b
    Wait-Leave $b $before @($a)
    $b = Start-Client 'ClientB2' @($a)

    # Keep the prior connection alive until its replacement joins: three distinct
    # anonymous sessions overlap. This does not establish an account identity policy.
    $oldA = $a
    $a = Start-Client 'ClientA3' @($b, $oldA)
    Assert-FinalPairs @($a, $b, $oldA)
    $before = Exit-Client $oldA
    Wait-Leave $oldA $before @($b, $a)
    Assert-FinalPairs @($a, $b)
    $oldB = $b
    $b = Start-Client 'ClientB3' @($a, $oldB)
    Assert-FinalPairs @($a, $b, $oldB)
    $before = Exit-Client $oldB
    Wait-Leave $oldB $before @($a, $b)
    Assert-FinalPairs @($a, $b)

    $before = Exit-Client $a
    Wait-Leave $a $before @($b)
    $before = Exit-Client $b
    Wait-Leave $b $before @()
    Assert-FinalPairs @()
    [pscustomobject]@{
        Test = 'E1 local anonymous connection lifecycle'; EngineRoot = $EngineRoot; Project = $project
        Map = '/Game/Frontier/Maps/Greenlands_E1'; Port = $Port; ServerPID = $script:server.Id; Steps = $results.ToArray()
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $logs 'Lifecycle-results.json') -Encoding UTF8
    Write-Output "PASS: six independent client joins, individual reconnect and two stages with three overlapping connections, six authoritative cleanup audits, final zero live pawns/possessions. Server PID $($script:server.Id), UDP $Port. Logs: $logs"
    Write-Output 'Headless local connection lifecycle only; no account identity/authentication, durable reconnect, rendered movement, input ownership attack, or Linux runtime acceptance.'
} finally {
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
    }
}
