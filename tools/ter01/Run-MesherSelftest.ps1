<#
TER-01b: build and run tools/ter01/mesher_selftest.cpp (SFVoxelCore.h without the engine) with the VS toolchain UE uses.
  pwsh tools/ter01/Run-MesherSelftest.ps1 [-Soak 500]
#>
param([int]$Soak = 500)
$ErrorActionPreference = 'Stop'
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$out = Join-Path ([IO.Path]::GetTempPath()) 'sf-mesher-selftest'
New-Item -ItemType Directory -Force $out | Out-Null
$src = Join-Path $PSScriptRoot 'mesher_selftest.cpp'
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && cl /nologo /O2 /std:c++17 /EHsc /W3 `"$src`" /Fe:`"$out\selftest.exe`" /Fo:`"$out\\`"" | Select-Object -Last 5
if ($LASTEXITCODE) { throw 'compile failed' }
& "$out\selftest.exe" $Soak
exit $LASTEXITCODE
