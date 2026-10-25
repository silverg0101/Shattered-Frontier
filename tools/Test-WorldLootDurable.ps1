[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'InventoryLedgerHostFixture.ps1')
$fx = @{}
$oldToolchain = $env:SF_FIXTURE_TOOLCHAIN_ROOT
try {
    if (-not (Test-Path (Join-Path $repo 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'))) { $env:SF_FIXTURE_TOOLCHAIN_ROOT = 'C:/Projects/Shattered-Frontier' }
    New-LedgerFixture $fx $repo $EvidenceRoot
    $project = Join-Path $repo 'BackendServices/InventoryLedger.Postgres.Tests/InventoryLedger.Postgres.Tests.csproj'
    $out = Join-Path $fx.base 'world-loot-tests'
    Push-Location (Join-Path $repo 'BackendServices')
    try {
        [void](Invoke-FxOk $fx $fx.dotnet @('restore',$project,'--locked-mode','--packages',$fx.nuget,'--verbosity','quiet'))
        [void](Invoke-FxOk $fx $fx.dotnet @('build',$project,'--no-restore','-c','Release','-o',$out,'--verbosity','quiet'))
    } finally { Pop-Location }
    $connection = "Host=127.0.0.1;Port=$($fx.pgPort);Database=$($fx.db);Username=ledger_svc;Password=$($fx.servicePassword);Pooling=false"
    $dll = Join-Path $out 'InventoryLedger.Postgres.Tests.dll'
    $seed = Invoke-FxOk $fx $fx.dotnet @($dll,'world-loot-seed') @{ INVENTORY_LEDGER_CONNECTION=$connection }
    [IO.File]::WriteAllText((Join-Path $EvidenceRoot 'seed.txt'),$seed)
    [void](Invoke-FxOk $fx (Join-Path $fx.pg 'pg_ctl.exe') @('-D',$fx.data,'-w','-m','immediate','stop'))
    [void](Invoke-FxOk $fx (Join-Path $fx.pg 'pg_ctl.exe') @('-D',$fx.data,'-l',(Join-Path $fx.base 'postgres.log'),'-w','-t','30','start'))
    $result = Invoke-FxOk $fx $fx.dotnet @($dll,'world-loot-recover') @{ INVENTORY_LEDGER_CONNECTION=$connection }
    [IO.File]::WriteAllText((Join-Path $EvidenceRoot 'recovery.txt'),$result)
    if ($result -notmatch 'WORLD_LOOT_DURABLE_PASS') { throw 'Missing world loot acceptance marker.' }
    Write-Output 'WORLD_LOOT_DURABLE_PASS restart replay concurrent-pickup exact-instance eligibility rollback'
} finally { Remove-LedgerFixture $fx; $env:SF_FIXTURE_TOOLCHAIN_ROOT = $oldToolchain }
