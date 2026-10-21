# Construction placement cost model

`PlacementService` composes ClaimTerritory build checks, InventoryLedger debit/credit receipts and a claim-scoped registry of foundation, wall, door and roof cells. Costs come from the single material-bundle output of each release-shipped CraftingContent recipe. Caller-supplied content IDs are checked against that catalogue; the server adapter must authenticate actor and location. Operation IDs return immutable receipts, changed payloads conflict, and claim/structure/container revisions fence stale commands.

The model tracks `Pending`, `Committed`, `Compensated` and `Rejected`. A deterministic post-consume fault refunds the exact bundle with its own idempotent ledger operation before returning a compensated receipt. Remove refunds the attributed contributor's container, with floor rounding. The shipped effective realm snapshot is Undo 100%, voluntary 50%, destruction 25%, each bounded 0–100 integer percent, published by config-service economy tier 3. The snapshot is pinned to each structure generation; new settings apply to subsequent placements. Zero-unit rounded refunds still extinguish the structure entitlement. No repair or unsupported-collapse policy is modeled.

This is process-local evidence, not a persistent service: the registry, coordinator states and receipts are not durable, and the existing ledger has no prepare/hold API or cross-domain claim fence. A crash between consume and compensation, an independent ledger mutation, or a concurrent claim change cannot be reconciled safely by this model. Production requires Construction Integration's durable prepare/decision/completion protocol, recovery worker, held refund outputs, and server authority adapter. The failure seam exercises the intended compensation arithmetic within one call; it does not establish crash recovery.

From the repository root:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/Placement.Tests -- content/world/zones/greenlands_silverbrook.json content/crafting/recipes_greenlands_starter.json
```
