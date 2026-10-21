[CmdletBinding()]
param(
    [switch]$Refining,
    [switch]$Silverbrook,  # the Silverbrook zone with nodes from its zone record (no refinery stations there)
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(12000,65000)][int]$Port = 17811,
    [ValidateRange(30,150)][int]$TimeoutSeconds = 90,
    [switch]$Ledger,  # persist grants through an owned PostgreSQL + InventoryLedger.Host (INT-03)
    [ValidateSet('none','drop_first_reply')][string]$Fault = 'none',
    [switch]$HostDown,  # server starts while the host is down; harvests fail Unavailable, then the host starts
    [switch]$ExitAfterCommit,  # server exits right after the first ledger commit; the row must survive
    [Parameter(DontShow)][switch]$FixtureSelfTest  # PostgreSQL + host bring-up/teardown only, no editor
)
$ErrorActionPreference = 'Stop'
if ($FixtureSelfTest) { $Ledger = [switch]$true }
if (-not $Ledger -and ($Fault -ne 'none' -or $HostDown -or $ExitAfterCommit)) { throw '-Fault, -HostDown and -ExitAfterCommit require -Ledger.' }
if ((@([bool]($Fault -ne 'none'),[bool]$HostDown,[bool]$ExitAfterCommit) | Where-Object { $_ }).Count -gt 1) { throw '-Fault, -HostDown and -ExitAfterCommit are mutually exclusive.' }
if ($Ledger -and ($Refining -or $Silverbrook)) { throw '-Ledger covers the sandbox gathering scenario only (no -Refining/-Silverbrook).' }
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$evidencePrefix = if ($Ledger) { 'gathering-ledger-' } else { 'gathering-network-' }
$root = Join-Path $repoRoot ('Artifacts\ConstructionSandbox\' + $evidencePrefix + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
if (-not $FixtureSelfTest -and -not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw 'Installed editor is missing.' }
if (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $Port).Count) { throw 'Test port is already in use.' }
if (Test-Path -LiteralPath $root) { throw 'Evidence root exists.' }
$null = New-Item -ItemType Directory -Path $root
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
$results = [ordered]@{ status='Running'; scope='Two editor clients; real owned RPCs and replication; synthetic targeting; session-only inventory; no durable/Linux/native-input acceptance'; runs=@() }
if ($Ledger) { . (Join-Path $PSScriptRoot 'InventoryLedgerHostFixture.ps1') }
$fx = @{}
function Start-Owned([string]$Name,[string]$Url,[string[]]$Extra,[hashtable]$Env = @{}) {
    $user = Join-Path $root ($Name+'-user')
    $null = New-Item -ItemType Directory -Path $user
    $log = Join-Path $root ($Name+'.log')
    $arguments = @(('"'+$project+'"'),$Url,'-unattended','-NullRHI','-nosound','-nop4',('-UserDir="'+$user+'/"'),('-abslog="'+$log+'"'))+$Extra
    foreach ($k in $Env.Keys) { Set-Item -Path "Env:$k" -Value $Env[$k] }  # set only around the launch so other children never inherit it
    try { $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $root ($Name+'.stdout.txt')) -RedirectStandardError (Join-Path $root ($Name+'.stderr.txt')) }
    finally { foreach ($k in $Env.Keys) { Remove-Item -Path "Env:$k" -ErrorAction SilentlyContinue } }
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
function Wait-LogCount([Diagnostics.Process]$Process,[string]$Name,[string]$Pattern,[int]$Count) {
    $phase = [Diagnostics.Stopwatch]::StartNew()
    $log = Join-Path $root ($Name+'.log')
    while ($phase.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ((Test-Path -LiteralPath $log) -and ([regex]::Matches((Get-Content -LiteralPath $log -Raw),$Pattern).Count -ge $Count)) { return }
        if ($Process.HasExited) { throw "$Name exited before $Count x $Pattern." }
        Start-Sleep -Milliseconds 200
    }
    throw "Timed out waiting for $Name ($Count x $Pattern)."
}
function Stop-Owned([Diagnostics.Process]$Process) {
    if (-not $Process.HasExited) { Stop-Process -Id $Process.Id; $Process.WaitForExit() }
    foreach ($run in $results.runs) { if ($run.pid -eq $Process.Id -and -not $run.Contains('exitCode')) { $run.stoppedByRunner=$true } }
}
function Wait-Exit([Diagnostics.Process]$Process,[string]$Name,[int]$Limit = $TimeoutSeconds) {
    $phase = [Diagnostics.Stopwatch]::StartNew()
    while (-not $Process.WaitForExit(200)) { if ($phase.Elapsed.TotalSeconds -gt $Limit) { throw "Timed out waiting for $Name." } }
    foreach ($run in $results.runs) { if ($run.name -eq $Name) { $run.exitCode=$Process.ExitCode } }
    if ($Process.ExitCode -ne 0) { throw "$Name exited $($Process.ExitCode)." }
}
# Starts one client pair and applies the finite-scenario and grant/replay/private-inventory assertions; returns the joined client logs.
function Invoke-ClientPair([string]$A,[string]$B) {
    $pa=Start-Owned $A ('127.0.0.1:'+$Port) @('-game',$scenarioFlag)
    $pb=Start-Owned $B ('127.0.0.1:'+$Port) @('-game',$scenarioFlag)
    Wait-Exit $pb $B; Wait-Exit $pa $A  # the observer finishes last; its log says why a pair stalls
    $clientText=@()
    foreach ($name in @($A,$B)) {
        $text=Get-Content -LiteralPath (Join-Path $root ($name+'.log')) -Raw
        if ($text -notmatch 'GATHERING_PILOT_CLIENT_DONE' -or $text -match 'GATHERING_PILOT_SCENARIO_FAILED|Fatal error:|Assertion failed:|Ensure condition failed:') { throw "$name did not pass the finite scenario." }
        $clientText+=$text
    }
    $all=$clientText -join "`n"
    if ($all -notmatch 'CLIENT_DONE Primary=1 Inventories=1 Wood=1 Ore=0 WoodRevision=1 OreRevision=1' -or
        $all -notmatch 'CLIENT_DONE Primary=0 Inventories=1 Wood=0 Ore=1 WoodRevision=1 OreRevision=1' -or
        $all -notmatch 'OBSERVER_PRIVATE_EMPTY Wood=0 Ore=0' -or $all -notmatch 'PRIMARY_REPLAY Operation=1') { throw 'Grant/replay/private inventory evidence incomplete (runner expects shipped pilot defaults).' }
    return $all
}
# -Ledger: the two committed containers read back through `host get` and the receipt table (plain, -Fault and -HostDown modes).
function Assert-LedgerPhase([string]$ServerText) {
    $ms=@([regex]::Matches($ServerText,'GATHERING_LEDGER_OUTCOME Op=\S+ Container=(\S+) Code=0 '))
    if ($ms.Count -ne 2) { throw "Expected exactly 2 ledger OUTCOME Code=0 lines, saw $($ms.Count)." }
    $ids=@($ms | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    if ($ids.Count -ne 2) { throw 'The two committed OUTCOME lines name the same container.' }
    $views=@($ids | ForEach-Object { Get-LedgerView $fx $_ })
    foreach ($v in $views) { if ($v.exitCode -ne 0 -or $v.revision -ne 1 -or $v.items.Count -ne 1) { throw "Container $($v.container) view unexpected: $($v.line)" } }
    if (@($views | Where-Object { $_.items['item/oak_log'] -eq 1 }).Count -ne 1 -or @($views | Where-Object { $_.items['item/iron_ore'] -eq 1 }).Count -ne 1) { throw 'Expected one container with item/oak_log=1 and one with item/iron_ore=1.' }
    $receipts=Get-ReceiptCount $fx
    if ($receipts -ne 2) { throw "Expected 2 operation receipts, found $receipts." }
    if ($Fault -eq 'drop_first_reply') {
        if ([regex]::Matches((Get-LedgerHostText $fx),'INVENTORY_LEDGER_FAULT_INJECTED').Count -ne 1) { throw 'Expected exactly one injected fault.' }
        if ([regex]::Matches($ServerText,'GATHERING_LEDGER_RETRY').Count -lt 1) { throw 'Server never retried after the dropped reply.' }
    }
    $results.containerViews=$views; $results.receiptCount=$receipts
    $results.phases.ledgerAssertions='Passed'
}
try {
    if ($Silverbrook -and $Refining) { throw 'Silverbrook has no refinery stations; run -Refining in the sandbox.' }
    $map=if ($Silverbrook) { '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook' } else { '/Game/Frontier/Maps/Greenlands_ConstructionSandbox' }
    $url=$map+'?game=/Script/ShatteredFrontier.SFGameMode'
    $serverFlags=@('-server',('-port='+$Port),'-multihome=127.0.0.1','-ConstructionNetworkSandbox','-GatheringPilot')
    if ($Silverbrook) { $serverFlags+='-GatheringPilotScenario' }  # the server puts each test pawn beside its node
    $serverEnv=@{}
    if ($Ledger) {
        New-LedgerFixture $fx $repoRoot $root
        $fx.hostPort=Get-FreeTcpPort
        $results.scope='Development loopback fixture: Win64 editor -server (not a packaged Server target) + editor clients, owned PostgreSQL 18.6, InventoryLedger.Host over cleartext gRPC with a per-run dev bearer token; gathering grants only; node charges not durable; no Linux/TLS/native-input/production acceptance'
        $results.ledger=$true; $results.fault=$Fault; $results.hostDown=[bool]$HostDown; $results.exitAfterCommit=[bool]$ExitAfterCommit
        $results.phases=[ordered]@{}
        if ($FixtureSelfTest) {
            Start-LedgerHost $fx ''
            $probe=Get-LedgerView $fx 'ctn_selftest_unknown'
            if ($probe.exitCode -ne 3 -or $probe.line -notmatch '^INVENTORY_LEDGER_VIEW_FAILED code=NotFound$') { throw "get on an unknown container returned exit $($probe.exitCode): $($probe.line)" }
            $results.selfTest=[ordered]@{ unknownContainerExit=$probe.exitCode; line=$probe.line }
            $results.phases.fixtureSelfTest='Passed'; $results.status='Passed'
            return
        }
        $ledgerIni='-ini:Game:[/Script/ShatteredFrontier.SFInventoryLedgerSettings]'
        $serverFlags+=@(($ledgerIni+':bEnabled=True'),($ledgerIni+':Endpoint=127.0.0.1:'+$fx.hostPort))
        if ($ExitAfterCommit) { $serverFlags+='-GatheringLedgerExitAfterCommit' }
        $serverEnv=@{ SF_INVENTORY_LEDGER_TOKEN=$fx.token }
        if (-not $HostDown) { $hostFault=if ($Fault -eq 'drop_first_reply') { 'drop_first_reply' } else { '' }; Start-LedgerHost $fx $hostFault }
        if (-not $HostDown -and $Fault -eq 'none' -and -not $ExitAfterCommit) {
            # Step 0: gRPC round trip + deadline case from an editor -server automation run, before the scenario.
            $rtPort=$Port+1
            while (@([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveUdpListeners() | Where-Object Port -eq $rtPort).Count) { $rtPort++ }
            $rt=Start-Owned 'grpc-roundtrip' $url @('-server',('-port='+$rtPort),'-multihome=127.0.0.1','-ExecCmds="Automation RunTests ShatteredFrontier.Backend.Grpc"','-TestExit="Automation Test Queue Empty"') @{ SF_INVENTORY_LEDGER_TEST_ENDPOINT=('127.0.0.1:'+$fx.hostPort); SF_INVENTORY_LEDGER_TOKEN=$fx.token }
            Wait-Exit $rt 'grpc-roundtrip' 300
            $rtText=Get-Content -LiteralPath (Join-Path $root 'grpc-roundtrip.log') -Raw
            if ($rtText -notmatch 'GRPC_ROUNDTRIP_OK' -or $rtText -notmatch 'GRPC_DEADLINE_EXCEEDED_OK' -or $rtText -match 'GRPC_ROUNDTRIP_FAILED' -or
                $rtText -notmatch 'Test Completed\. Result=\{Success\}' -or $rtText -match 'Test Completed\. Result=\{(?!Success\})\w+\}' -or $rtText -match 'Fatal error:') { throw 'gRPC round-trip automation did not pass (see grpc-roundtrip.log).' }
            $results.phases.grpcRoundTrip='Passed'
            $results.expectedExtraState='ctn_dev_grpc_roundtrip (round-trip container; no entries, no receipts)'
        }
    }
    $server=Start-Owned 'server' $url $serverFlags $serverEnv
    Wait-Log $server 'server' 'GATHERING_PILOT_READY'
    $scenarioFlag=if ($Refining) { '-GatheringRefiningScenario' } else { '-GatheringPilotScenario' }
    if ($ExitAfterCommit) {
        $first=Start-Owned 'first' ('127.0.0.1:'+$Port) @('-game',$scenarioFlag)
        $second=Start-Owned 'second' ('127.0.0.1:'+$Port) @('-game',$scenarioFlag)
        Wait-Exit $server 'server'  # exit code 0 after the first ledger commit
        Stop-Owned $first; Stop-Owned $second  # the clients lose the server; failing is expected
        $serverText=Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
        if ($serverText -notmatch 'GATHERING_LEDGER_EXIT_AFTER_COMMIT Op=') { throw 'Server exited without the EXIT_AFTER_COMMIT marker.' }
        $ms=@([regex]::Matches($serverText,'GATHERING_LEDGER_OUTCOME Op=\S+ Container=(\S+) Code=0 '))
        if ($ms.Count -lt 1) { throw 'No committed ledger OUTCOME before the server exit.' }
        $ids=@($ms | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
        $views=@($ids | ForEach-Object { Get-LedgerView $fx $_ })
        foreach ($v in $views) {
            if ($v.exitCode -ne 0 -or $v.items.Count -ne 1 -or @($v.items.Values)[0] -ne 1) { throw "Committed container $($v.container) did not survive with quantity 1: $($v.line)" }
        }
        $receipts=Get-ReceiptCount $fx
        if ($receipts -lt 1 -or $receipts -ne $ms.Count) { throw "Receipt count $receipts does not match $($ms.Count) committed OUTCOME lines." }
        $results.containerViews=$views; $results.receiptCount=$receipts; $results.phases.exitAfterCommit='Passed'
        $results.map=$map; $results.status='Passed'
        return
    }
    if ($HostDown) {
        $first=Start-Owned 'first' ('127.0.0.1:'+$Port) @('-game',$scenarioFlag)
        $second=Start-Owned 'second' ('127.0.0.1:'+$Port) @('-game',$scenarioFlag)
        # The observer waits for the primary's grant, so only the primary's harvest reaches the ledger here.
        Wait-LogCount $server 'server' 'GATHERING_PILOT_OUTCOME Operation=1 Result=13' 1
        Stop-Owned $first; Stop-Owned $second; Stop-Owned $server  # never reach CLIENT_DONE; expected
        $downText=Get-Content -LiteralPath (Join-Path $root 'server.log') -Raw
        # Unavailable after MaxAttempts (3 shipped): two retries, no charge consumed, node revision unchanged, no receipt.
        if ($downText -notmatch 'GATHERING_LEDGER_OUTCOME Op=gather/\S+ Container=\S+ Code=14 Revision=0 Delta=0 NodeRevision=0 Charges=3' -or
            [regex]::Matches($downText,'GATHERING_LEDGER_RETRY ').Count -ne 2 -or (Get-ReceiptCount $fx) -ne 0) { throw 'Host-down outcome is not Unavailable with an unchanged node and no receipt.' }
        $results.phases.hostDownUnavailable='Passed'
        # The construction mode assigns the primary role once per server, so recovery uses a fresh server on the same ledger.
        Start-LedgerHost $fx ''
        $server=Start-Owned 'server2' $url $serverFlags $serverEnv
        Wait-Log $server 'server2' 'GATHERING_PILOT_READY'
        $serverLog='server2.log'
        $all=Invoke-ClientPair 'first2' 'second2'
    } else { $all=Invoke-ClientPair 'first' 'second' }
    if (-not $serverLog) { $serverLog='server.log' }
    $serverText=Get-Content -LiteralPath (Join-Path $root $serverLog) -Raw
    if ([regex]::Matches($serverText,'GATHERING_PILOT_OUTCOME Operation=1 Result=2').Count -ne 2) { throw 'Server did not commit exactly two harvests.' }
    if ($Refining) {
        if ($all -notmatch 'REFINING_PILOT_CLIENT_DONE Primary=1 Wood=0 Ore=0 Timber=4 Ingots=0 SawmillRevision=1 ForgeRevision=1' -or
            $all -notmatch 'REFINING_PILOT_CLIENT_DONE Primary=0 Wood=0 Ore=0 Timber=0 Ingots=1 SawmillRevision=1 ForgeRevision=1' -or
            [regex]::Matches($serverText,'GATHERING_PILOT_OUTCOME Operation=2 Result=15').Count -ne 2) { throw 'Refinement outputs or exactly-once commits are incomplete.' }
    }
    $results.refining=[bool]$Refining
    $results.map=$map
    if ($Silverbrook -and $serverText -notmatch 'GATHERING_PILOT_READY Source=ZoneRecord Nodes=([1-9]\d*)') { throw 'Silverbrook nodes did not come from the zone record.' }
    if ($Ledger) { Assert-LedgerPhase $serverText }
    $results.status='Passed'
} catch { $results.status='Failed'; $results.error=$_.Exception.Message; throw }
finally {
    foreach ($process in $owned) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit(); }
        foreach ($run in $results.runs) { if ($run.pid -eq $process.Id -and -not $run.Contains('exitCode')) { $run.stoppedByRunner=$true } }
    }
    if ($Ledger) {
        try { Remove-LedgerFixture $fx } catch { $fx.cleanupFailure = "$($fx.cleanupFailure) $($_.Exception.Message)" }
        $results.ledgerCleanup=[ordered]@{ privateStorageRemoved=$fx.privateStorageRemoved; failure=$fx.cleanupFailure }
        if ($results.status -eq 'Passed' -and ($fx.cleanupFailure -or -not $fx.privateStorageRemoved)) { $results.status='Failed'; $results.error='Ledger fixture cleanup failed.' }
    }
    $results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'results.json') -Encoding utf8
    Write-Output "Evidence: $root | $($results.status)"
}
