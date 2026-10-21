# In-process MVP journey

`Journey` composes the six package-free domain models with one injected clock, shared character and item IDs, and one process gate. It takes server-trusted operation IDs and retains whole-command receipts. Harvest grants to InventoryLedger before emitting gather evidence; crafting emits evidence from positive conversion receipt changes. Wall placement requires an active claim and consumes one crafted wall bundle. A simulated boar defeat completes the quest. The completion intent grants one `item/frontier_founder_completion_bundle` token in the ledger; this is a stand-in for the absent reward bundle catalogue. Exchange performs the direct item trade through escrow.

The Silverbrook zone JSON gained `gathering_node/iron_vein` because the quest and iron ingot recipe already require `item/iron_ore`, while the zone had no iron node. The zone authoring pipeline owns regeneration of that group.

This facade is process-local. Its gate and receipt cache do not provide crash recovery or a distributed transaction across node depletion, ledger grant, quest progress and reward delivery. Production integration still needs authenticated authority, durable operation and outbox storage, reward bundle expansion, real construction placement, combat evidence and network adapters. The test's node charges, yields, capacity and fixed clock are development fixtures, not new player controls.

From the repository root, use the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe restore BackendServices/MvpJourney.Tests --locked-mode
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --no-restore --project BackendServices/MvpJourney.Tests --locked-mode
```
