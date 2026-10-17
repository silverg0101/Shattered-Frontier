# Legends of the Shattered Frontier — Itemization: Analytics, Security and Agent Implementation Guide
### Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per Shared Registry v1.0.

Entry file: [Itemization Architecture](../Legends_Shattered_Frontier_Itemization_Architecture_v1.0.md) · Registry: [Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

---

## 1. Analytics hooks

Analytics owns metric definitions in the warehouse; this section states which Itemization events and metrics matter and what they must answer. Event payloads are in the Item model part and the Loot and Rewards part.

### 1.1 Metrics

| Metric | Definition (source events) | Target / alert |
| --- | --- | --- |
| `m_items_created_by_source` | count of `item.instance.created` by `source`, `rarity`, `category`, per realm per hour | dashboard; alert on > 3× 7-day baseline for any `(source, rarity)` |
| `m_loot_drop_rate_deviation` | observed / expected per `(table_id, definition_id)` from `loot.table.rolled` vs pack manifest expectations | alert if \|deviation\| > 15 % over ≥ 10,000 rolls |
| `m_rarity_distribution` | share of gear drops by rarity per source | Loot and Rewards part, Drop rates by rarity |
| `m_pity_trigger_rate` | `loot.pity.triggered` / successes per `pity_key` | 25–45 % of successes (with defaults); outside → table too stingy or generous |
| `m_pity_mean_attempts` | mean `failures_before + 1` per key | ≈ 0.72 × 1/p |
| `m_equipped_ilvl_avg` | from `inventory.item.equipped.equipped_ilvl_avg` per level band, per week | levelling: within 8 of `4 × L` |
| `m_hours_to_ilvl_tier` | played hours from level 100 until `equipped_ilvl_avg` ≥ tier (405, 415, 420, 430, 445, 460) | Loot and Rewards part, Gear acquisition pace |
| `m_gold_repair_sink_per_hour` | Σ `item.instance.repaired.cost_copper` / played hours, per level band | Loot and Rewards part, Gold faucet vs repair sink |
| `m_gold_repair_sink_share` | repair sink / gold created (economy events) | 10–20 %; 7-day breach → Economy tunes repair scaling |
| `m_item_exit_rate` | items leaving via `item.instance.destroyed`, `item.instance.salvaged`, vendor, by rarity and age | Loot and Rewards part, Item sink rates |
| `m_salvage_share` | salvaged / all exits | 35–55 % |
| `m_broken_item_rate` | `item.instance.broken` per 1,000 combat hours | trend only; spikes imply wear tuning or bot farming |
| `m_reward_delivery_latency_p99` | `reward.grant.delivered.latency_ms` p99 | < 2 s; alert > 10 s for 5 min |
| `m_reward_failed_grants` | count of `reward.grant.failed` | alert on any > 0 per 15 min in prod |
| `m_reward_overflow_rate` | `overflowed_items` / items delivered | < 5 %; higher → bag sizes or UX |
| `m_loot_assign_seconds_p50` | `loot.drop.assigned.seconds_to_assign` by mode | need/greed < 45 s |
| `m_master_loot_self_assign_share` | master-loot drops assigned to the loot master | review > 40 % per raid group over 4 weeks (abuse signal, not an alert) |
| `m_legendary_path_completion_rate` | `item.legendary.stage_advanced` reaching `forged` / started | 35–55 % of starters within 10 weeks |
| `m_legendary_days_to_forge` | `days_since_start` at forge | 42–56 days median |
| `m_ledger_reconciliation_anomalies` | reconciler mismatches (`item.instance.quarantined` with reason `ledger_mismatch`) | must be 0; any → page |
| `m_items_per_character` | active items per character (daily snapshot) | median < 250, p99 < 1,500 |
| `m_affix_utilisation` | Σ affix cost / `item_budget` | mean 0.93–0.96; drift → roll bug |

### 1.2 Required dimensions

`realm_id`, `dc_region`, `content_version`, `source`, `rarity`, `ilvl` bucket (10s), level band, difficulty, `loot_mode`, group size class. No p2/p3 data is needed for any itemization metric; GM `ticket_ref` (p2) is excluded from warehouse aggregates.

### 1.3 Telemetry not to emit

Do not emit an event per wear batch, per stat recompute, or per tooltip view. Wear is visible through `item.instance.broken` and `item.instance.repaired`.

## 2. Security considerations

The Security spec owns the threat model, detectors, risk scores and enforcement; this section lists Itemization-specific threats, the server-side validation that answers them, and the signals reported to `anticheat-service`.

### 2.1 Threats and controls

| Threat | Control |
| --- | --- |
| Item duplication by racing moves, trades, mail, or disconnects | single writer (INV-1); per-owner row locks; optimistic `version`; escrow-only cross-owner moves; idempotency keys with request-hash check; partial unique location indexes; nightly ledger replay (INV-7) |
| Stack split/merge arithmetic exploits (overflow, negative quantities) | `CHECK (quantity >= 0)`, int bounds, conservation in one transaction, property tests |
| Forged client loot or reward requests | clients cannot call `GrantItems`, `RollTable`, `IssueGrant`; caller identity via mTLS; source-code allow-list per caller |
| Replay of kill credit / double turn-in | grant idempotency key includes `source_ref`; loot idempotency key includes `context_id` |
| Predicting rolls | seeds are HMACs with a secret epoch key; clients never see seeds or draw counts; `seed_commitment` only |
| Manipulating need/greed (need on unusable items) | server checks usability; window enforced server-side |
| Master-loot abuse / RMT funnels | audit every assignment; `m_master_loot_self_assign_share`; trade window limited to eligible members |
| Gold/item RMT through trades and markets | binding rules; escrow validations; `security.trade.flagged` comes from economy-service; inventory provides lineage on request |
| Bot farming of open-world drops | per-character drop telemetry; anticheat consumes `item.instance.created` with `source = loot_drop` rates per hour |
| Rogue GM or insider grants | `gm_grant` only via admin-portal; two-person rule above `rare` or 1,000 gold; every action to audit-service |
| Equip-swap exploits (stats during combat) | `EquipItem` requires zone-server out-of-combat token; servers use `stats_version` snapshots |
| Tampering with ledger rows | append-only grants, triggers, hash chain verification |
| Content mistakes creating overpowered items | `lsf-itemmath` validator on publish; INV-10 at create; kill switch `ff.item.creation_<definition>` per definition (LiveOps kill switch) |
| Exploited legendary progress (fake events) | progress only from durable domain events of owning services; `AdvanceLegendaryStage` re-checks counters |

### 2.2 Server validation rules (normative)

1. Every create request validates: definition exists in `content_version`, rarity within range and ceiling, ilvl within the ladder for `source`, Σ affix cost ≤ `item_budget`, `source` allowed for caller.
2. Every move validates owner, status (`active`, or `broken` for same-owner moves), binding, container capacity, and `unique_owned`.
3. `ApplyWear` rejects batches for items not equipped by that character at `stats_version`, wear values above `WEAR_MAX_PER_BATCH` (weapon 20, armor 20 per piece per 30 s; `tune.item.wear_max_per_batch`), and duplicate `batch_id`.
4. `RepairItems` must reference a valid quote ≤ 60 s old; the gold debit (economy-service) commits before durability changes.
5. `RollTable` refuses callers other than reward-service and unknown `context_kind`.
6. `IssueGrant` checks source allow-list, lockout eligibility, bundle validity, store-bundle content rules.

### 2.3 Signals reported to `anticheat-service`

Reported through the Security spec's ingestion interface (and visible as domain events):

| Signal | Producer | Trigger |
| --- | --- | --- |
| `itemization.idempotency_mismatch` | all three services | same key, different request hash |
| `itemization.unauthorized_source` | reward-service | `SOURCE_NOT_ALLOWED` |
| `itemization.need_on_unusable` | loot-service | `NEED_NOT_ALLOWED` > 3 per session |
| `itemization.wear_anomaly` | inventory-service | wear batch exceeds limits or wear implausibly low for combat time |
| `itemization.loot_rate_outlier` | analytics-pipeline (from metrics) | character drop rate > 4σ over 24 h |
| `itemization.ledger_mismatch` | reconciler | INV-7 violation (also quarantines) |
| `itemization.rapid_transfer_chain` | inventory-service | same item transferred ≥ 4 owners in 24 h |
| `itemization.version_conflict_burst` | inventory-service | > 20 `VERSION_CONFLICT` per minute for one owner (dupe probing) |

Signal names are proposals for the Security spec's detector catalog (entry file Registry additions).

## 3. Agent implementation guide

### 3.1 Repository and module layout

Per the Repository Structure document (`/BackendServices`). Language and frameworks follow the Backend spec; paths below are normative for structure, not language.

```text
BackendServices/
  libs/
    lsf-itemmath/            # pure, deterministic: B(ilvl), budgets, costs, affix roll, set bonus,
                             # durability/repair, vendor value, pity, loot evaluation, RNG (xoshiro256**), HMAC seeds
      src/ tests/ golden/    # golden vectors shared by all services and tools
    lsf-content-model/       # typed item, affix, item_set, legendary_path, loot, reward definitions + validator
  inventory-service/
    api/proto/lsf/inventory_service/v1/
    src/ (handlers, domain, repo, outbox, consumers/legendary, reconciler, escrow_sweeper)
    migrations/ (inventory schema)
    tests/ (unit, integration, property, load)
  loot-service/
    api/proto/lsf/loot_service/v1/
    src/ (roll, drops, group_modes, pity, aggregates)
    migrations/ (loot schema)
  reward-service/
    api/proto/lsf/reward_service/v1/
    src/ (issue, planner, saga_worker, revocation, authz)
    migrations/ (reward schema)
Tools/
  itemization-sim/           # Monte Carlo: drop rates, pity, gear pace, gold faucet/sink (uses lsf-itemmath)
```

Rule: no formula is implemented twice. Services, the content validator, and the simulator import `lsf-itemmath`.

### 3.2 Build order and milestones

| Milestone | Scope | Exit criteria |
| --- | --- | --- |
| M1 — Math core | `lsf-itemmath` budget, costs, affix roll, RNG, seeds, pity, loot evaluation; golden vectors from this spec's worked examples | all worked examples reproduce exactly; property tests green |
| M2 — Content model | definitions + validator rules (Stat budget part, Validator rules; loot validator) | sample pack validates; invalid fixtures rejected with precise errors |
| M3 — Inventory core | schema, create/move/split/merge/equip, ledger + hash chain, outbox, idempotency | INV-1…INV-9 tests green; ledger replay equals rows |
| M4 — Reward service | issue, plan, saga worker, item + currency lines (economy stub), events | idempotent replays; forced failures recover |
| M5 — Loot service | personal mode, open-world batch, aggregates, pity | Monte Carlo matches expected within 1 % at 10⁶ rolls |
| M6 — Durability and repair | wear batches, broken, quotes, repair via economy-service, salvage via crafting-service, destroy | repair cost examples exact; broken stats zero |
| M7 — Escrow | trade/market/mail holds with economy-service; sweeper | chaos tests: no dupes under kill -9 at every step |
| M8 — Group loot | need/greed, master, council, child grants | concurrency tests on choices; timeouts |
| M9 — Legendary paths | progress consumers, stages, forging, upgrades | full path test with synthetic events |
| M10 — Revocation and reconciliation | `RevokeGrant`, lineage quarantine, nightly reconciler | rollback test across 3 owners and splits |
| M11 — Load and hardening | load targets, anticheat signals, dashboards | §3.5 load criteria met |

### 3.3 Invariants checklist

- [ ] Only inventory-service writes items; only economy-service writes currency (registry authority rule 3).
- [ ] Every item creation has a registry `source` code and `causation_id`; every terminal transition has a sink code (except `merged`).
- [ ] Ledger append-only, hash-chained, one entry per state change; replay equals current row.
- [ ] One location per non-terminal item; escrowed items in exactly one open hold.
- [ ] Σ affix cost ≤ `item_budget`; ilvl ≤ ladder for source; rarity ≤ ceiling; never `mythic`.
- [ ] Rolls are pure functions of (seed inputs, `content_version`); seeds never leave the server.
- [ ] Every mutating RPC is idempotent with request-hash check.
- [ ] Grant plans are persisted before any side effect; re-delivery never re-rolls.
- [ ] Pity only on personal-scope keyed entries.
- [ ] `store_purchase` grants never contain stat-budget items, XP, LP, gold, or loot.
- [ ] Events published via transactional outbox using the registry envelope; `event_id` unique.

### 3.4 Test matrix

| Level | Test cases (concrete) |
| --- | --- |
| Unit | `B(460) = 41.814 ± 0.001`; Example A chest yields str 12, con 12, crit 33, parry 31, move 240 bp, armor 1,443 with the given `r` values; Example D crafted ilvl 418, roll floor 0.9808; repair 42,400 / 88,333 copper; vendor value 2,097; pity `p_eff(60) = 0.12` for p0 0.02; personal chance 0.3125; set bonus `SB(2) = 6.693`; luck 75 → bonus 0.05 |
| Unit | state transitions: every allowed `item_status` edge succeeds, every other edge fails with typed error; `broken` cannot escrow; legendary cannot be salvaged/destroyed by players |
| Property-based | for random definitions/ilvl/rarity/seed: Σ cost ≤ budget; determinism (two runs identical); no exclusivity group twice; stack split/merge conserves quantity for random op sequences; ledger replay equals state after random op sequences; loot evaluation never exceeds depth and never yields entries failing conditions |
| Property-based | pity: for random p0 ∈ [0.001, 0.2], failures never exceed `h − 1`; mean attempts < 1/p0 |
| Statistical | 10⁶ rolls per sample table: observed frequency within 3σ of manifest expectation; affix weights frequency within 3σ |
| Integration | grant saga with economy/progression/faction fakes injecting timeouts and 5xx → eventually `delivered`, each line exactly once (count downstream effects) |
| Integration | trade escrow: cancel, timeout, double accept, accept-after-release → no item in two owners; `TransferEscrow` replay idempotent |
| Integration | group need/greed with 5 clients submitting concurrently and late; all pass → greed; nobody → gold conversion |
| Integration | revocation: grant → split stack → trade half → consume part → `RevokeGrant` quarantines lineage on both owners and reports deficit |
| Integration | legendary: synthetic `quest.instance.completed`, `world.event.ended`, `encounter.attempt.defeated`, `crafting.item.crafted` → forged item ilvl 465 → two upgrades → ilvl 475, affix values scaled, gold debited with sink `legendary_upgrade_cost` |
| Chaos | kill inventory-service mid-transaction at each step of move/trade/grant (fault injection) → restart → reconciler reports 0 anomalies |
| Load | inventory: 5,000 mutating RPC/s per realm DB, p99 < 50 ms; reward: 2,000 grants/s, p99 issue < 100 ms, delivered p99 < 2 s; loot: 20,000 rolls/s (batch) p99 < 20 ms per batch of 100; raid boss kill with 25 personal rolls end-to-end < 1 s |

### 3.5 Acceptance criteria

1. All worked examples in the Stat budget part, Item model part, and Loot and Rewards part reproduce exactly from `lsf-itemmath` golden tests.
2. Zero reconciliation anomalies after the chaos suite and after a 24-hour soak at load targets.
3. Every event in registry Event taxonomy for `item`, `inventory`, `loot`, `reward` is emitted with the documented payload, registered in the schema registry, and validated by a contract test.
4. Content validator rejects every invalid fixture category listed in the Stat budget part, Validator rules, and the loot validator rules.
5. Kill switches `ff.item.creation_<definition>`, `ff.loot.pity_enabled`, `ff.item.vendor_stock_enabled` take effect within the LiveOps kill-switch SLA without restarts.
6. Revocation across lineage works for splits, trades, mail and market escrow.

### 3.6 Pitfalls

- Rounding: keep `item_budget` as decimal; floor only final stat values; never round intermediate slices.
- Floating point in money: repair and vendor values are computed in decimal/float but stored and charged as integer copper with `ceil`/`floor` exactly as specified.
- RNG draw order drift: adding a draw (e.g., new rarity step) silently changes every replay — bump `roll_algorithm_version` and keep the old path.
- Content version skew: always roll with the `content_version` pinned in the grant/roll context, not "latest".
- Reading from replicas for writes: equipment validation and moves must read the primary.
- Treating `merged` as a sink inflates sink metrics.
- Sending wear per hit: batch it.
- Letting loot-service call other services mid-roll: breaks determinism; use the context snapshot.
- Forgetting `allow_overflow`: full bags must never fail a grant.
- Revoking by destroying third-party items without a case decision: quarantine first.

### 3.7 Do / Don't for coding agents

| Do | Don't |
| --- | --- |
| Use registry names exactly (services, enums, events, prefixes, source/sink codes) | Invent new enum values or event names without listing them as Registry additions |
| Put every formula in `lsf-itemmath` with the tuning key as a parameter | Hard-code constants in services; read them from `config-service` tuning tables |
| Write ledger + row + outbox in one transaction | Publish events directly to the bus from handlers |
| Make every mutating handler idempotent and test replay | Rely on client-side deduplication |
| Use escrow for any cross-owner move | Update `owner_id` directly in a trade handler |
| Pin `content_version` per roll and grant | Re-evaluate amounts or re-roll on retry |
| Add golden tests for each worked example you touch | Change a formula without updating this spec and the golden vectors |
| Keep `mythic` rarity rejected | Ship a `mythic` item definition "for testing" in a prod pack |
| Run only targeted tests for the module changed (AGENTS.md) | Run the full suite unless asked |
| Update `docs/dependency-map.md` when you add services, routes, or commands (in the change that adds them) | Edit the registry from a domain change |
