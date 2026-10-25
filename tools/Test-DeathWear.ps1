[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'InventoryLedgerHostFixture.ps1')
$evidence = Join-Path $repo ('Artifacts/DeathWear/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $evidence -Force
$fx = @{}
try {
    New-LedgerFixture $fx $repo $evidence
    $project = Join-Path $repo 'BackendServices/InventoryLedger.Postgres.Tests/InventoryLedger.Postgres.Tests.csproj'
    $out = Join-Path $evidence 'private/test-build'
    [void](Invoke-FxOk $fx $fx.dotnet @('restore',$project,'--locked-mode','--packages',$fx.nuget,'--verbosity','quiet'))
    [void](Invoke-FxOk $fx $fx.dotnet @('build',$project,'--no-restore','--configuration','Release','--verbosity','quiet',"-p:OutDir=$out/"))
    $dll = Join-Path $out 'InventoryLedger.Postgres.Tests.dll'
    $envs = @{ INVENTORY_LEDGER_CONNECTION=$fx.connection }
    Invoke-FxOk $fx $fx.dotnet @($dll,'death-wear-seed') $envs
    [void](Invoke-FxOk $fx (Join-Path $fx.pg 'pg_ctl.exe') @('-D',$fx.data,'-m','immediate','-w','-t','30','stop'))
    $fx.started = $false
    [void](Invoke-FxOk $fx (Join-Path $fx.pg 'pg_ctl.exe') @('-D',$fx.data,'-l',(Join-Path $fx.base 'postgres-restart.log'),'-w','-t','30','start'))
    $fx.started = $true
    Invoke-FxOk $fx $fx.dotnet @($dll,'death-wear-restart') $envs
}
finally {
    Remove-LedgerFixture $fx
    if ($fx.dotnet -and (Test-Path -LiteralPath $fx.dotnet)) { & $fx.dotnet build-server shutdown | Out-Null }
}
