# Exchange MVP domain model

Package-free, process-local model for character gold wallets, direct two-party trades, and whole-stack fixed-price listings. The InventoryLedger is the item writer. `DeliverEscrowBatch` is its single trusted settlement hook: it validates all escrow sources, destinations, revisions, quantities and capacities in one store transaction. Exchange serializes wallet mutations and inventory calls under one process gate; item delivery happens before wallet postings only after every monetary check has passed. This is in-memory atomicity only, not a distributed commit or crash-safe production saga.

Every mutation takes an operation ID (maximum 128 characters). Successful and rejected receipts are retained for this process lifetime; exact replay returns the same receipt, and conflicting reuse throws. Currency entries have registry currency/source/sink codes, flow class and gap-free sequence per wallet. `market_purchase` and `market_sale` are transfers; listing, transaction and expiry fees are sinks. Listings snapshot the effective typed settings at creation. Settings expose keys, labels, units, shipped defaults, bounds, permission and adoption boundary for a future shared configuration workflow; this package does not publish settings. Whole-stack purchases are deliberate MVP scope; no partial fills.

The caller must supply trusted character identity, item catalogue facts, fair value, realm/range/hub attestation and restrictions before calling this model. Listing checks use the supplied binding/category/level/hub/realm facts and enforce the authored price ceiling. A Boolean attestation is not a production authorization mechanism. The model has no database, listener, Unreal adapter, price index, transfer holds, risk scoring, weekly cap, sanctions, durable receipts, outbox or cross-service recovery. Escrow containers remain character-owned quantity containers rather than `itm_` instances, matching the current InventoryLedger slice. It accepts only character gold for trade and market. `Credit` is a small fixture/domain entry point with allow-listed registry source codes; production faucet authorization remains integration work. Gold amounts are integer copper. Cancellation returns every locked side's item escrow and releases gold reservations; expiry charges a spendable-bounded fee and returns the item.

From the repository root, with the pinned backend SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/Exchange.Tests --locked-mode
```
