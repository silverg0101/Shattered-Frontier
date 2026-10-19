$ErrorActionPreference = 'Stop'
$previewProject = (Resolve-Path (Join-Path $PSScriptRoot '../../ShatteredFrontier.uproject')).Path
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' $previewProject '/Game/Frontier/Maps/Test/Greenlands_ArtReview' '-game' '-windowed'
