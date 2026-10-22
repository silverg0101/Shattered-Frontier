[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Level','Slope')][string]$Ground,
    [Parameter(Mandatory)][string]$OutputDirectory,   # absolute, must not exist yet
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17823,
    [ValidateRange(60,300)][int]$TimeoutSeconds = 220,
    [string[]]$ExtraArgs = @()   # appended to server and client, e.g. diagnostic console variables
)
# Rendered felling review: dedicated server (felling opt-in) + one 1920x1080 client on /Game/Frontier/Maps/Dev/TreeFelling/.
# The client (-HarvestFellingReview) makes three owned strikes and captures lean frames, rest and stump/log close-ups.
# Collection only: a fresh reviewer judges the frames (docs/visual-review.md).
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root = [IO.Path]::GetFullPath($OutputDirectory)
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
$map = "/Game/Frontier/Maps/Dev/TreeFelling/TreeFelling_$Ground"
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw 'Installed editor is missing.' }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) { throw 'Test port is already in use.' }
if (Test-Path -LiteralPath $root) { throw 'Evidence directory exists.' }
$null = New-Item -ItemType Directory -Path $root
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$results = [ordered]@{ status='Running'; ground=$Ground; map=$map; runs=@(); frames=@() }
$clock = [Diagnostics.Stopwatch]::StartNew()
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra) {
    $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $log = Join-Path $root ($Name+'.log')
    $arguments = @(('"'+$project+'"'),$Url,'-unattended','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt'))
    $null = $process.Handle
    $owned.Add($process)
    $results.runs += [ordered]@{ name=$Name; pid=$process.Id; log=$log }
    return $process
}
try {
    $server=Start-Owned 'server' ($map+'?game=/Script/ShatteredFrontier.SFGameMode') (@('-server','-NullRHI',('-port='+$Port),'-multihome=127.0.0.1','-ini:Game:[/Script/ShatteredFrontier.SFHarvestTreeFallSettings]:bEnablePreparedOakFelling=True')+$ExtraArgs)
    while (-not ((Test-Path -LiteralPath (Join-Path $root 'server.log')) -and (Select-String -LiteralPath (Join-Path $root 'server.log') -Pattern 'CONSTRUCTION_NETWORK_READY' -Quiet))) {
        if ($server.HasExited -or $clock.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Owned server failed to become ready.' }
        Start-Sleep -Milliseconds 200
    }
    $client=Start-Owned 'client' ('127.0.0.1:'+$Port) (@('-game','-HarvestFellingReview',('-HarvestFellingReviewOutput="'+$root+'"'),'-RenderOffscreen','-windowed','-ResX=1920','-ResY=1080')+$ExtraArgs)
    while (-not $client.WaitForExit(200)) {
        if ($server.HasExited) { throw 'Owned server exited during review.' }
        if ($clock.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Rendered review exceeded total deadline.' }
    }
    $results.runs[1].exitCode=$client.ExitCode
    $text=Get-Content -LiteralPath (Join-Path $root 'client.log') -Raw
    $serverText=Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
    $results.serverFall=@([regex]::Matches($serverText,'HARVEST_TREE_FALL_(STARTED|RESTING)[^\r\n]*') | ForEach-Object Value)
    $results.rest=([regex]::Match($text,'HARVEST_FELLING_REST [^\r\n]*')).Value
    if ($client.ExitCode -ne 0 -or $text -notmatch 'HARVEST_FELLING_REVIEW_DONE' -or ($text+$serverText) -match 'HARVEST_FELLING_REVIEW_FAILED|Fatal error:|Assertion failed:|Ensure condition failed:') { throw "Felling review client did not complete (exit $($client.ExitCode))." }
    if ([regex]::Matches($serverText,'GATHERING_PILOT_OUTCOME Operation=\d+ Result=2\b').Count -ne 3) { throw 'Server did not grant exactly three operations.' }
    foreach ($m in [regex]::Matches($text,'HARVEST_FELLING_SHOT Name=(\S+)[^\r\n]*')) {
        $path=Join-Path $root ($m.Groups[1].Value+'.png')
        $frame=[Drawing.Image]::FromFile($path)
        try { if ($frame.Width -ne 1920 -or $frame.Height -ne 1080) { throw "$path is not 1920x1080." } } finally { $frame.Dispose() }
        $results.frames += [ordered]@{ path=$path; log=$m.Value }
    }
    $results.status='Passed'
} catch { $results.status='Failed'; $results.error=$_.Exception.Message; throw }
finally {
    foreach ($process in $owned) { if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() } }
    $results.elapsedSeconds=$clock.Elapsed.TotalSeconds
    $results | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($results.status)"
}
