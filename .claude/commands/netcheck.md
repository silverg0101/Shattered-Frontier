---
description: Run a multi-process network check runner from tools/
argument-hint: gathering | refining | construction | felling | e1
---
Run the network check named by `$ARGUMENTS` from the repo root in Windows PowerShell, in the background:

| Name | Command |
|------|---------|
| gathering | `.\tools\Test-GatheringPilot.ps1` |
| refining | `.\tools\Test-GatheringPilot.ps1 -Refining` |
| construction | `.\tools\Test-ConstructionNetwork.ps1` |
| felling | `.\tools\Test-HarvestFellingNetwork.ps1` |
| e1 | `.\tools\Test-E1NetworkScenario.ps1` |

For any other name, list the `tools\Test-*.ps1` runners and ask which one. The editor must be built first (`/build-editor`).

Report pass or fail, the thrown error if any, and the evidence folder the runner prints.
