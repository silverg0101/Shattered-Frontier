param([string]$Sdk,[switch]$TwoClient,[string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
if (-not $Sdk) { $Sdk = Join-Path $root 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe' }
if (-not (Test-Path -LiteralPath $Sdk)) { throw "Pinned backend SDK missing: $Sdk" }
if (-not $EvidenceRoot) { $EvidenceRoot = Join-Path $root 'docs/evidence/clm01-claim-grid' }
if ($TwoClient) {
    $runRoot = Join-Path $EvidenceRoot ('run-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    $null = New-Item -ItemType Directory -Path $runRoot -Force
    $coverage = [ordered]@{
        scope = 'Owned PostgreSQL and two-client fixture; test 8 only partially covers legacy plot mask restore.'
        tests = [ordered]@{
            '2'='NotRun: sparse growth/reach scenario is not wired to the client runner.'
            '3'='NotRun: sparse point/cap scenario is not wired to the client runner.'
            '4'='NotRun: shrink/refund/adoption scenario is not wired to the client runner.'
            '5'='NotRun: blocked-cell reasons are not wired to the client runner.'
            '6'='NotRun: engulfment scenario is not wired to the client runner.'
            '7'='NotRun: three-claim slot scenario is not wired to the client runner.'
            '8'='Partial: legacy structure mask restore and two-client permission flow only; sparse mask, points and anchor assertions are absent.'
            '9'='NotRun: empty-claim archive scenario is not wired to the client runner.'
        }
    }
    [IO.File]::WriteAllText((Join-Path $runRoot 'coverage.json'), (($coverage | ConvertTo-Json -Depth 5) -replace "`r`n", "`n") + "`n", [Text.UTF8Encoding]::new($false))
}
& $Sdk run --project BackendServices/ClaimTerritory.Tests -- content/world/zones/greenlands_silverbrook.json
if ($LASTEXITCODE -ne 0) { throw 'ClaimTerritory.Tests failed' }
& py -3 tools/claim_bands.py 900 900
if ($LASTEXITCODE -ne 0) { throw 'claim_bands.py failed' }
if ($TwoClient) {
    & tools/Test-ClaimHousing.ps1 -StarterKit -EvidenceRoot $runRoot
    if ($LASTEXITCODE -ne 0) { throw 'Owned PostgreSQL two-client restart fixture failed' }
}
