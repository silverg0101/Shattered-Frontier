# Silverbrook boar reward model

`Rewards` validates a narrow, package-free `weighted_one` item table and grants the Silverbrook boar's `item/boar_hide` to its credited killer through `InventoryLedger.Grant`. The authored table ships two guaranteed hides, matching the Unreal pilot's `HidesPerBoar=2`; one extra hide has weight 5 of 100. The table version and defeat ID determine a stable SHA-256 roll. The ledger operation ID is the defeat ID, with registry source `loot_drop` and the same causation ID. Callers must supply one immutable server-confirmed defeat event and its original container revision on every retry. Changed reuse is rejected. Eligibility requires killer credit and both killer and boar outside the protected town.

Run from the repository root with the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe restore BackendServices/Rewards.Tool --locked-mode
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe restore BackendServices/Rewards.Tests --locked-mode
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/Rewards.Tool --no-restore -- content/loot
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/Rewards.Tests --no-restore -- content/loot
```

This is an in-process integration model, not a network adapter or Unreal connection. The caller supplies trusted eligibility facts; denied event decisions live in this service instance. Multi-item grants, pity counters, secured seed epochs, durable reward decisions, and production config publication remain outside the MVP. The authored count and weights are release content adopted at table load; the shared configuration workflow is required before live adjustment.
