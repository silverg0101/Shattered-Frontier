# Legends of the Shattered Frontier — Economy Part 4: Analytics, Security and Agent Implementation Guide
### Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per Shared Registry v1.0.

Entry file: [Economy Architecture](../Legends_Shattered_Frontier_Economy_Architecture_v1.0.md) · Registry: [Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

This part states which economy events and metrics matter (Analytics hooks), the domain threats and server-validation rules (Security considerations), and tells a coding agent how to build `economy-service` and its tooling from Parts 1–3 without inventing behaviour. Where this guide and Parts 1–3 differ, Parts 1–3 win; where any Economy text and the Registry differ on a name, the Registry wins.

---

## Analytics hooks

Existing Analytics metrics fed by this spec (definitions stay in Analytics): `m_gold_created`, `m_gold_destroyed`, `m_gold_faucet_sink_ratio`, `m_gold_money_supply`, `m_gold_money_supply_growth_wow`, `m_gold_ledger_residual`, `m_inflation_index`, `m_gold_inflation_index_wow`, `m_wealth_gini`, `m_market_velocity`, `m_quest_gold_faucet_share`, `m_marks_from_dungeons_per_char_week`, `m_marks_time_to_cap_hours`, `m_season_tokens_expired_ratio`, `m_rollback_net_created_copper`, `m_gold_created_per_active_hour`, `m_event_gold_created`, `m_gold_repair_sink_share`.

Economy targets (Economy owns these values; Analytics lists them as illustrative): weekly `m_gold_faucet_sink_ratio` 0.90–1.15 per realm; `m_wealth_gini` 0.55–0.75; `m_market_velocity` 0.15–0.40 per week; `m_gold_inflation_index_wow` within ±2 % during events, ±1 % in steady state. Full target set: Economic balancing targets.

Proposed new metrics (for Analytics to adopt):

| Metric | Formula | Grain |
| --- | --- | --- |
| `m_index_reconciliation_delta` | `|economy_published_value − index_value|` in `snap_price_index_daily` | day per dc_region |
| `m_market_fee_sink_copper` | Σ debits with sinks `market_listing_fee`, `market_transaction_fee`, `market_expiry_fee` | day per realm |
| `m_market_fee_sink_share` | `m_market_fee_sink_copper / m_gold_destroyed` | week per realm |
| `m_lien_outstanding_copper` | Σ `lien_outstanding` (gold) over distinct accounts in the snapshot | day per dc_region |
| `m_lien_collected_copper` | Σ debits with `lien_id` set | day |
| `m_lien_written_off_copper` | Σ `written_off_amount` of `economy.lien.closed` | week |
| `m_transfer_holds_placed` | count of `economy.hold.placed` by `transfer_kind` | day |
| `m_transfer_hold_cancel_rate` | cancelled / (released + cancelled) holds | week |
| `m_marks_capped_amount` | Σ `capped_amount` on marks credits | week per realm |
| `m_expiry_fee_waived_copper` | Σ `expiry_fee_waived_copper` | week |
| `m_gold_sink_share_by_group` | Σ sink debits per sink group (Sink composition at cap) / `m_gold_destroyed` | week per realm |
| `m_vendor_faucet_share` | Σ `vendor_sale` credits (excluding bought-back sales) / `m_gold_created` | week per realm |
| `m_price_band_adherence` | share of basket raw resources whose 7-day median lies in their tier price band | week per dc_region |
| `m_marks_balance_p50_at_cap` | median marks balance of level-100 characters in the snapshot | week per realm |

Dashboards (Analytics-built): an economy control board per realm showing FSR, sink composition vs bands, index and `D`, repair scaling, holds and liens; the seasonal audit report.

Telemetry not to emit: per-search events (`SearchListings`) are not published; Economy logs only aggregate search counts as `ops` metrics, to keep p1 volume down.

---

## Security considerations

### Threats and controls

| Threat | Control | Signal / detector |
| --- | --- | --- |
| Duplication via retries or crashes | idempotency table, per-wallet `wallet_seq`, serializable transactions, outbox-driven escrow with economy-first sweeper check | `economy_invariant` rejects; `detector/currency_conservation` |
| Negative balance or overdraft races | CHECK constraints, row locks, `reserved` excluded from spendable | INV-7 never violable by construction |
| Crowns → gold leakage | per-currency allow-list, causation join check, crowns excluded from trade/market/mail | hourly INV-9 query |
| RMT delivery (gold gifts, overpriced listings) | transfer holds at `elevated`, price ceiling `P_max`, mail newbie throttle, gift trades always evaluated | `security.trade.flagged` (anticheat), funnel detector |
| Wash trading / index manipulation | fees, outlier and flag exclusion, weight cap, trading-skill exclusion of same-account buyers (Character) | `market` detectors over `market.listing.sold` |
| Market spoofing (list/cancel cycles) | non-refundable listing fee, `listing_age_seconds` | `market.listing.cancelled` features |
| Lien evasion via alts or guilds | account-wide scope, 100 % garnish on transfer-ins, outgoing transfer block, guild deposit block | lien collection metrics |
| Vendor arbitrage | `VENDOR_MARKUP_MIN`, natural Charisma only, buyback at original price | content validator; `m_vendor_faucet_share` |
| Event-multiplier abuse | credits validated against active `lop_`, multiplier ≤ 2.0 | `economy_invariant` |
| Insider abuse (GM grants, hold release) | JIT elevation, per-action cap, second approver above cap, audit-before-act | audit-service, `staff` detectors |
| Bait-and-switch | `expected_unit_price` on buys, offer change resets trade confirmations | `FAILED_PRECONDITION` |

### Server validation rules (normative)

1. **Server authority and no client amounts.** Clients send intents (`BuyListing(listing_id, quantity, expected_unit_price)`); prices, fees, values and balances are computed server-side. `expected_unit_price` protects buyers from bait-and-switch (mismatch → `FAILED_PRECONDITION`).
2. **Invariants owned here:** INV-7 (CHECK constraints), INV-8 (double-entry in one transaction; per-wallet `wallet_seq`), INV-9 (allow-list table + CI check), INV-10 (escrow per side with inventory-service), INV-11 (idempotency table), INV-12 (weekly counter row lock). Violations raise `economy_invariant` rejects to `anticheat-service.ReportSignals`.
3. **Audit.** `audit-service.Append` before: any `gm_grant`, `liveops_compensation`, `enforcement_rollback` (including garnish debits), `enforcement_restore`; any transfer ≥ `tune.security.audit_value_threshold_copper` (10,000,000); `PlaceLien`, `ReleaseLien`, `WriteOffLien`, `ApplyRestriction`, `LiftRestriction`, `ResolveHold`, `RecomputeIndex`, backfill `ExportWalletSnapshot`. Audit unavailable → these fail closed.
4. **Fail modes.** `CheckRestrictions` fail-closed at or above `T_hold`; `GetRiskScore` failure → hold. `ff.security.economy_freeze` rejects trade, market and mail gold on the realm and pauses holds and expiry; NPC sinks and rewards continue.
5. **Lien evasion.** Outgoing transfer block (Part 1, Wallet liens), 100 % garnish on transfer-ins, account scope across characters and realms of the dc_region; guild wallets are not garnished, so deposits into guild wallets by a liened account are blocked (treated as outgoing transfers).
6. **Market manipulation.** Spoofing (list/cancel cycles) is deterred by the non-refundable listing fee and visible through `listing_age_seconds`; wash trades between linked accounts are detected by Security's trade detectors from `market.listing.sold` and `economy.trade.completed`. Outlier filtering and flag exclusion keep manipulated fills out of the index.
7. **Hold opacity.** Players see a generic "pending review"; band, score and detector are never sent to clients.
8. **Privacy.** All payload IDs are `p1_pseudonymous`; no `p2`/`p3` fields are published. The wallet snapshot contains no names or contact data.
9. **Staff powers.** `gm_grant` and `ResolveHold` require JIT elevation (`elv_`) and are capped per action (`tune.economy.gm_grant_max_copper`, default 1,000,000 copper; above requires a second approver).
10. **Crowns isolation.** Crowns wallets accept only allow-listed codes; liens on crowns are rejected; crowns are excluded from trades, market and mail.
11. **Service identity.** Every caller is identified by mTLS service identity; the caller allow-list is checked per code. zone-server calls carry `srvi_` and may act only for characters currently in its zone (checked against world-service's location view).
12. **Rate limits.** Per character: 30 market mutations per minute, 10 searches per 10 s, 20 trade proposals per 10 min, 50 vendor transactions per minute; excess → `rate_limited` reject to anticheat.

### Signals reported to `anticheat-service`

| Signal (`reject_reason`) | Trigger |
| --- | --- |
| `economy_invariant` | allow-list or caller violation, reconciliation residual, season/event credit mismatch, escrow-transfer failure after payment, vendor quote mismatch |
| `rate_limited` | market/trade/vendor rate limit exceeded |
| `sanction_restricted` | attempts while `trade_restriction`/`market_restriction`/rollback freeze is active (repeated attempts are a signal) |
| `replay_detected` | idempotency key reused with a different request hash |

---

## Repository and module layout

Per the Repository Structure document (`/BackendServices`, introduced only with a chosen backend implementation). Language and frameworks follow the Backend spec; paths are normative for structure, not language.

```text
BackendServices/
  libs/
    lsf-econmath/              # pure, deterministic: gold_reward/marks_reward wrappers, market fees,
                               # garnish allocation, marks cap, fair value, Laspeyres index, imputation,
                               # weight capping, dynamic fee factor, repair/vendor scaling steps,
                               # npc_price_mult, vendor buy/sell prices, price bands
      src/ tests/ golden/      # golden vectors = every worked example in Parts 1–3
    lsf-econ-codes/            # generated from Registry §7.2 + Part 1 allow-lists (codes, flow classes,
                               # per-currency and per-caller allow-lists); single source for service and CI
  economy-service/
    api/proto/lsf/economy_service/v1/   # EconomyService, EconomyAdmin
    src/
      ledger/                  # write procedure, idempotency, wallet locks, outbox
      wallets/                 # GetWallet, Credit, Debit, TransferCurrency (mail)
      caps/                    # marks weekly counters
      liens/                   # PlaceLien/Release/WriteOff, garnish hook, expiry job, sanction consumer
      holds/                   # hold rule, re-check job, ResolveHold
      market/                  # listings, fills, search, expiry job, escrow worker
      trade/                   # trade state machine, escrow worker
      vendors/                 # QuoteNpcPrices, Buy/Sell/Buyback, stock counters
      index/                   # prices daily, basket selection, index compute, revisions
      controls/                # dynamic fees, repair scaling, vendor scaling proposals
      snapshot/                # wallet-snapshot-export (Iceberg writer)
      reconcile/               # hourly INV-8, INV-9 queries
      privacy/                 # PrivacyParticipant.ExportSubject / EraseSubject
      consumers/               # enforcement.sanction.applied, liveops.event.*, progression.attribute.changed,
                               # faction.reputation.changed, security.trade.flagged
    migrations/ (economy schema)
    tests/ (unit, integration, property, load, chaos)
Tools/
  economy-sim/                 # agent-based simulator: faucets/sinks per band, market, index, controls;
                               # produces the seasonal audit report (uses lsf-econmath)
  content-validator/ economy rules: vendor catalogs (arbitrage guard), currency definitions
```

Rule: no formula is implemented twice. economy-service, the content validator, the simulator and Analytics' reference checks import `lsf-econmath` or reproduce its golden vectors.

---

## Build order and milestones

| Milestone | Scope | Exit criteria |
| --- | --- | --- |
| E1 Codes and math | `lsf-econ-codes` generated from Registry; `lsf-econmath` with golden vectors | every worked example in Parts 1–3 reproduced exactly; CI fails on Registry/code drift |
| E2 Ledger core | schema, write procedure, `GetWallet`/`Credit`/`Debit`, idempotency, outbox, allow-lists, audit-before-act | property tests for INV-7/8/11 pass; `economy.currency.*` payloads validate against schema registry |
| E3 Caps and seasons | marks weekly counters; season token rules and conversion bound checks | marks cap example; LiveOps conversion example; INV-12 property test |
| E4 Trade and mail | trade state machine, `TransferCurrency` (mail), escrow integration with inventory-service | chaos test: kill -9 at every saga step, no dupes (INV-10) |
| E5 Market | listings, fills, fees, search, expiry, price ceiling, escrow worker | fee worked example; partial fills; replica search vs primary revalidation |
| E6 Holds | hold rule, re-check job, `ResolveHold`, `economy.hold.*` | hold worked example; fail-closed paths with injected `CheckRestrictions`/`GetRiskScore` failures |
| E7 Liens | Place/Release/WriteOff, garnish hook, transfer block, sanction consumer, expiry | lien worked example; multi-lien allocation; enforcement mirror consistent |
| E8 Vendors | `QuoteNpcPrices`, vendor RPCs, stock counters, buyback, catalog validator | NPC price example; arbitrage guard rejects bad catalogs; requires Itemization `CreateVendorItems`/`MoveToBuyback` |
| E9 Index and snapshot | prices daily, basket, index revisions, wallet snapshot export, reconciliation | index worked example and imputation variant; snapshot reconciliation identity holds on a replayed day |
| E10 Controls and privacy | dynamic fees, repair/vendor scaling, event multiplier validation, privacy participant | control step limits; erasure saga phase 1 test |
| E11 Load and audit | load tests, seasonal audit report via `economy-sim` | load targets met; audit report generated for a simulated season |

Dependencies outside Economy: E4/E5 need inventory-service escrow (Itemization M7); E6 needs enforcement `CheckRestrictions` and anticheat `GetRiskScore` (Security S6/S5); E8 needs the two proposed inventory RPCs; E9 needs Analytics landing namespace grants.

---

## Invariants checklist

1. No wallet balance below 0; `0 ≤ reserved ≤ balance` (DB CHECK; never only app code).
2. Every `txn_` row has exactly one of `source_code`/`sink_code`, `amount > 0`, a `causation_id`, an `idempotency_key`, a `flow_class` matching `lsf-econ-codes`, and a `config_version`.
3. Per wallet, `wallet_seq` is gap-free and `balance = Σ signed amounts` of its rows.
4. Per realm × currency × hour: `Σ credits − Σ debits = Δ Σ balances` (INV-8); `Σ market_sale = Σ market_purchase`; `Σ trade_send = Σ trade_receive`.
5. No gold credit is caused by a crowns debit (INV-9); crowns wallets carry only allow-listed codes.
6. Marks credited per character per week ≤ 1,500 excluding corrections (INV-12).
7. Each escrow hold is referenced by exactly one open trade, listing fill or mail transfer; no escrow is released for a committed transfer (economy-first sweeper check).
8. Same idempotency key → same result, no second effect, for ≥ 7 days (INV-11).
9. Liens: `collected ≤ original_amount`; garnish ≤ the credit in the same transaction; no lien on crowns; terminal states never change.
10. Holds: balances unchanged while `held`; every hold reaches `released` or `cancelled` by `max_release_at` unless the realm is frozen.
11. Vendor catalogs satisfy `base_price ≥ 4 × vendor_value`; vendor buy reservations are either consumed by a debit or released.
12. Audited actions have an audit record written before the effect; audit unavailable → no effect.
13. No event published without its DB transaction committed (outbox); no event lost after commit.

---

## Test matrix

| Level | Case | Expected |
| --- | --- | --- |
| Unit | Market fees for 20 ingots at 1,500, 12 sold, 8 expired | fees 300 / 900 / 120; seller net 17,100 |
| Unit | Marks cap: used 1,420, grant 150 | applied 80, capped 70; next grant capped fully |
| Unit | Garnish: lien 250,000; quest 3,600; market sale 40,000 with fee 2,000 | garnish 1,800 then 38,000; outstanding 210,200 |
| Unit | Garnish across two liens (100 then 50 outstanding), credit 400 at 0.5 | g = 150; allocation 100 → lien 1 (settled), 50 → lien 2 (settled); one debit row, two `lien_collections` |
| Unit | Index: 4-item basket example | I = 106.40; chain 119.17; imputation variant 107.65 |
| Unit | Weight cap: raw 0.21 in a 24-item basket | capped 0.15, excess redistributed, Σ w = 1 ± 1e-9 |
| Unit | Dynamic fee: I_chain 106 vs 100 | D = 1.12; r_txn_eff = 0.056; step limit applied from previous D |
| Unit | NPC prices: Charisma 80, honored | mult 0.9025; potion 1,805; ring sale 2,149 |
| Unit | Price ceiling: unsold epic sword vendor value 2,097 | P_max 167,760 |
| Unit | Season conversion: level 100, 1,640 tokens | debit 1,000, credit 2,030,000, debit 640; bound check passes |
| Property | Random interleavings of credits, debits, trades, fills, mail with retries and duplicate deliveries | INV-7, INV-8, INV-10, INV-11 hold; wallet fold equals balance |
| Property | Random lien/credit sequences | collected never exceeds original; no negative balance; settle exactly at 0 |
| Property | Random marks credits with corrections across week boundaries | weekly sum ≤ cap; corrections uncounted |
| Property | Random vendor catalogs and modifier inputs | buy price ≥ 2.4 × vendor value > sell price |
| Integration | Fill with seller at band `elevated`, value ≥ T_hold | fill `held`; buyer gold reserved; release after 24 h executes; events carry `hold_id` |
| Integration | `GetRiskScore` timeout at value ≥ T_hold | hold placed with `risk_band` absent |
| Integration | `CheckRestrictions` error below / above T_hold | proceed / reject with retry hint |
| Integration | Permanent ban event for liened account | all active liens `written_off`; `economy.lien.closed` each |
| Integration | Appeal overturned | `ReleaseLien` returns collected; restore saga credit with `enforcement_restore` is exempt from garnish |
| Integration | Erasure of account with open listing, held trade, lien and balances | listing cancelled, trade cancelled, lien written off, balances debited `account_erasure` |
| Integration | Audit-service down during `gm_grant` | `UNAVAILABLE`, no ledger row |
| Integration | Snapshot day replay | `balance_d − balance_{d−1}` equals ledger sum per wallet; totals match `economy.snapshot.exported` |
| Chaos | kill -9 between ledger commit and `TransferEscrow` in a fill | worker completes transfer on restart; inventory sweeper does not release escrow |
| Chaos | serialization failures under hot wallet (guild upkeep + 50 deposits/s) | retries succeed; p99 within budget |
| Load | 5,000 `Credit`/s per dc_region sustained, 20,000/s burst (raid reset) | p99 ≤ 40 ms; no lock timeouts |
| Load | 500 fills/s on one hot commodity book | p99 `BuyListing` ≤ 80 ms; no double-fill of a listing |
| Load | `SearchListings` 3,000 rps | p99 ≤ 60 ms on replicas |

---

## Acceptance criteria

1. All golden vectors from Parts 1–3 pass in `lsf-econmath` and in the service integration suite.
2. Hourly reconciliation shows residual 0 for 14 consecutive simulated days with chaos injection enabled.
3. Every `economy.*` and `market.*` event validates against its payload table (Part 3, Event payloads); Analytics' interim-requirement fields are present.
4. CI static check: the allow-list seed equals the Part 1 tables and `lsf-econ-codes` equals Registry §7.2; no code outside the Registry is used unless behind its proposal flag.
5. A simulated season in `economy-sim` with reference populations keeps realm FSR within 0.90–1.15 and Gini within 0.55–0.75, and produces the seasonal audit report.
6. Fail-closed behaviour verified for audit, restrictions and risk score dependencies.
7. Privacy participant passes Backend's erasure and export saga tests.

---

## Pitfalls

- Reading balances from a replica or cache before a debit or season conversion: always lock the primary row.
- Garnishing before applying the marks cap or before the market fee debit: the order is cap → credit → fee → garnish.
- Treating `market_purchase` or `trade_send` as sinks in metrics: they are transfers.
- Forgetting that a fully capped marks credit writes no `txn_` row but must still return a stored idempotent result.
- Releasing escrow from inventory's sweeper while a hold is `held`: holds keep escrow alive until resolved.
- Re-applying Charisma or faction modifiers on buyback (the price is the original sale price).
- Using effective (gear-boosted) Charisma for prices: natural only.
- Writing a "refund" credit for a failed vendor purchase: use reservation and release instead.
- Computing the index from fills that later become flagged without publishing revision 1.
- Letting an expiry fee drive a wallet negative: cap by spendable and record the waived part.
- Dropping old ledger partitions: never; archive is Analytics' copy, the OLTP ledger is the system of record.

---

## Do / Don't for coding agents

| Do | Don't |
| --- | --- |
| Use Registry codes from `lsf-econ-codes` only | Invent a source/sink code or reuse a nearby one (e.g. `vendor_purchase` for store spend) |
| Put every balance change through the ledger write procedure | Update `economy.wallets` without a `txn_` row |
| Compute prices, fees and values server-side from tuning keys | Accept amounts or prices from clients |
| Call inventory-service for every item move via escrow | Write `inventory` tables or move items directly between owners |
| Publish events via the outbox in the same transaction | Publish events before commit or from a different transaction |
| Fail closed on audit, and on restrictions at or above `T_hold` | Fail open on any audited or high-value path |
| Keep player-facing errors generic for holds and sanctions | Send risk band, score or detector names to clients |
| Treat merged names (`QuoteNpcPrices`, `vendor/`, `transfer_kind = guild`, settlement-tax codes) as Registry-canonical; keep settlement-tax behavior flagged off while amounts and Building inputs remain open | Ship settlement-tax behavior before its open inputs are resolved |
| Add a golden vector for every new formula | Re-implement a formula outside `lsf-econmath` |
| Raise contradictions with other specs in the entry file's open issues | Edit the Registry, the dependency map, or other specs |
