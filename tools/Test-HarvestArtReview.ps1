[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$CompareNative,
    [switch]$Unlit,
    [ValidateRange(12000,65000)][int]$Port = 17821,
    [ValidateRange(30,180)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root = Join-Path $repoRoot ('Artifacts\ConstructionSandbox\harvest-art-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw 'Installed editor is missing.' }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) { throw 'Test port is already in use.' }
if (Test-Path -LiteralPath $root) { throw 'Evidence root exists.' }
$null = New-Item -ItemType Directory -Path $root
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$results = [ordered]@{ status='Running'; scope='One rendered editor client, real owned gathering RPCs, procedural rock cuts and transient cosmetic debris; collection only, no visual approval, manual/native-input, packaged, durable or Linux acceptance'; runs=@(); frames=@() }
$clock = [Diagnostics.Stopwatch]::StartNew()
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra) {
    $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $log = Join-Path $root ($Name+'.log')
    $arguments = @(('"'+$project+'"'),$Url,'-unattended','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt'))
    $null = $process.Handle  # PS 5.1 keeps ExitCode only if the handle is opened before exit
    $owned.Add($process)
    $results.runs += [ordered]@{ name=$Name; pid=$process.Id; arguments=$arguments; log=$log }
    return $process
}
try {
    $server=Start-Owned 'server' '/Game/Frontier/Maps/Greenlands_ConstructionSandbox?game=/Script/ShatteredFrontier.SFGameMode' @('-server','-NullRHI',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox','-GatheringPilot')
    while (-not ((Test-Path -LiteralPath (Join-Path $root 'server.log')) -and (Select-String -LiteralPath (Join-Path $root 'server.log') -Pattern 'GATHERING_PILOT_READY' -Quiet))) {
        if ($server.HasExited -or $clock.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Owned server failed to become ready within the total deadline.' }
        Start-Sleep -Milliseconds 200
    }
    [string[]]$comparisonArgs=@()
    if ($CompareNative) { $comparisonArgs+=@('-HarvestArtCompareNative') }
    $results.nativeMaterialComparison=[bool]$CompareNative
    $results.unlitMaterialDiagnostic=[bool]$Unlit
    if ($Unlit) { $comparisonArgs+=@('-HarvestArtUnlit') }
    $client=Start-Owned 'client' ('127.0.0.1:'+$Port) (@('-game','-HarvestArtReview',('-HarvestArtReviewOutput="'+$root+'"'),'-RenderOffscreen','-windowed','-ResX=1280','-ResY=720')+$comparisonArgs)
    while (-not $client.WaitForExit(200)) {
        if ($server.HasExited) { throw 'Owned server exited during review.' }
        if ($clock.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Rendered review exceeded total deadline.' }
    }
    $results.runs[1].exitCode=$client.ExitCode
    if ($client.ExitCode -ne 0) { throw "Rendered client exited $($client.ExitCode)." }
    $text=Get-Content -LiteralPath (Join-Path $root 'client.log') -Raw
    $serverText=Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
    if ($text -notmatch 'HARVEST_ART_RESOURCES_READY Materials=\d+ Textures=\d+' -or $text -notmatch 'HARVEST_ART_DONE Ore=3 Cuts=3 Revision=3 Charges=0 Captures=4 ReplayUnchanged=1' -or
        ($text+$serverText) -match 'HARVEST_ART_FAILED|Fatal error:|Assertion failed:|Ensure condition failed:') { throw 'Rendered review did not complete its semantic checks.' }
    if ([regex]::Matches($serverText,'GATHERING_PILOT_OUTCOME Operation=[123] Result=2\b').Count -ne 3) { throw 'Server did not grant exactly three operations.' }
    $hashes=@()
    foreach ($cut in 0..3) {
        $state=[regex]::Match($text,('HARVEST_ART_STATE Cuts='+$cut+' Ore='+$cut+' Revision='+$cut+' Charges='+(3-$cut)+' MeshHash=(\d+) Vertices=\d+ Triangles=\d+ Debris=(\d+)'))
        if (-not $state.Success) { throw "Missing actual mesh/state evidence for cut $cut." }
        if ($cut -gt 0 -and [int]$state.Groups[2].Value -le 0) { throw "Missing live cosmetic fragments at cut $cut." }
        $hashes+=$state.Groups[1].Value
        $path=Join-Path $root ('rock-cut-'+$cut+'.png')
        $frame=[Drawing.Image]::FromFile($path)
        try {
            if ($frame.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid -or $frame.Width -ne 1280 -or $frame.Height -ne 720) { throw "Cut $cut capture is not 1280x720 PNG." }
        } finally { $frame.Dispose() }
        $results.frames += [ordered]@{ cut=$cut; path=$path; meshHash=$state.Groups[1].Value; sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    }
    if (@($hashes | Select-Object -Unique).Count -ne 4) { throw 'Each captured cut must change actual rendered mesh vertices.' }
    $results.status='Passed'
} catch { $results.status='Failed'; $results.error=$_.Exception.Message; throw }
finally {
    foreach ($process in $owned) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
        foreach ($run in $results.runs) { if ($run.pid -eq $process.Id -and -not $run.Contains('exitCode')) { $run.stoppedByRunner=$true } }
    }
    $results.cleanupComplete=@($owned | Where-Object { -not $_.HasExited }).Count -eq 0
    $results.elapsedSeconds=$clock.Elapsed.TotalSeconds
    $results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($results.status)"
}
