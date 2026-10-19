[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17791,
    [Parameter(HelpMessage='Maximum seconds per startup or exit phase.')][ValidateRange(60,150)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root = Join-Path $repoRoot ('Artifacts\ConstructionSandbox\network-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw 'Installed editor commandlet is missing.' }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object { $_.Port -in @($Port,($Port+2)) }).Count) { throw 'An owned test port is already in use.' }
if (Test-Path -LiteralPath $root) { throw 'Evidence root exists.' }
$null = New-Item -ItemType Directory -Path $root
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$results = [ordered]@{ status='Running'; engine=$editor; project=$project; port=$Port; runs=@(); scope='Isolated loopback editor dedicated-server, canonical cube assembly and rough-timber support, transient build capability; no production ownership/persistence/general procedural shape or packaged acceptance.' }
$clock = [Diagnostics.Stopwatch]::StartNew()
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra) {
    $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $log = Join-Path $root ($Name+'.log')
    $arguments = @(('"'+$project+'"'),$Url,'-NullRHI','-unattended','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt'))
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
    $url = '/Game/Frontier/Maps/Greenlands_ConstructionSandbox?game=/Script/ShatteredFrontier.SFConstructionNetworkGameMode'
    $disabled = Start-Owned 'flag-negative' $url @('-server',('-port='+($Port+2)),'-multihome=127.0.0.1','-testexit=CONSTRUCTION_NETWORK_DISABLED')
    Wait-Exit $disabled 'flag-negative'
    $disabledLog = Get-Content -LiteralPath (Join-Path $root 'flag-negative.log') -Raw
    if ($disabledLog -notmatch 'CONSTRUCTION_NETWORK_DISABLED' -or $disabledLog -match 'CONSTRUCTION_NETWORK_READY|CONSTRUCTION_NETWORK_ACCEPT') { throw 'Missing startup opt-in did not fence the network mode.' }
    $server = Start-Owned 'server' $url @('-server',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox')
    Wait-Log $server 'server' 'CONSTRUCTION_NETWORK_READY'
    $driver = Start-Owned 'driver' ('127.0.0.1:'+$Port) @('-game','-ConstructionNetworkScenario')
    $observer = Start-Owned 'observer' ('127.0.0.1:'+$Port) @('-game','-ConstructionNetworkScenario')
    Wait-Exit $driver 'driver'
    Wait-Exit $observer 'observer'
    if ($server.HasExited) { throw 'Owned server exited during the client exercise.' }
    $payloads = @()
    $observedRoles = @()
    foreach ($role in @('driver','observer')) {
        $log = Get-Content -LiteralPath (Join-Path $root ($role+'.log')) -Raw
        $success = [regex]::Matches($log,'CONSTRUCTION_NETWORK_SCENARIO_SUCCESS Builder=([01]) Revision=3 Members=3')
        if ($success.Count -ne 1 -or $log -match 'CONSTRUCTION_NETWORK_\w*FAILED|Fatal error:|Assertion failed:|Ensure condition failed:') { throw "$role failed its semantic scenario." }
        $observedRoles += [int]$success[0].Groups[1].Value
        $projection = [regex]::Matches($log,'CONSTRUCTION_NETWORK_PROJECTED Revision=3 Members=3 Collision=1 Content=([a-fA-F0-9]{64})')
        if ($projection.Count -ne 1) { throw "$role lacks a unique exact local collision projection." }
        $payloads += $projection[0].Groups[1].Value
    }
    if (@($observedRoles | Sort-Object -Unique).Count -ne 2) { throw 'Expected one server-assigned builder and one observer.' }
    if ($payloads[0] -ne $payloads[1]) { throw 'Clients projected different canonical payloads.' }
    $serverLog = Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
    foreach ($marker in @('CONSTRUCTION_NETWORK_ACCEPT Revision=1 Members=1 Cell=X=-4 Y=-4 Z=0','CONSTRUCTION_NETWORK_ACCEPT Revision=2 Members=2 Cell=X=-4 Y=-4 Z=1','CONSTRUCTION_NETWORK_ACCEPT Revision=3 Members=3 Cell=X=-3 Y=-4 Z=0','CONSTRUCTION_NETWORK_RESULT Builder=1 Result=7 Revision=0 Members=0 Collision=0','CONSTRUCTION_NETWORK_RESULT Builder=1 Result=9 Revision=1 Members=1 Collision=1','CONSTRUCTION_NETWORK_RESULT Builder=1 Result=3 Revision=3 Members=3 Collision=1','CONSTRUCTION_NETWORK_RESULT Builder=0 Result=2 Revision=3 Members=3 Collision=1')) {
        if ([regex]::Matches($serverLog,[regex]::Escape($marker)).Count -ne 1) { throw "Missing/duplicate authoritative observation: $marker" }
    }
    $join = $serverLog.IndexOf('CONSTRUCTION_NETWORK_PAIR_READY Players=2')
    $accept = $serverLog.IndexOf('CONSTRUCTION_NETWORK_ACCEPT')
    if ($join -lt 0 -or $accept -le $join -or [regex]::Matches($serverLog,'CONSTRUCTION_NETWORK_ACCEPT').Count -ne 3) { throw 'Three placements were not accepted after both players joined.' }
    if ($serverLog -notmatch ('CONSTRUCTION_NETWORK_ACCEPT Revision=3 Members=3[^\r\n]*Content='+[regex]::Escape($payloads[0]))) { throw 'Clients differ from server content.' }
    $results.contentBlake3 = $payloads[0]
    $results.status = 'Passed'
    Write-Output "PASS: two concurrent clients, supported three-cube assembly, matching collision, occupied/floating/stale/permission rejection. Evidence: $root"
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
