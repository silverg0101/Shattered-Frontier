# Greenlands starter crafting content

`CraftingContent` loads release-shipped JSON recipes and validates IDs, stations, quantity and duration bounds, duplicate slots, and item dependency cycles. Counts and durations are content values, not runtime console settings. They are release-bound until a reviewed typed control is declared in the shared configuration workflow. The MVP station names are `sawmill`, `forge`, and a provisional `workbench` for hand assembly; the latter needs a canonical workshop mapping before production integration.

`CraftAttemptService.ExecuteGraded` selects exact input grade definitions, computes deterministic quality from skill and material midpoints, and converts to a graded output in one ledger receipt. Common keeps its base id. A crafter below difficulty still succeeds with Crude. `CraftingControls` declares bounded launch values for the shared configuration workflow; grade thresholds remain versioned code. The legacy `Execute` method serves the original content-model tests.

The 12 recipes include nine defaults, trainer-taught Worked Timber, scroll-taught Iron Brackets, and practice-discovered Tempered Iron Ingot. `CraftingContent.Tool` also validates dialogue `learn:` choices against trainer ids. Generated graded definitions and the scroll are created in the editor by `tools/Create-GradedItems.py` and checked by `tools/Check-ItemIds.py`. No item-instance provenance is claimed.

From the repository root, using the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe restore BackendServices/CraftingContent.Tool --locked-mode
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe restore BackendServices/CraftingContent.Tests --locked-mode
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --no-restore --project BackendServices/CraftingContent.Tool -- content/crafting
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --no-restore --project BackendServices/CraftingContent.Tests -- content/crafting/recipes_greenlands_starter.json
```
