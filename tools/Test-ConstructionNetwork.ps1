[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17791,
    [switch]$CaptureFrames,
    [Parameter(HelpMessage='Maximum seconds per startup or exit phase.')][ValidateRange(60,150)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
if ($CaptureFrames) { Add-Type -AssemblyName System.Drawing }
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root = Join-Path $repoRoot ('Artifacts\ConstructionSandbox\network-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw 'Installed editor commandlet is missing.' }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object { $_.Port -in @($Port,($Port+2)) }).Count) { throw 'An owned test port is already in use.' }
if (Test-Path -LiteralPath $root) { throw 'Evidence root exists.' }
$null = New-Item -ItemType Directory -Path $root
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$results = [ordered]@{ status='Running'; engine=$editor; project=$project; port=$Port; runs=@(); scope='Isolated loopback editor dedicated-server, canonical cube/slab/slope placement/removal and door placement/toggling with rough-timber support, transient build capability; no production ownership/persistence/general procedural shape or packaged acceptance.' }
$clock = [Diagnostics.Stopwatch]::StartNew()
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra) {
    $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $log = Join-Path $root ($Name+'.log')
    $arguments = @(('"'+$project+'"'),$Url,'-unattended','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    if ($Name -in @('driver','observer')) { $arguments += ('-ConstructionNetworkRemovalBarrier="'+(Join-Path $root 'both-projected-three.ready')+'"') }
    if ($CaptureFrames -and $Name -in @('driver','observer')) {
        $arguments += @('-RenderOffscreen','-windowed','-ResX=1280','-ResY=720',('-ConstructionNetworkScreenshot="'+(Join-Path $root ($Name+'.png'))+'"'))
    } else { $arguments += '-NullRHI' }
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt'))
    $null = $process.Handle  # PS 5.1 keeps ExitCode only if the handle is opened before exit
    $owned.Add($process)
    $results.runs += [ordered]@{ name=$Name; pid=$process.Id; arguments=$arguments; log=$log }
    return $process
}
function Wait-Log([Diagnostics.Process]$Process,[string]$Name,[string]$Marker) {
    $phase = [Diagnostics.Stopwatch]::StartNew()
    $log = Join-Path $root ($Name+'.log')
    while ($phase.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern $Marker -Quiet)) { return }
        if ($Process.HasExited) { throw "$Name exited before $Marker." }
        Start-Sleep -Milliseconds 200
    }
    throw "Timed out waiting for $Name ($Marker)."
}
function Wait-Exit([Diagnostics.Process]$Process,[string]$Name) {
    $phase = [Diagnostics.Stopwatch]::StartNew()
    while (-not $Process.WaitForExit(200)) { if ($phase.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw "Timed out waiting for $Name." } }
    foreach ($run in $results.runs) { if ($run.name -eq $Name) { $run.exitCode=$Process.ExitCode } }
    if ($Process.ExitCode -ne 0) { throw "$Name exited $($Process.ExitCode)." }
}
try {
    $normal = Start-Owned 'normal-negative' '/Game/Frontier/Maps/Greenlands_E1' @('-game','-ConstructionNetworkSandbox','-ConstructionNetworkScenario','-testexit=E1_WORLD_READY')
    Wait-Exit $normal 'normal-negative'
    $normalLog = Get-Content -LiteralPath (Join-Path $root 'normal-negative.log') -Raw
    if ($normalLog -notmatch 'E1_WORLD_READY' -or $normalLog -match 'CONSTRUCTION_NETWORK_') { throw 'Normal E1 admitted the network sandbox.' }
    $url = '/Game/Frontier/Maps/Greenlands_ConstructionSandbox?game=/Script/ShatteredFrontier.SFGameMode'
    $disabled = Start-Owned 'flag-negative' $url @('-server',('-port='+($Port+2)),'-multihome=127.0.0.1','-testexit=CONSTRUCTION_NETWORK_DISABLED')
    Wait-Exit $disabled 'flag-negative'
    $disabledLog = Get-Content -LiteralPath (Join-Path $root 'flag-negative.log') -Raw
    if ($disabledLog -notmatch 'CONSTRUCTION_NETWORK_DISABLED' -or $disabledLog -match 'CONSTRUCTION_NETWORK_READY|CONSTRUCTION_NETWORK_ACCEPT|CONSTRUCTION_NETWORK_EDIT') { throw 'Missing startup opt-in did not fence the network mode.' }
    $server = Start-Owned 'server' $url @('-server',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox','-ConstructionNetworkDoorObstruction')
    Wait-Log $server 'server' 'CONSTRUCTION_NETWORK_READY'
    $driver = Start-Owned 'driver' ('127.0.0.1:'+$Port) @('-game','-ConstructionNetworkScenario')
    $observer = Start-Owned 'observer' ('127.0.0.1:'+$Port) @('-game','-ConstructionNetworkScenario')
    $nonemptyPayloads=@()
    foreach ($role in @('driver','observer')) {
        $process=if ($role -eq 'driver') { $driver } else { $observer }
        Wait-Log $process $role 'CONSTRUCTION_NETWORK_PROJECTED Revision=3 Members=3 Collision=1 Content='
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $matches=[regex]::Matches($log,'CONSTRUCTION_NETWORK_PROJECTED Revision=3 Members=3 Collision=1 Content=([a-fA-F0-9]{64})')
        if ($matches.Count -ne 1) { throw "$role lacks one nonempty pre-removal projection." }
        $nonemptyPayloads += $matches[0].Groups[1].Value
    }
    if ($nonemptyPayloads[0] -ne $nonemptyPayloads[1]) { throw 'Clients disagree before removal.' }
    $results.nonemptyContentBlake3=$nonemptyPayloads[0]
    Set-Content -LiteralPath (Join-Path $root 'both-projected-three.ready') -Value 'Both clients projected revision3 with matching content and collision.' -Encoding ascii
    $emptyPayloads=@()
    foreach ($role in @('driver','observer')) {
        $process=if ($role -eq 'driver') { $driver } else { $observer }
        Wait-Log $process $role 'CONSTRUCTION_NETWORK_EMPTY_READY Builder='
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $matches=[regex]::Matches($log,'CONSTRUCTION_NETWORK_PROJECTED Revision=6 Members=0 Collision=1 Content=([a-fA-F0-9]{64})')
        if ($matches.Count -ne 1) { throw "$role lacks one coherent empty projection before mixed shapes." }
        $emptyPayloads += $matches[0].Groups[1].Value
    }
    if ($emptyPayloads[0] -ne $emptyPayloads[1]) { throw 'Clients disagree on empty projection.' }
    $results.emptyContentBlake3=$emptyPayloads[0]
    Set-Content -LiteralPath (Join-Path $root 'both-projected-three.ready.empty') -Value 'Both clients validated empty revision6; observer denials completed.' -Encoding ascii
    $closedPayloads=@()
    foreach ($role in @('driver','observer')) {
        $process=if ($role -eq 'driver') { $driver } else { $observer }
        Wait-Log $process $role 'CONSTRUCTION_NETWORK_DOOR_CLOSED_READY Builder='
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $matches=[regex]::Matches($log,'CONSTRUCTION_NETWORK_PROJECTED Revision=11 Members=2 Collision=1 Content=([a-fA-F0-9]{64}) Doors=1')
        if ($matches.Count -ne 1) { throw "$role lacks the closed door before toggling." }
        $closedPayloads += $matches[0].Groups[1].Value
    }
    if ($closedPayloads[0] -ne $closedPayloads[1]) { throw 'Clients disagree on the closed door.' }
    $results.closedDoorContentBlake3=$closedPayloads[0]
    Set-Content -LiteralPath (Join-Path $root 'both-projected-three.ready.door-closed') -Value 'Both clients projected closed11; observer toggle denied.' -Encoding ascii
    $openPayloads=@()
    foreach ($role in @('driver','observer')) {
        $process=if ($role -eq 'driver') { $driver } else { $observer }
        Wait-Log $process $role 'CONSTRUCTION_NETWORK_DOOR_OPEN_READY Builder='
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $matches=[regex]::Matches($log,'CONSTRUCTION_NETWORK_PROJECTED Revision=12 Members=2 Collision=1 Content=([a-fA-F0-9]{64}) Doors=1')
        if ($matches.Count -ne 1) { throw "$role lacks the open-door collision projection." }
        $openPayloads += $matches[0].Groups[1].Value
    }
    if ($openPayloads[0] -ne $openPayloads[1] -or $openPayloads[0] -eq $closedPayloads[0]) { throw 'Open-door clients disagree or did not change pose.' }
    $results.openDoorContentBlake3=$openPayloads[0]
    Set-Content -LiteralPath (Join-Path $root 'both-projected-three.ready.door-open') -Value 'Both clients projected open12; optional open captures completed.' -Encoding ascii
    foreach ($role in @('driver','observer')) {
        $process=if ($role -eq 'driver') { $driver } else { $observer }
        Wait-Log $process $role 'CONSTRUCTION_NETWORK_DOOR_REMOVE_READY Builder='
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $matches=[regex]::Matches($log,'CONSTRUCTION_NETWORK_PROJECTED Revision=13 Members=2 Collision=1 Content=([a-fA-F0-9]{64}) Doors=1')
        if ($matches.Count -ne 1 -or $matches[0].Groups[1].Value -ne $closedPayloads[0]) { throw "$role did not restore the closed door before removal." }
    }
    Set-Content -LiteralPath (Join-Path $root 'both-projected-three.ready.door-remove') -Value 'Both clients projected closed13; observer removal denied.' -Encoding ascii
    $removedPayloads=@()
    foreach ($role in @('driver','observer')) {
        $process=if ($role -eq 'driver') { $driver } else { $observer }
        Wait-Log $process $role 'CONSTRUCTION_NETWORK_LINE_READY Builder='
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $matches=[regex]::Matches($log,'CONSTRUCTION_NETWORK_PROJECTED Revision=14 Members=2 Collision=1 Content=([a-fA-F0-9]{64}) Doors=0')
        if ($matches.Count -ne 1) { throw "$role lacks the no-door projection before line placement." }
        $removedPayloads += $matches[0].Groups[1].Value
    }
    if ($removedPayloads[0] -ne $removedPayloads[1]) { throw 'Clients disagree on door removal.' }
    Set-Content -LiteralPath (Join-Path $root 'both-projected-three.ready.line') -Value 'Both clients projected removed14; observer line denied.' -Encoding ascii
    $baseLinePayloads=@()
    foreach ($role in @('driver','observer')) {
        $process=if ($role -eq 'driver') { $driver } else { $observer }
        Wait-Log $process $role 'CONSTRUCTION_NETWORK_LAYER_READY Builder='
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $matches=[regex]::Matches($log,'CONSTRUCTION_NETWORK_PROJECTED Revision=15 Members=5 Collision=1 Content=([a-fA-F0-9]{64}) Doors=0')
        if ($matches.Count -ne 1) { throw "$role lacks the base row before layer placement." }
        $baseLinePayloads += $matches[0].Groups[1].Value
    }
    if ($baseLinePayloads[0] -ne $baseLinePayloads[1]) { throw 'Clients disagree on the base row.' }
    Set-Content -LiteralPath (Join-Path $root 'both-projected-three.ready.layer') -Value 'Both clients projected base row15 before upper-floor controls.' -Encoding ascii
    Wait-Exit $driver 'driver'
    Wait-Exit $observer 'observer'
    if ($server.HasExited) { throw 'Owned server exited during the client exercise.' }
    $payloads = @()
    $observedRoles = @()
    foreach ($role in @('driver','observer')) {
        $log = Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $success = [regex]::Matches($log,'CONSTRUCTION_NETWORK_SCENARIO_SUCCESS Builder=([01]) Revision=16 Members=8 Doors=0')
        if ($success.Count -ne 1 -or $log -match 'CONSTRUCTION_NETWORK_\w*FAILED|Fatal error:|Assertion failed:|Ensure condition failed:') { throw "$role failed its semantic scenario." }
        $observedRoles += [int]$success[0].Groups[1].Value
        $projection = [regex]::Matches($log,'CONSTRUCTION_NETWORK_PROJECTED Revision=16 Members=8 Collision=1 Content=([a-fA-F0-9]{64}) Doors=0')
        if ($projection.Count -ne 1) { throw "$role lacks a unique exact local collision projection." }
        $payloads += $projection[0].Groups[1].Value
        if ($CaptureFrames) {
            if ($log -notmatch 'CONSTRUCTION_NETWORK_CAPTURE_SAVED') { throw "$role did not complete its capture." }
            $frame=[Drawing.Image]::FromFile((Join-Path $root ($role+'.png')))
            try {
                if ($frame.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid -or $frame.Width -ne 1280 -or $frame.Height -ne 720) { throw "$role capture is not the requested 1280x720 PNG." }
            } finally { $frame.Dispose() }
            $openFrame=[Drawing.Image]::FromFile((Join-Path $root ($role+'.open.png')))
            try {
                if ($openFrame.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid -or $openFrame.Width -ne 1280 -or $openFrame.Height -ne 720) { throw "$role open capture is not1280x720 PNG." }
            } finally { $openFrame.Dispose() }
        }
    }
    if (@($observedRoles | Sort-Object -Unique).Count -ne 2) { throw 'Expected one server-assigned builder and one observer.' }
    $viewRequests=0
    foreach ($role in @('driver','observer')) {
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $viewRequests += [regex]::Matches($log,'CONSTRUCTION_NETWORK_VIEW_REQUEST Cell=X=-4 Y=-4 Z=0 Shape=0 ').Count
    }
    $mixedViewRequests=0
    $yawRotation=$null
    foreach ($role in @('driver','observer')) {
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $mixedViewRequests += [regex]::Matches($log,'CONSTRUCTION_NETWORK_VIEW_REQUEST Cell=X=-4 Y=-4 Z=0 Shape=1 ').Count
        $mixedViewRequests += [regex]::Matches($log,'CONSTRUCTION_NETWORK_VIEW_REQUEST Cell=X=-4 Y=-5 Z=0 Shape=2 ').Count
        $yaw=[regex]::Matches($log,'CONSTRUCTION_NETWORK_SELECTION Shape=2 Yaw=1 Rotation=([0-9]+)')
        if ($yaw.Count -eq 1) { $yawRotation=$yaw[0].Groups[1].Value }
    }
    $memberSnap=0
    $supportPreviews=0
    foreach ($role in @('driver','observer')) {
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $memberSnap += [regex]::Matches($log,'CONSTRUCTION_NETWORK_VIEW_REQUEST Cell=X=-4 Y=-5 Z=0 Shape=2 Rotation=[0-9]+ FromMember=1').Count
        foreach ($marker in @('CONSTRUCTION_NETWORK_SUPPORT_PREVIEW SupportingBaseAllowed=0 Revision=3','CONSTRUCTION_NETWORK_SUPPORT_PREVIEW SupportedTopAllowed=1 Revision=3','CONSTRUCTION_NETWORK_SUPPORT_PREVIEW PlacementAllowed=0 RemovalAllowed=1 Revision=8')) {
            $supportPreviews += [regex]::Matches($log,[regex]::Escape($marker)).Count
        }
    }
    if ($supportPreviews -ne 3) { throw 'Expected all three local support preview observations.' }
    if ($memberSnap -ne 1) { throw 'Slope placement did not snap from the existing slab face.' }
    if ($mixedViewRequests -ne 2 -or $null -eq $yawRotation) { throw 'Slab/slope selection, R yaw or B input path missing.' }
    $removeRequests=0
    foreach ($role in @('driver','observer')) {
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $removeRequests += [regex]::Matches($log,'CONSTRUCTION_NETWORK_VIEW_REMOVE Cell=X=-4 Y=-4 Z=1').Count
    }
    if ($removeRequests -ne 1) { throw 'Expected one supported top removal through ordinary demolition-mode LMB input.' }
    if ($viewRequests -ne 1) { throw 'Expected one grounded placement through ordinary B-key view input.' }
    if ($payloads[0] -ne $payloads[1]) { throw 'Clients projected different canonical payloads.' }
    $serverLog = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
    if ($serverLog -notmatch ('CONSTRUCTION_NETWORK_EDIT Action=Place Revision=3 Members=3[^\r\n]*Content='+[regex]::Escape($nonemptyPayloads[0]))) { throw 'Nonempty clients differ from server content.' }
    foreach ($marker in @('CONSTRUCTION_NETWORK_EDIT Action=Place Revision=1 Members=1 Cell=X=-4 Y=-4 Z=0','CONSTRUCTION_NETWORK_EDIT Action=Place Revision=2 Members=2 Cell=X=-4 Y=-4 Z=1','CONSTRUCTION_NETWORK_EDIT Action=Place Revision=3 Members=3 Cell=X=-3 Y=-4 Z=0','CONSTRUCTION_NETWORK_RESULT Builder=1 Result=7 Revision=0 Members=0 Collision=0','CONSTRUCTION_NETWORK_RESULT Builder=1 Result=9 Revision=1 Members=1 Collision=1','CONSTRUCTION_NETWORK_RESULT Builder=1 Result=3 Revision=3 Members=3 Collision=1','CONSTRUCTION_NETWORK_RESULT Builder=1 Result=9 Revision=3 Members=3 Collision=1 Action=Remove','CONSTRUCTION_NETWORK_RESULT Builder=1 Result=11 Revision=4 Members=2 Collision=1 Action=Remove','CONSTRUCTION_NETWORK_RESULT Builder=1 Result=3 Revision=4 Members=2 Collision=1 Action=Remove','CONSTRUCTION_NETWORK_EDIT Action=Remove Revision=4 Members=2 Cell=X=-4 Y=-4 Z=1','CONSTRUCTION_NETWORK_EDIT Action=Remove Revision=5 Members=1 Cell=X=-3 Y=-4 Z=0','CONSTRUCTION_NETWORK_EDIT Action=Remove Revision=6 Members=0 Cell=X=-4 Y=-4 Z=0','CONSTRUCTION_NETWORK_RESULT Builder=0 Result=2 Revision=6 Members=0 Collision=1 Action=Place','CONSTRUCTION_NETWORK_RESULT Builder=0 Result=2 Revision=6 Members=0 Collision=1 Action=Remove','CONSTRUCTION_NETWORK_EDIT Action=Place Revision=7 Members=1 Cell=X=-4 Y=-4 Z=0','CONSTRUCTION_NETWORK_EDIT Action=Place Revision=8 Members=2 Cell=X=-4 Y=-5 Z=0')) {
        if ([regex]::Matches($serverLog,[regex]::Escape($marker)).Count -ne 1) { throw "Missing/duplicate authoritative observation: $marker" }
    }
    $join = $serverLog.IndexOf('CONSTRUCTION_NETWORK_PAIR_READY Players=2')
    $accept = $serverLog.IndexOf('CONSTRUCTION_NETWORK_EDIT')
    if ($join -lt 0 -or $accept -le $join -or [regex]::Matches($serverLog,'CONSTRUCTION_NETWORK_EDIT').Count -ne 10) { throw 'Ten structural edits were not accepted after both players joined.' }
    if ($serverLog -notmatch ('CONSTRUCTION_NETWORK_DOOR_REMOVE Revision=14 Members=2 Doors=0[^\r\n]*Content='+[regex]::Escape($removedPayloads[0]))) { throw 'Clients differ from server removed content.' }
    if ([regex]::Matches($serverLog,'CONSTRUCTION_NETWORK_RESULT Builder=1 Result=13 Revision=6 Members=0 Collision=1 Action=Place').Count -ne 2) { throw 'Invalid shape/rotation were not rejected unchanged.' }
    if ($serverLog -notmatch ('CONSTRUCTION_NETWORK_EDIT Action=Remove Revision=6 Members=0[^\r\n]*Content='+[regex]::Escape($emptyPayloads[0]))) { throw 'Empty client projection differs from server.' }
    if ($serverLog -notmatch 'CONSTRUCTION_NETWORK_EDIT Action=Place Revision=7 Members=1[^\r\n]*Shape=1 Rotation=' -or $serverLog -notmatch ('CONSTRUCTION_NETWORK_EDIT Action=Place Revision=8 Members=2[^\r\n]*Shape=2 Rotation='+$yawRotation+'(?:\s|$)')) { throw 'Server mixed shape/rotation differs from selected input.' }
    foreach ($marker in @('CONSTRUCTION_NETWORK_DOOR_RESULT Builder=0 Result=2 Revision=6 Doors=0 Collision=1','CONSTRUCTION_NETWORK_DOOR_RESULT Builder=1 Result=4 Revision=8 Doors=0 Collision=1','CONSTRUCTION_NETWORK_DOOR_PLACE Revision=9 Members=2 Doors=1 Cell=X=2 Y=0 Z=0 Yaw=0','CONSTRUCTION_NETWORK_RESULT Builder=1 Result=7 Revision=9 Members=2 Collision=1 Action=Place','CONSTRUCTION_NETWORK_EDIT Action=Place Revision=10 Members=3 Cell=X=-6 Y=-4 Z=0','CONSTRUCTION_NETWORK_EDIT Action=Remove Revision=11 Members=2 Cell=X=-6 Y=-4 Z=0')) {
        if ([regex]::Matches($serverLog,[regex]::Escape($marker)).Count -ne 1) { throw "Missing/duplicate door admission/reservation/retention observation: $marker" }
    }
    $doorView=0
    foreach ($role in @('driver','observer')) {
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $doorView += [regex]::Matches($log,'CONSTRUCTION_NETWORK_VIEW_DOOR Cell=X=2 Y=0 Z=0 Yaw=0').Count
    }
    if ($doorView -ne 1 -or [regex]::Matches($serverLog,'CONSTRUCTION_NETWORK_DOOR_PLACE').Count -ne 1) { throw 'Expected one server door from normal4/B input.' }
    foreach ($marker in @('CONSTRUCTION_NETWORK_TOGGLE_RESULT Builder=0 Result=2 Revision=11 Doors=1 Collision=1','CONSTRUCTION_NETWORK_DOOR_TOGGLE Revision=12 Members=2 Doors=1 Open=1','CONSTRUCTION_NETWORK_DOOR_OBSTRUCTION_READY Physics=1','CONSTRUCTION_NETWORK_TOGGLE_RESULT Builder=1 Result=14 Revision=12 Doors=1 Collision=1','CONSTRUCTION_NETWORK_DOOR_OBSTRUCTION_REMOVED','CONSTRUCTION_NETWORK_DOOR_TOGGLE Revision=13 Members=2 Doors=1 Open=0')) {
        if ([regex]::Matches($serverLog,[regex]::Escape($marker)).Count -ne 1) { throw "Missing/duplicate door motion observation: $marker" }
    }
    if ($serverLog -notmatch ('CONSTRUCTION_NETWORK_DOOR_TOGGLE Revision=12 Members=2 Doors=1 Open=1[^\r\n]*Content='+[regex]::Escape($openPayloads[0]))) { throw 'Server/open-client content differs.' }
    foreach ($marker in @('CONSTRUCTION_NETWORK_REMOVE_DOOR_RESULT Builder=0 Result=2 Revision=13 Doors=1 Collision=1','CONSTRUCTION_NETWORK_REMOVE_DOOR_RESULT Builder=1 Result=17 Revision=14 Doors=0 Collision=1')) {
        if ([regex]::Matches($serverLog,[regex]::Escape($marker)).Count -ne 1) { throw "Missing/duplicate door removal observation: $marker" }
    }
    $doorRemovals=0
    foreach ($role in @('driver','observer')) {
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $doorRemovals += [regex]::Matches($log,'CONSTRUCTION_NETWORK_VIEW_REMOVE_DOOR Cell=X=2 Y=0 Z=0').Count
    }
    if ($doorRemovals -ne 1 -or [regex]::Matches($serverLog,'CONSTRUCTION_NETWORK_DOOR_REMOVE Revision=').Count -ne 1) { throw 'Expected one ordinary demolition-mode LMB request and one committed door removal.' }
    $toggleViews=0
    foreach ($role in @('driver','observer')) {
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $toggleViews += [regex]::Matches($log,'CONSTRUCTION_NETWORK_VIEW_TOGGLE Cell=X=2 Y=0 Z=0').Count
    }
    if ($toggleViews -ne 3 -or [regex]::Matches($serverLog,'CONSTRUCTION_NETWORK_DOOR_TOGGLE').Count -ne 2) { throw 'Expected three ordinary E requests and only two committed pose changes.' }
    foreach ($role in @('driver','observer')) {
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        foreach ($pose in @(0,1)) {
            if ([regex]::Matches($log,('CONSTRUCTION_NETWORK_CAPSULE_PASSAGE Builder=[01] Open='+$pose+' BothDirections=1 RadiusCm=34.0 HalfHeightCm=88.0')).Count -ne 1) { throw "$role lacks actual capsule-size passage evidence for pose$pose." }
        }
    }
    $virtualTargets=0
    foreach ($role in @('driver','observer')) {
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $virtualTargets += [regex]::Matches($log,'CONSTRUCTION_NETWORK_VIRTUAL_DOOR_TARGET Revision=12 PhysicalDoorHit=0').Count
    }
    if ($virtualTargets -ne 1) { throw 'Open doorway targeting without a physical frame/leaf hit was not observed.' }
    foreach ($marker in @('CONSTRUCTION_NETWORK_LINE_RESULT Builder=0 Result=2 Revision=14 Members=2 Collision=1','CONSTRUCTION_NETWORK_LINE_RESULT Builder=1 Result=7 Revision=14 Members=2 Collision=1','CONSTRUCTION_NETWORK_LINE_RESULT Builder=1 Result=0 Revision=15 Members=5 Collision=1')) {
        if ([regex]::Matches($serverLog,[regex]::Escape($marker)).Count -ne 1) { throw "Missing/duplicate line observation: $marker" }
    }
    if ([regex]::Matches($serverLog,'CONSTRUCTION_NETWORK_LINE_RESULT Builder=1 Result=18 Revision=14 Members=2 Collision=1').Count -ne 2) { throw 'Diagonal/over-limit lines did not reject unchanged.' }
    if ([regex]::Matches($serverLog,'CONSTRUCTION_NETWORK_LINE Revision=15 Members=5 Doors=0 Cells=3').Count -ne 1 -or $serverLog -notmatch ('CONSTRUCTION_NETWORK_LINE Revision=15[^\r\n]*Content='+[regex]::Escape($baseLinePayloads[0]))) { throw 'Atomic three-cell line differs from client content.' }
    $lineViews=0; $lineSeeds=0
    foreach ($role in @('driver','observer')) {
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $lineViews += [regex]::Matches($log,'CONSTRUCTION_NETWORK_VIEW_LINE Revision=14 Start=X=5 Y=-4 Z=0 End=X=7 Y=-4 Z=0').Count
        $lineSeeds += [regex]::Matches($log,'CONSTRUCTION_NETWORK_LINE_SEED_ONLY Revision=14 Members=2 Collision=1').Count
    }
    if ($lineViews -ne 1 -or $lineSeeds -ne 1) { throw 'Ordinary L/B seed and one atomic line dispatch were not observed.' }
    $cursorChecks=0; $demolitionModes=0
    foreach ($role in @('driver','observer')) {
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $cursorChecks += [regex]::Matches($log,'CONSTRUCTION_NETWORK_CURSOR_INPUT CameraHold=1 BlockedWorldClick=1 CursorRestored=1 Revision=0').Count
        $demolitionModes += [regex]::Matches($log,'CONSTRUCTION_NETWORK_DEMOLITION_MODE Enabled=1').Count
    }
    if ($cursorChecks -ne 1 -or $demolitionModes -ne 2) { throw 'Cursor/RMB isolation or explicit block/door demolition mode was not observed.' }
    if ([regex]::Matches($serverLog,'CONSTRUCTION_NETWORK_LINE Revision=16 Members=8 Doors=0 Cells=3 Start=X=5 Y=-4 Z=1 End=X=7 Y=-4 Z=1').Count -ne 1 -or $serverLog -notmatch ('CONSTRUCTION_NETWORK_LINE Revision=16[^\r\n]*Content='+[regex]::Escape($payloads[0]))) { throw 'Upper slab row did not publish atomically with matching content.' }
    $layerViews=0; $layerSeeds=0
    foreach ($role in @('driver','observer')) {
        $log=Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $layerViews += [regex]::Matches($log,'CONSTRUCTION_NETWORK_VIEW_LINE Revision=15 Start=X=5 Y=-4 Z=1 End=X=7 Y=-4 Z=1').Count
        $layerSeeds += [regex]::Matches($log,'CONSTRUCTION_NETWORK_LAYER_SEED_ONLY Revision=15 Members=5 Layer=1').Count
    }
    if ($layerViews -ne 1 -or $layerSeeds -ne 1) { throw 'P/PageUp/B upper-layer seed and dispatch were not observed.' }
    $results.contentBlake3 = $payloads[0]
    $results.captureFrames = [bool]$CaptureFrames
    $results.status = 'Passed'
    Write-Output "PASS: two concurrent clients, construction input, retained door, actual E open/blocked-close/retry, both-client pose/collision agreement, explicit demolition-mode door removal without ghost colliders and scoped physics canary cleanup. Evidence: $root"
} catch {
    $results.status='Failed'; $results.error=$_.Exception.Message; throw
} finally {
    $complete=$true
    foreach ($process in $owned) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
        if (-not $process.WaitForExit(5000)) { $complete=$false }
    }
    $results.cleanupComplete=$complete
    $results.elapsedSeconds=$clock.Elapsed.TotalSeconds
    if (-not $complete) { $results.status='Failed'; $results.error='Owned cleanup incomplete.' }
    $results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding UTF8
    if (-not $complete) { throw $results.error }
}
