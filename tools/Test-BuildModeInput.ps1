[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$EvidenceRoot,
    [ValidateRange(60,300)][int]$TimeoutSeconds = 150
)
# GM-01 build-mode review run: -game on /Game/Frontier/Maps/Dev/BuildMode/BuildMode_Review, every mapped action injected by
# tools/review_build_mode_in_game.py, then the log is checked for each INPUT_ACTION line (and the pre-B negative control).
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $EvidenceRoot) { $EvidenceRoot = Join-Path $repoRoot ('Artifacts\BuildModeReview\run-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$null = New-Item -ItemType Directory -Force -Path $EvidenceRoot
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
$log = Join-Path $EvidenceRoot 'game.log'
$script = Join-Path $PSScriptRoot 'review_build_mode_in_game.py'
$evidence = $EvidenceRoot -replace '\\', '/'
$arguments = @(('"' + $project + '"'), '/Game/Frontier/Maps/Dev/BuildMode/BuildMode_Review', '-game', '-SFDevUnlimitedStock', '-RenderOffscreen', '-ResX=1920', '-ResY=1080',
    '-unattended', '-nosound', '-nop4', ('-abslog="' + $log + '"'), ('"-ExecCmds=py ' + ($script -replace '\\', '/') + ' ' + $evidence + '"'))
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
try {
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { throw "Review run exceeded $TimeoutSeconds seconds (owned PID $($process.Id))." }
} finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}
$text = Get-Content -LiteralPath $log -Raw
$always = 'IA_Inventory','IA_Skills','IA_Hotbar5','IA_Hotbar6','IA_CycleRecipe','IA_ClaimPlot','IA_Trade','IA_Market','IA_TargetNext','IA_BuildMode',
    'IA_Escape','IA_Hotbar1','IA_Hotbar2','IA_Hotbar3','IA_Hotbar4','IA_BuildRotate','IA_BuildLine','IA_BuildPlane','IA_BuildLayerUp',
    'IA_BuildLayerDown','IA_BuildDemolish','IA_BuildDoor','IA_BuildPlace'
$missing = @($always | Where-Object { $text -notmatch "INPUT_ACTION $_ Event=" })
# IA_BuildPlace was injected once before B (must not fire) and once in build mode (must fire exactly once).
$placeCount = ([regex]::Matches($text, 'INPUT_ACTION IA_BuildPlace Event=')).Count
$buildOn = $text -match 'BUILD_MODE Enabled=1'
$review = $text -match 'BM_REVIEW \{.*\}' ? $Matches[0] : ''
$json = Join-Path $EvidenceRoot 'build-mode-review.json'
$checks = if (Test-Path $json) { (Get-Content $json -Raw | ConvertFrom-Json) } else { $null }
$ok = $missing.Count -eq 0 -and $placeCount -eq 1 -and $buildOn -and $checks -and $checks.passed
[ordered]@{ passed = [bool]$ok; missingActions = $missing; buildPlaceFires = $placeCount; buildModeOn = $buildOn; exitCode = $process.ExitCode; evidence = $EvidenceRoot } |
    ConvertTo-Json | Set-Content (Join-Path $EvidenceRoot 'result.json')
Get-Content (Join-Path $EvidenceRoot 'result.json')
if (-not $ok) { exit 1 }
