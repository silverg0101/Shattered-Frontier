# Greenlands starter crafting content

`CraftingContent` loads release-shipped JSON recipes and validates IDs, stations, quantity and duration bounds, duplicate slots, and item dependency cycles. Counts and durations are content values, not runtime console settings. They are release-bound until a reviewed typed control is declared in the shared configuration workflow. The MVP station names are `sawmill`, `forge`, and a provisional `workbench` for hand assembly; the latter needs a canonical workshop mapping before production integration.

`CraftAttemptService.Execute` validates the recipe and station, then calls InventoryLedger `Convert` with the server-supplied operation ID and expected container revision. A repeated identical operation ID returns the ledger's committed receipt. Missing inputs, stale revision, capacity failures and conflicting retries reject without mutation. The caller must authenticate the actor, authorize the container and station, complete the channel, and supply a trusted operation ID. The service has no listener, durable store, item definitions for new outputs, recipe learning, quality ladder, repair-kit use, or Unreal adapter. The bundle outputs are provisional material items; construction consumption is not wired yet.

From the repository root, using the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe restore BackendServices/CraftingContent.Tool --locked-mode
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe restore BackendServices/CraftingContent.Tests --locked-mode
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --no-restore --project BackendServices/CraftingContent.Tool -- content/crafting
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --no-restore --project BackendServices/CraftingContent.Tests -- content/crafting/recipes_greenlands_starter.json
```
