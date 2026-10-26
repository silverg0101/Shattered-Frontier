$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Content = Join-Path $Repo 'GameClient/Content/Frontier'
$Config = Get-Content (Join-Path $Repo 'GameClient/Config/DefaultGame.ini') -Raw

foreach ($Directory in @('/Game/Frontier/UI/Kit', '/Game/Frontier/UI/Icons')) {
    $Rule = "+DirectoriesToAlwaysCook=(Path=`"$Directory`")"
    if (-not $Config.Contains($Rule)) { throw "Missing cook rule for dynamically loaded assets: $Directory" }
}

$Assets = @(
    'UI/Kit/DA_HudKit.uasset',
    'UI/Kit/T_Kit_icon_timber_floor.uasset',
    'UI/Kit/T_Kit_icon_timber_wall.uasset',
    'UI/Kit/T_Kit_icon_wooden_door.uasset',
    'UI/Kit/T_Kit_icon_timber_roof.uasset',
    'UI/Icons/T_Icon_build_foundation.uasset',
    'UI/Icons/T_Icon_build_wall.uasset',
    'UI/Icons/T_Icon_build_door.uasset',
    'UI/Icons/T_Icon_build_roof.uasset'
)
foreach ($Asset in $Assets) {
    if (-not (Test-Path -LiteralPath (Join-Path $Content $Asset) -PathType Leaf)) { throw "Missing source asset: $Asset" }
}

$BuiltItem = Get-Item -LiteralPath (Join-Path $Content 'Characters/Player/Built')
$Built = $BuiltItem.FullName
$ManifestDirectory = Split-Path -Parent $BuiltItem.Target
$ManifestPath = Join-Path $ManifestDirectory 'assembly-manifest.json'
if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) { throw 'The verified MetaHuman assembly manifest is not available beside the generated-face cache.' }
$Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
if ([IO.Path]::GetFullPath($BuiltItem.Target).TrimEnd('\') -ne [IO.Path]::GetFullPath($Manifest.Cache).TrimEnd('\')) {
    throw 'The generated-face junction does not point at the cache named by its manifest.'
}
$ManifestEntries = @{}
foreach ($Entry in $Manifest.Files) { $ManifestEntries[$Entry.Path] = $Entry }
$Faces = @(Get-Content (Join-Path $Repo 'GameClient/Config/DefaultGame.ini') | Where-Object { $_ -match '^\+PlayerFaceIds=(.+)$' } | ForEach-Object { $_ -replace '^\+PlayerFaceIds=', '' })
if ($Faces.Count -eq 0) { throw 'No allowed PlayerFaceIds were found in DefaultGame.ini.' }
foreach ($Face in $Faces) {
    $RelativePath = "MHC_$Face/BP_MHC_$Face.uasset"
    $Entry = $ManifestEntries[$RelativePath]
    $AssetPath = Join-Path $Built $RelativePath
    if (-not $Entry -or -not (Test-Path -LiteralPath $AssetPath -PathType Leaf)) { throw "Missing allowed face class package: $AssetPath" }
    $Item = Get-Item -LiteralPath $AssetPath
    if ($Item.Length -ne $Entry.Bytes -or (Get-FileHash -LiteralPath $AssetPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Entry.Sha256.ToLowerInvariant()) {
        throw "Allowed face class package does not match the verified assembly manifest: $AssetPath"
    }
}

Write-Output "PASS: UI art and cook roots are present; all $($Faces.Count) allowed face-class packages match the verified assembly cache. This does not verify a cooked package."
