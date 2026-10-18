[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(1024, 65535)][int]$Port = 17781,
    [ValidateSet('Normal', 'Adverse100')][string]$NetworkProfile = 'Normal',
    [switch]$CaptureFrames
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
foreach ($path in @($project, $editor)) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing prerequisite: $path" } }
if (Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue) { throw "UDP $Port is already in use" }
$logs = Join-Path $repoRoot ('GameClient\Saved\Logs\E1-Network-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $logs
$clientRendering = @('-NullRHI')
$frames = @()
if ($CaptureFrames) {
    $framesDir = Join-Path $logs 'Frames'
    $null = New-Item -ItemType Directory -Path $framesDir
    $clientRendering = @('-RenderOffScreen', '-windowed', '-ResX=1280', '-ResY=720', ('-E1CaptureDir="' + $framesDir + '"'))
}
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
# Fixed versioned test conditions, not live gameplay/balance controls. Nominal RTT is not measured RTT.
$lagMs = if ($NetworkProfile -eq 'Adverse100') { 50 } else { 0 }
$varianceMs = if ($NetworkProfile -eq 'Adverse100') { 10 } else { 0 }
$lossPercent = if ($NetworkProfile -eq 'Adverse100') { 1 } else { 0 }
$emulationArguments = @("-PktLag=$lagMs", "-PktLagVariance=$varianceMs", "-PktLoss=$lossPercent", '-PktOrder=0', '-PktDup=0', '-PktIncomingLagMin=0', '-PktIncomingLagMax=0')
function Launch([string[]]$LaunchArguments) {
    $process = Start-Process -FilePath $editor -ArgumentList (@('"' + $project + '"') + $LaunchArguments + $emulationArguments) -WindowStyle Hidden -PassThru
    $owned.Add($process)
}
function Assert-OwnedProcessesAlive {
    foreach ($process in $owned) {
        if ($process.HasExited) { throw "Owned process $($process.Id) exited. Inspect $logs" }
    }
}
function Wait-Log([string]$Path, [string]$Pattern, [int]$Seconds = 90) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Assert-OwnedProcessesAlive
        if ((Test-Path -LiteralPath $Path) -and (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) { return }
        Start-Sleep -Milliseconds 250
    }
    throw "Timed out waiting for $Pattern in $Path"
}
function Read-Samples([string]$Path, [int]$PlayerId) {
    $pattern = 'E1_SCENARIO_SAMPLE Role=(\w+) Pawn=(\S+) PlayerId=(\d+) OwnerLocal=(\d) Time=([\d.]+) X=(-?[\d.]+) Y=(-?[\d.]+) Z=(-?[\d.]+) Grounded=(\d)'
    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -match $pattern -and [int]$Matches[3] -eq $PlayerId) {
            [pscustomobject]@{ X=[double]::Parse($Matches[6], [Globalization.CultureInfo]::InvariantCulture); Y=[double]::Parse($Matches[7], [Globalization.CultureInfo]::InvariantCulture); Z=[double]::Parse($Matches[8], [Globalization.CultureInfo]::InvariantCulture); Grounded=[int]$Matches[9] }
        }
    }
}
function Assert-Movement([string]$Path, [int]$PlayerId) {
    $samples = @(Read-Samples $Path $PlayerId)
    if ($samples.Count -lt 2) { throw "Insufficient samples for PlayerId=$PlayerId in $Path" }
    $first = $samples[0]
    $maxSquared = 0.0
    foreach ($sample in $samples) {
        $distanceSquared = [Math]::Pow($sample.X-$first.X,2)+[Math]::Pow($sample.Y-$first.Y,2)
        $maxSquared = [Math]::Max($maxSquared, $distanceSquared)
    }
    if ($maxSquared -lt 40000.0) { throw "Movement below 200cm for PlayerId=$PlayerId in $Path" }
    return [Math]::Round([Math]::Sqrt($maxSquared), 1)
}
function Assert-CorrelatedTrajectory([string]$ServerLog, [string]$ObserverLog, [int]$PlayerId) {
    $serverSamples = @(Read-Samples $ServerLog $PlayerId)
    $observerSamples = @(Read-Samples $ObserverLog $PlayerId)
    $matched = @()
    foreach ($sample in $serverSamples) {
        foreach ($remote in $observerSamples) {
            $separation = [Math]::Pow($sample.X-$remote.X,2)+[Math]::Pow($sample.Y-$remote.Y,2)+[Math]::Pow($sample.Z-$remote.Z,2)
            if ($separation -le 10000.0) { $matched += $sample; break }
        }
    }
    foreach ($first in $matched) {
        foreach ($last in $matched) {
            if (([Math]::Pow($first.X-$last.X,2)+[Math]::Pow($first.Y-$last.Y,2)) -ge 40000.0) { return }
        }
    }
    throw 'Server and observer lack two spatially matching trajectory samples separated by 200cm (100cm matching tolerance)'
}
function Read-ActualEmulation([string]$Path, [string]$Role) {
    $pattern = 'E1_SCENARIO_EMULATION Role=' + $Role + ' PktLag=(\d+) PktLagVariance=(\d+) PktLoss=(\d+) PktOrder=(\d+) PktDup=(\d+) IncomingLagMin=(\d+) IncomingLagMax=(\d+)'
    Wait-Log $Path $pattern 2
    $line = Select-String -LiteralPath $Path -Pattern $pattern | Select-Object -First 1
    $values = $line.Matches[0].Groups
    $actual = [ordered]@{ LagMs=[int]$values[1].Value; VarianceMs=[int]$values[2].Value; LossPercent=[int]$values[3].Value; Order=[int]$values[4].Value; Dup=[int]$values[5].Value; IncomingLagMinMs=[int]$values[6].Value; IncomingLagMaxMs=[int]$values[7].Value }
    if ($actual.LagMs -ne $lagMs -or $actual.VarianceMs -ne $varianceMs -or $actual.LossPercent -ne $lossPercent -or $actual.Order -ne 0 -or $actual.Dup -ne 0 -or $actual.IncomingLagMinMs -ne 0 -or $actual.IncomingLagMaxMs -ne 0) {
        throw "Actual netdriver emulation disagrees with fixed profile $NetworkProfile for $Role"
    }
    return $actual
}
function Assert-JumpAndLanding([string]$Path, [int]$PlayerId) {
    $samples = @(Read-Samples $Path $PlayerId)
    if ($samples.Count -lt 5) { throw "Insufficient jump samples in $Path" }
    $baseline = ($samples | Measure-Object Z -Minimum).Minimum
    $airborne = @($samples | Where-Object { $_.Grounded -eq 0 -and $_.Z -ge $baseline + 50.0 })
    $last = $samples[-1]
    if (-not $airborne.Count -or $last.Grounded -ne 1 -or [Math]::Abs($last.Z-$baseline) -gt 20.0) { throw "No replicated jump and return to ground in $Path" }
    return [Math]::Round((($samples | Measure-Object Z -Maximum).Maximum - $baseline), 1)
}
try {
    $server = Join-Path $logs 'Server.log'
    $driver = Join-Path $logs 'Driver.log'
    $observer = Join-Path $logs 'Observer.log'
    Launch @('/Game/Frontier/Maps/Greenlands_E1', '-server', '-NullRHI', '-unattended', '-nosound', '-nop4', '-E1ProbeRole=Server', "-port=$Port", '-MULTIHOME=127.0.0.1', ('-abslog="' + $server + '"'))
    Wait-Log $server 'E1_WORLD_READY.*NetMode=1'
    # Start both clients together to preserve the finite server sampling window.
    Launch (@("127.0.0.1:$Port", '-game', '-unattended', '-nosound', '-nop4', '-E1ProbeRole=Driver', ('-abslog="' + $driver + '"')) + $clientRendering)
    Launch (@("127.0.0.1:$Port", '-game', '-unattended', '-nosound', '-nop4', '-E1ProbeRole=Observer', ('-abslog="' + $observer + '"')) + $clientRendering)
    Wait-Log $driver 'Welcomed by server'
    Wait-Log $observer 'Welcomed by server'
    Wait-Log $driver 'E1_INTERACTION_RESULT Result=0 Accepted'
    Wait-Log $driver 'E1_SCENARIO_DONE Role=Driver' 45
    Wait-Log $observer 'E1_SCENARIO_DONE Role=Observer' 45
    $actualEmulation = [ordered]@{ Server=(Read-ActualEmulation $server 'Server'); Driver=(Read-ActualEmulation $driver 'Driver'); Observer=(Read-ActualEmulation $observer 'Observer') }
    $intent = Select-String -LiteralPath $driver -Pattern 'E1_SCENARIO_DRIVER Pawn=(\S+) PlayerId=(\d+) Action=Interact' | Select-Object -First 1
    if (-not $intent) { throw 'Driver never issued the public interaction intent' }
    $playerId = [int]$intent.Matches[0].Groups[2].Value
    $authorityPattern = 'E1_INTERACTION_SERVER_RESULT Pawn=\S+ PlayerId=' + $playerId + ' Result=0 Authority=1'
    Wait-Log $server $authorityPattern 2
    $serverDistance = Assert-Movement $server $playerId
    $observerDistance = Assert-Movement $observer $playerId
    $driverDistance = Assert-Movement $driver $playerId
    Assert-CorrelatedTrajectory $server $observer $playerId
    $serverJumpCm = Assert-JumpAndLanding $server $playerId
    $observerJumpCm = Assert-JumpAndLanding $observer $playerId
    $ownerPattern = 'E1_SCENARIO_SAMPLE Role=Driver Pawn=\S+ PlayerId=' + $playerId + ' OwnerLocal=1'
    if (-not (Select-String -LiteralPath $driver -Pattern $ownerPattern -Quiet)) { throw 'Driver movement did not belong to its locally controlled pawn' }
    $observerPattern = 'E1_SCENARIO_SAMPLE Role=Observer Pawn=\S+ PlayerId=' + $playerId + ' OwnerLocal=0'
    if (-not (Select-String -LiteralPath $observer -Pattern $observerPattern -Quiet)) { throw 'Observer did not sample the remote Driver pawn' }
    if ($CaptureFrames) {
        foreach ($role in @('Driver','Observer')) {
            foreach ($phase in @('Arrival','Probe','Departed')) {
                $frame = Join-Path $framesDir "$role-$phase.png"
                if (-not (Test-Path -LiteralPath $frame -PathType Leaf)) { throw "Missing actual capture: $frame" }
                $frameStream = $null
                $frameImage = $null
                try {
                    $frameStream = [IO.File]::OpenRead($frame)
                    $frameImage = [Drawing.Image]::FromStream($frameStream, $false, $true)
                    if ($frameImage.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid -or $frameImage.Width -ne 1280 -or $frameImage.Height -ne 720) {
                        throw 'Capture must decode as a 1280x720 PNG'
                    }
                } catch {
                    throw "Invalid PNG capture ${frame}: $($_.Exception.Message)"
                } finally {
                    if ($null -ne $frameImage) { $frameImage.Dispose() }
                    if ($null -ne $frameStream) { $frameStream.Dispose() }
                }
                $frames += $frame
            }
        }
    }
    $build = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
    $summary = [ordered]@{
        SchemaVersion=1; Result='PASS'; Map='/Game/Frontier/Maps/Greenlands_E1'; NetworkProfile=$NetworkProfile
        EmulationRevision=1; NominalConfiguredRttMs=(2*$lagMs); MeasuredRttMs=$null; ActualEmulation=$actualEmulation
        ProcessIds=[ordered]@{ Server=$owned[0].Id; Driver=$owned[1].Id; Observer=$owned[2].Id }
        LocalMetadata=[ordered]@{ EngineVersion="$($build.MajorVersion).$($build.MinorVersion).$($build.PatchVersion)"; EngineChangelist=$build.Changelist; OSVersion=[Environment]::OSVersion.VersionString; LogicalProcessors=[Environment]::ProcessorCount }
        DriverPlayerId=$playerId; DisplacementCm=[ordered]@{ Driver=$driverDistance; Server=$serverDistance; Observer=$observerDistance }
        JumpHeightObservedCm=[ordered]@{ Server=$serverJumpCm; Observer=$observerJumpCm }
        CaptureMode=$(if ($CaptureFrames) { 'RenderedOffscreen' } else { 'NullRHI' }); CapturedFrames=$frames
        Limits='Development scenario; offscreen frames require visual inspection and are not manual acceptance. No adversarial, Linux runtime, measured RTT or capacity certification.'
    }
    Assert-OwnedProcessesAlive
    $summaryPath = Join-Path $logs 'Summary.json'
    $summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8
    Write-Output "PASS: replicated Driver PlayerId=$playerId moved Driver=${driverDistance}cm Server=${serverDistance}cm Observer=${observerDistance}cm; owner received Accepted probe RPC. Logs: $logs"
    Write-Output 'Normal-input replication/probe evidence only; optional offscreen frames require visual inspection, not manual acceptance. No adversarial ownership, Linux runtime or capacity proof.'
    Write-Output "Profile=$NetworkProfile actual emulation verified on three netdrivers; nominal configured RTT=$($lagMs*2)ms, not measured. Summary: $summaryPath"
} finally {
    foreach ($process in $owned) { if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue } }
}
