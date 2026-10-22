param([string]$Sdk,[switch]$TwoClient)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
if (-not $Sdk) { $Sdk = Join-Path $root 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe' }
if (-not (Test-Path -LiteralPath $Sdk)) { throw "Pinned backend SDK missing: $Sdk" }
& $Sdk run --project BackendServices/ClaimTerritory.Tests -- content/world/zones/greenlands_silverbrook.json
if ($LASTEXITCODE -ne 0) { throw 'ClaimTerritory.Tests failed' }
& py -3 tools/claim_bands.py 900 900
if ($LASTEXITCODE -ne 0) { throw 'claim_bands.py failed' }
if ($TwoClient) {
    & tools/Test-ClaimHousing.ps1 -StarterKit
    if ($LASTEXITCODE -ne 0) { throw 'Owned PostgreSQL two-client restart fixture failed' }
}
