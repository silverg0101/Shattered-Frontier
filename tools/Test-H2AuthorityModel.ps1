[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowed = Join-Path $repo 'Artifacts/Persistence/AuthorityModel'
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path -LiteralPath $EvidenceRoot)) { throw 'Supply a new child of Artifacts/Persistence/AuthorityModel.' }
$pins = Get-Content -LiteralPath (Join-Path $repo 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
$dotnet = Join-Path $repo 'Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
if ((Get-FileHash -LiteralPath $dotnet -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.sdk.executableSha256) {
    throw 'Prepare the pinned H2 SDK first.'
}
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
$env:DOTNET_GENERATE_ASPNET_CERTIFICATE = 'false'
$env:DOTNET_CLI_UI_LANGUAGE = 'en'
Push-Location (Join-Path $repo 'BackendServices')
try {
    $project = 'fixtures/AuthorityModel/AuthorityModel.csproj'
    $buildRoot = Join-Path $EvidenceRoot 'build'
    $restore = & $dotnet restore $project --locked-mode --packages (Join-Path $repo 'Artifacts/Persistence/Toolchains/nuget') --artifacts-path $buildRoot --verbosity quiet 2>&1
    $code = $LASTEXITCODE
    $restore | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'restore.txt') -Encoding utf8
    if ($code) { throw 'Locked model restore failed.' }
    $build = & $dotnet build $project --no-restore --configuration Release --artifacts-path $buildRoot --verbosity quiet 2>&1
    $code = $LASTEXITCODE
    $build | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'build.txt') -Encoding utf8
    if ($code) { throw 'Model build failed.' }
    $binary = Join-Path $buildRoot 'bin/AuthorityModel/release/AuthorityModel.dll'
    $raw = & $dotnet $binary 2>&1
    $code = $LASTEXITCODE
    $raw | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'model-output.txt') -Encoding utf8
    if ($code) { throw 'Authority model failed; inspect model-output.txt.' }
    $result = ($raw -join "`n") | ConvertFrom-Json
    if (-not $result.passed -or @($result.results | Where-Object { -not $_.passed }).Count) { throw 'A model case failed.' }
    $result | Add-Member -NotePropertyName sourceSha256 -NotePropertyValue (Get-FileHash fixtures/AuthorityModel/Program.cs -Algorithm SHA256).Hash.ToLowerInvariant()
    $result | Add-Member -NotePropertyName binarySha256 -NotePropertyValue (Get-FileHash $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    $result | Add-Member -NotePropertyName completedUtc -NotePropertyValue ([DateTimeOffset]::UtcNow.ToString('o'))
    $result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'results.json') -Encoding utf8
    "Authority model: $($result.cases) cases passed; $($result.schedules) bounded schedules. Evidence: $EvidenceRoot"
}
finally { Pop-Location }
