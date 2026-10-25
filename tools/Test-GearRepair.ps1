[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'InventoryLedgerHostFixture.ps1')
$evidence = Join-Path $repo ('Artifacts/GearRepair/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $evidence -Force
$fx = @{}; $gameplay = $null
try {
    if (-not (Test-Path (Join-Path $repo 'Artifacts/Persistence/Toolchains/postgresql-18.6/pgsql/bin/postgres.exe'))) {
        $env:SF_FIXTURE_TOOLCHAIN_ROOT = 'C:/Projects/Shattered-Frontier'
    }
    New-LedgerFixture $fx $repo $evidence
    $schemas = @('CharacterState.Postgres/Schema.sql','CharacterState.Postgres/Schema.Position.sql',
        'CharacterState.Postgres/Schema.Recipes.sql','CharacterState.Postgres/Schema.Appearance.sql',
        'CharacterState.Postgres/Schema.GatheringSkills.sql','CharacterState.Postgres/Schema.Death.sql',
        'CharacterState.Postgres/Schema.Rested.sql','Housing.Postgres/Schema.sql',
        'Exchange.Postgres/Schema.sql','Housing.Postgres/Schema.Gameplay.sql',
        'Housing.Postgres/Schema.Geometry.sql','Housing.Postgres/Schema.Construction.sql') |
        ForEach-Object { Get-Content (Join-Path $repo "BackendServices/$_") -Raw }
    [void](Invoke-LedgerSql $fx ("SET ROLE ledger_owner;`n" + ($schemas -join "`n")) $fx.db)
    Invoke-LedgerSchemaMigrations $fx @('inventory','character_state','housing','exchange')
    $grants = 'GRANT USAGE ON SCHEMA character_state,housing,exchange TO ledger_svc; ' +
        ((@('inventory','character_state','housing','exchange') | ForEach-Object {
            "GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA $_ TO ledger_svc;"
        }) -join ' ')
    [void](Invoke-LedgerSql $fx $grants $fx.db)
    $hostProject = Join-Path $repo 'BackendServices/Gameplay.Host/Gameplay.Host.csproj'
    $testProject = Join-Path $repo 'BackendServices/Gameplay.Host.Tests/Repair/Repair.Host.Tests.csproj'
    $hostOut = Join-Path $evidence 'private/gameplay-build'
    $testOut = Join-Path $evidence 'private/repair-tests'
    [void](Invoke-FxOk $fx $fx.dotnet @('restore',$testProject,'--use-lock-file','--packages',$fx.nuget,'--verbosity','quiet'))
    [void](Invoke-FxOk $fx $fx.dotnet @('build',$hostProject,'--no-restore','--configuration','Release','--verbosity','quiet','--output',$hostOut))
    [void](Invoke-FxOk $fx $fx.dotnet @('build',$testProject,'--no-restore','--configuration','Release','--verbosity','quiet','--output',$testOut))
    $port = Get-FreeTcpPort; $token = New-HexSecret; $fx.secrets += $token
    $envs = @{ GAMEPLAY_HOST_PORT="$port"; SF_GAMEPLAY_TOKEN=$token; GAMEPLAY_LEDGER_CONNECTION=$fx.connection;
        GAMEPLAY_CHARACTER_CONNECTION=$fx.connection; GAMEPLAY_HOUSING_CONNECTION=$fx.connection;
        GAMEPLAY_EXCHANGE_CONNECTION=$fx.connection; GAMEPLAY_CONTENT_ROOT=(Join-Path $repo 'content') }
    Invoke-WithEnv $envs { $script:gameplay = Start-Process -FilePath $fx.dotnet -ArgumentList @((Join-Path $hostOut 'Gameplay.Host.dll')) `
        -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $evidence 'gameplay.out') `
        -RedirectStandardError (Join-Path $evidence 'gameplay.err') }
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while (-not ((Test-Path (Join-Path $evidence 'gameplay.out')) -and
        (Get-Content (Join-Path $evidence 'gameplay.out') -Raw) -match 'GAMEPLAY_HOST_READY')) {
        if ($gameplay.HasExited -or $timer.Elapsed.TotalSeconds -gt 60) { throw 'Gameplay host did not become ready.' }
        Start-Sleep -Milliseconds 200
    }
    $testDll = Join-Path $testOut 'Repair.Host.Tests.dll'
    Invoke-FxOk $fx $fx.dotnet @($testDll,'gear-repair') $envs
    [void](Invoke-FxOk $fx (Join-Path $fx.pg 'pg_ctl.exe') @('-D',$fx.data,'-m','immediate','-w','-t','30','stop'))
    $fx.started = $false
    [void](Invoke-FxOk $fx (Join-Path $fx.pg 'pg_ctl.exe') @('-D',$fx.data,'-l',(Join-Path $fx.base 'postgres-restart.log'),'-w','-t','30','start'))
    $fx.started = $true
    Invoke-FxOk $fx $fx.dotnet @($testDll,'gear-repair-restart') $envs
}
finally {
    if ($gameplay -and -not $gameplay.HasExited) { Stop-Process -Id $gameplay.Id; $gameplay.WaitForExit() }
    Remove-LedgerFixture $fx
    if ($fx.dotnet -and (Test-Path -LiteralPath $fx.dotnet)) { & $fx.dotnet build-server shutdown | Out-Null }
    Remove-Item Env:SF_FIXTURE_TOOLCHAIN_ROOT -ErrorAction SilentlyContinue
}
