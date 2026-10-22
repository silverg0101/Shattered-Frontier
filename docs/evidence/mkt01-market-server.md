# MKT-01 market server checkpoint — 2026-10-22

Implemented the Silverbrook market content, parser/validator, host allowlist and fair value checks, server-owned listing and sale fees, listing quantity and price bounds, market-separated book reads and buys, and a plain replicated board marker. The zone server accepts list/buy/cancel only within the configured reach of a matching board. The Silverbrook game mode creates the marker at the Market Square south edge by tracing terrain. E opens the market panel there; real mode no longer inserts fake market rows or fake sell stock. The Alpha Starter Kit purse is 500 copper once per character when enabled.

CRF-01 grade IDs are `item/<slug>/<grade>` for Crude/Fine/Superior/Masterwork and the base ID for Common; `item/recipe_scroll_iron_brackets` is allowlisted. Fair value multipliers are 0.5/1.5/2.5/4. This change has no new art. The live board's listing refresh, backpack Sell list, My Listings and wallet presentation remain UI work. Additional town markets can be added as separate content files and books; only Silverbrook is authored now.

The 100 copper fee minimum in the draft MKT spec conflicts with its 80 copper example and the owner's 1% fee decision. The implemented minimum is 1 copper, which produces a 1 copper listing fee for 80 copper of timber and refuses a 1 copper total listing. The host ignores client supplied fee and created-time fields. The host's unit-price ceiling is 20 times the content fair value; this follows the spec's `20× fair value + 1` refusal example instead of the conflicting 100,000 copper absolute floor.

Checks on this worktree before rebase:

- Pinned SDK locked restore/build of `Gameplay.Host`: passed, zero warnings/errors.
- `Exchange.Tool.Tests`: passed grade values, denied goods, market isolation and duplicate validation; `Exchange.Tool content/markets`: `MARKET_CONTENT_OK`.
- `Exchange.Tests`: passed atomic trade, retry, concurrent buyers, fees, cancel and expiry.
- `Build.bat ShatteredFrontierEditor Win64 Development -Project=<this worktree's uproject>`: passed.
- `Automation RunTests ShatteredFrontier.Exchange`: six tests passed (`Artifacts/Automation/MKT01-20261022-024226/exchange.log`). A prior command with a `.*` suffix matched no tests and is not counted.
- `tools/Test-GameplayHosts.ps1 -EvidenceRoot Artifacts/Persistence/GameplayHosts/mkt01-20261022-024033`: passed, including invalid listing/market/quantity/price, one buyer among two, 1% listing and 5% sale fees, cancel, expiry, escrow and settlement crash recovery. Fixture removed its private cluster.
- `tools/Test-ExchangeTrade.ps1 -Market -Port 17898`: passed (`Artifacts/ExchangeTrade/20261022-024827-189/results.json`): forged market and 6 m reach probes were refused before an RPC; one trade, one Silverbrook listing and sale, 2 copper in fees, buyer purse 85, seller purse 13, replay unchanged, bags reconciled, private cluster removed. An earlier run identified a rootless board actor with an incorrect world position; the actor now has a scene root and the passing run uses it.

No steam-server steps were run. All fixture credentials were generated for each run and stayed in ignored evidence folders.

Post-rebase (`origin/main` at `b93db5c2`): pinned `Gameplay.Host` locked build passed with zero warnings/errors; `Exchange.Tool.Tests` and `Exchange.Tests` passed; the editor target built; `Automation RunTests ShatteredFrontier.Exchange` passed all six tests (`Artifacts/Automation/MKT01-final-20261022-025936/exchange.log`). `Test-ExchangeTrade.ps1 -Market -Port 17901` passed (`Artifacts/ExchangeTrade/20261022-030145-305/results.json`), including both board denials, one sale and its replay. The first post-rebase `Test-GameplayHosts.ps1` run exposed a missing `MarketId` in a replay fixture request; that request was corrected. The fresh `mkt01-final-20261022-030007` run passed (`Artifacts/Persistence/GameplayHosts/`), including wrong-market cancellation, concurrent buy, escrow recovery, settlement recovery and isolated PostgreSQL cleanup. Grade multipliers now live in versioned market content and `Exchange.Tool.Tests` also rejects altered constants.
