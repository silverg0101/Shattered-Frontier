[CmdletBinding()]
param(
    # Gathering: two clients harvest in Silverbrook; Combat: one client kills a boar (quest started at the boar step).
    [Parameter(Mandatory)][ValidateSet('Gathering','Combat')][string]$Leg,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17871,
    [ValidateRange(60,600)][int]$TimeoutSeconds = 240
)
# The gathering and combat pilots run in different game modes, so each leg proves its own events reach the quest
# (real server-side harvest/boar outcomes, no synthetic events) and the owning client's tracker view follows.
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repo 'GameClient\ShatteredFrontier.uproject'
foreach ($required in @($editor,$project)) { if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required input missing: $required" } }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) { throw 'Test port is already in use.' }
$root = Join-Path $repo ('Artifacts\QuestPilot\' + $Leg.ToLower() + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $root
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$result = [ordered]@{ status='Running'; leg=$Leg; runs=@() }
$silverbrook = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra) {
    $log = Join-Path $root ($Name+'.log'); $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $arguments = @(('"'+$project+'"'),$Url,'-unattended','-NullRHI','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt'))
    $null = $process.Handle; $owned.Add($process)
    $result.runs += [ordered]@{ name=$Name; pid=$process.Id; log=$log; arguments=$arguments }
    return $process
}
function Wait-Log([Diagnostics.Process]$Process,[string]$Name,[string]$Pattern) {
    $watch = [Diagnostics.Stopwatch]::StartNew(); $log = Join-Path $root ($Name+'.log')
    while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern $Pattern -Quiet)) { return }
        if ($Process.HasExited) { throw "$Name exited before $Pattern." }
        Start-Sleep -Milliseconds 300
    }
    throw "Timed out waiting for $Name ($Pattern)."
}
function Read-Log([string]$Name) { Get-Content -LiteralPath (Join-Path $root ($Name+'.log')) -Raw }
try {
    if ($Leg -eq 'Gathering') {
        $url = $silverbrook + '?game=/Script/ShatteredFrontier.SFConstructionNetworkGameMode'
        $server = Start-Owned 'server' $url @('-server',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox','-GatheringPilot','-GatheringPilotScenario','-QuestPilot')
        Wait-Log $server 'server' 'QUEST_PILOT_READY Quest=quest/frontier_founder Steps=5'
        $first = Start-Owned 'first' ('127.0.0.1:'+$Port) @('-game','-GatheringPilotScenario')
        $second = Start-Owned 'second' ('127.0.0.1:'+$Port) @('-game','-GatheringPilotScenario')
        Wait-Log $server 'server' 'QUEST_PILOT_EVENT Kind=gather Target=item/oak_log .* Result=0 Step=1 '
        Wait-Log $server 'server' 'QUEST_PILOT_EVENT Kind=gather Target=item/iron_ore .* Result=3 Step=0 '  # out of order: ignored, never credited
        # Only the owner of the oak harvest sees its tracker move on; the other client's view must stay at step 1.
        $watch = [Diagnostics.Stopwatch]::StartNew(); $moved = @()
        while ($watch.Elapsed.TotalSeconds -lt $TimeoutSeconds -and $moved.Count -lt 1) {
            $moved = @('first','second' | Where-Object { (Test-Path (Join-Path $root "$_.log")) -and (Read-Log $_) -match 'QUEST_PILOT_CLIENT_VIEW Step=2' })
            Start-Sleep -Milliseconds 300
        }
        if ($moved.Count -ne 1) { throw "Expected exactly one client tracker at step 2, got $($moved.Count)." }
    } else {
        $url = $silverbrook + '?game=/Script/ShatteredFrontier.SFCombatPilotGameMode'
        $server = Start-Owned 'server' $url @('-server',('-port='+$Port),'-multihome=127.0.0.1','-CombatPilot','-CombatPilotScenario','-QuestPilot','-QuestPilotStartStep=defeat_boar')
        Wait-Log $server 'server' 'QUEST_PILOT_READY Quest=quest/frontier_founder Steps=5'
        $client = Start-Owned 'client' ('127.0.0.1:'+$Port) @('-game','-CombatPilotScenario')
        Wait-Log $server 'server' 'QUEST_PILOT_EVENT Kind=defeat Target=npc/boar Op=boar:boar_farm_edge:0:\d+ Result=0'
        Wait-Log $server 'server' 'QUEST_PILOT_COMPLETE Quest=quest/frontier_founder'
        Wait-Log $server 'server' 'QUEST_PILOT_REWARD Bundle=reward/frontier_founder_completion'
        Wait-Log $client 'client' 'QUEST_PILOT_CLIENT_VIEW Step=5 Complete=1'
        Start-Sleep -Seconds 5  # a replayed or second credit would show up now
        $text = Read-Log 'server'
        if ([regex]::Matches($text,'QUEST_PILOT_REWARD Bundle=').Count -ne 1) { throw 'Chain reward was not granted exactly once.' }
    }
    $text = Read-Log 'server'
    if ($text -match 'QUEST_PILOT_DISABLED|Fatal error:|Assertion failed:') { throw 'Quest pilot disabled or server failed.' }
    $result.status = 'Passed'
} catch { $result.status = 'Failed'; $result.error = $_.Exception.Message; throw }
finally {
    foreach ($process in $owned) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
        foreach ($run in $result.runs) { if ($run.pid -eq $process.Id) { $run.exitCode = $process.ExitCode } }
    }
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($result.status)"
}
