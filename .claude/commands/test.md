---
description: Run Unreal automation tests matching a filter, headless
argument-hint: <test filter> [-Map /Game/Frontier/Maps/Sandbox/SB_Name]
---
Run the automation tests matching the filter in `$ARGUMENTS` (several filters may be joined with `+`). An optional `-Map <asset path>` selects the map; omit it to use the project default. If no filter is given, ask for one; never run the full suite unless I say so.

Run from the repo root in PowerShell, in the background:

```powershell
$out = Join-Path $PWD ('Artifacts\Automation\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\GameClient\ShatteredFrontier.uproject" <optional-map-asset-path> -unattended -NullRHI -nosound -nop4 -NoSplash '-ExecCmds=Automation RunTests <filter>' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$out" "-abslog=$out\editor.log"
```

Then read `$out\index.json` and report passed, failed and skipped counts, each failing test with its first error, and the report path. If no tests matched, say so; that is not a pass.
