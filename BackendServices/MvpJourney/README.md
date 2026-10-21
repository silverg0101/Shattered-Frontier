# In-process MVP journey

`Journey` composes eight package-free domain models with one injected clock, shared character and item IDs, and one process gate. It takes server-trusted operation IDs and retains whole-command receipts. Harvest grants to InventoryLedger before emitting gather and Progression receipts; crafting emits both from positive conversion receipt changes. Wall placement requires an active claim and consumes one crafted wall bundle. A simulated credited boar defeat uses the authored `content/loot/greenlands_starter.json` table through Rewards, grants hides once through InventoryLedger, feeds an enemy-defeat receipt to Progression, and completes the quest. The completion intent grants one `item/frontier_founder_completion_bundle` token in the ledger; this is a stand-in for the absent reward bundle catalogue. Exchange performs the direct item trade through escrow.

`JourneyPracticeSettings` is a typed, validated release-authored effective snapshot for this adapter's missing producer fields: difficulty 25 (0–1100), action duration 6 seconds (1–60), and boar level 1 (1–200). It is captured at journey creation. The shared configuration workflow must publish adjusted values for new instances; this adapter has no console editor. The default progression roll source is `Random.Shared.NextDouble`; tests inject zero to check exact Progression formula outcomes. This is a fixture mapping until trusted gathering, crafting and combat producers provide difficulty, duration and enemy level in their receipts.

The Silverbrook zone JSON gained `gathering_node/iron_vein` because the quest and iron ingot recipe already require `item/iron_ore`, while the zone had no iron node. The zone authoring pipeline owns regeneration of that group.

This facade is process-local. Its gate and receipt cache do not provide crash recovery or a distributed transaction across node depletion, ledger grant, progression, quest progress and reward delivery. Production integration still needs authenticated authority, durable operation and outbox storage, reward bundle expansion, real construction placement, combat evidence and network adapters. The boar simulation supplies trusted eligibility facts for this test only. The test's node charges, yields, capacity, fixed clock and roll are development fixtures, not new player controls.

From the repository root, use the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe restore BackendServices/MvpJourney.Tests --locked-mode
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --no-restore --project BackendServices/MvpJourney.Tests --locked-mode
```
