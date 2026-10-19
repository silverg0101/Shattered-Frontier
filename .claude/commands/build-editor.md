---
description: Build the ShatteredFrontierEditor target (Win64 Development)
---
Build the editor as `GameClient/README.md` describes. If an `UnrealEditor` process has this project open, stop and tell me to close it or use Live Coding instead.

Run from the repo root in PowerShell, in the background if it may exceed two minutes:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' ShatteredFrontierEditor Win64 Development "-Project=$PWD\GameClient\ShatteredFrontier.uproject" -WaitMutex -NoHotReloadFromIDE
```

Report the exit code, warning and error counts, and the first error with its file and line. Do not dump the full log.
