# Inventory ledger MVP

Package-free, in-memory inventory quantities with an atomic persistence interface. `InventoryLedger` is the only writer; callers supply trusted server-side operation IDs, source or sink context, and expected revisions. A repeated operation ID returns its original receipt before revision checks; different arguments with that ID are rejected. The store transaction commits quantities, revisions, and the receipt together before acknowledgment. Failed commands leave all three unchanged.

Containers have a registered kind and owner ID. Capacity is supplied at creation in quantity units, bounded to 1–10,000, and immutable for this slice; no live tuning control or default is introduced. The caller chooses a capacity from trusted content or policy. `Transfer` only moves between containers of the same owner; cross-owner moves need the designed escrow and binding checks. `Convert` is an atomic recipe quantity transformation, with a trusted recipe ID; this package does not validate a recipe catalogue. Equippable gear creates distinct `itm_` IDs at grant or conversion, with selected-copy transfers and replay-stable receipts; non-gear remains quantity-only. It does not yet model stack slots, binding, an append-only hash chain, or production authority checks. Source/sink codes and `recipe/` IDs are caller-provided and require integration validation. The API accepts `item/` definition IDs and `ctn_` container IDs; the test IDs are shortened fixtures.

The store contract requires a transactional, serialized implementation that persists the complete state and receipt before returning. `InMemoryInventoryLedgerStore` meets that contract only for the lifetime of the process; it provides no crash durability. The PostgreSQL adapter provides atomic commit and durable acknowledgment for quantities, gear instances, and receipts.

From the repository root, using the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/InventoryLedger.Tests --locked-mode
```
