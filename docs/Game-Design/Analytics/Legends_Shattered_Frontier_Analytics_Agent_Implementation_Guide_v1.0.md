# Legends of the Shattered Frontier — Analytics Part 4: Agent Implementation Guide
## Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per Shared Registry v1.0.

- Entry file: [Analytics Architecture](../Legends_Shattered_Frontier_Analytics_Architecture_v1.0.md)
- Registry: [Shared Registry v1.0](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

Guidance for coding agents implementing `telemetry-ingest`, `analytics-pipeline`, `analytics-warehouse`, the `lsf-events` SDK and the schema registry. The current Node.js proof of concept (`server/`, `public/`) implements none of this; do not retrofit it.

---

## 1. Repository and module layout

```text
analytics/
  sdk/
    lsf-events-core/        # envelope types, ULID, privacy-class computation, JSON Schema validation
    lsf-events-outbox/      # transactional outbox table + relay (PostgreSQL → event-bus)
    lsf-events-ue/          # UE5.8 C++ module: client telemetry buffer, batching, consent, server telemetry
  schemas/
    <domain>/<event_name>/v<schema_version>.json   # JSON Schema 2020-12 with x-privacy-class annotations
    CODEOWNERS              # owning spec reviewers + analytics reviewer
  services/
    telemetry-ingest/       # HTTP intake, scrubbers, schema registry API (lsf.telemetry_ingest.v1)
      migrations/           # PostgreSQL schema telemetry_ingest
  pipeline/
    stream/                 # Flink jobs: ap-raw-writer, ap-enricher, ap-sessionizer, ap-alert-evaluator, ap-metrics-bridge
    batch/                  # Spark jobs: ap-dedup-compactor, ap-privacy-worker, ap-batch-detectors, ap-reports
    models/                 # SQL models: clean → modeled (one file per table)
    metrics/<domain>/<metric_name>.sql   # one SQL model per catalog metric + YAML header
    alerts/<rule_id>.yaml   # alert rules (synced to analytics_pipeline.alert_rules)
    experiments/            # analysis library (z-test, Welch, CUPED, delta method, SRM)
    migrations/             # PostgreSQL schema analytics_pipeline
    api/                    # MetricsService and PrivacyService (lsf.analytics_pipeline.v1)
  warehouse/
    iceberg/                # DDL for raw, clean, modeled, restricted namespaces
    clickhouse/             # DDL for live tables and metric store
    catalog/                # Iceberg REST catalog config; PostgreSQL schema analytics_warehouse
  dashboards/               # Grafana/Superset definitions as code
  tests/
    fixtures/events/        # golden events per event_name/version (valid + invalid)
    property/  integration/  load/  privacy/
```

Metric file header (YAML front matter, validated in CI against `analytics_pipeline.metric_definitions`):

```yaml
metric_name: m_gold_faucet_sink_ratio
metric_version: 1
owner_spec: Economy
kind: ratio
grains: [day, week]
dimensions: [dc_region, realm_id]
freshness_class: daily
status: provisional        # Economy payloads not yet defined
trl_allowed: true
guardrail_allowed: false
client_sourced: false
```

---

## 2. Build order and milestones

| Milestone | Deliverable | Depends on | Exit criteria |
| --- | --- | --- | --- |
| A1 Envelope SDK | `lsf-events-core`, outbox relay, UE module skeleton | Registry envelope | Golden-envelope tests pass in all SDK languages; ULID/prefix checks; null fields omitted |
| A2 Schema registry | `telemetry_ingest` schema, Register/Approve/Get/CheckCompatibility, CI check in owning repos | A1 | All payload tables of Character, Itemization, Quest, Dungeon, Raid, Security, LiveOps specs registered as `active` v1; compatibility matrix tests green |
| A3 Ingest | `telemetry-ingest` client path, scrubbers, quotas, clock correction, integrity forwarding | A2, gateway | Load test at Phase 2 capacity target; privacy tests green; `report_signature` never on bus copy |
| A4 Raw + clean | `ap-raw-writer`, `ap-enricher`, dedup, rejected-events table | A2 | E1–E9 enforced; duplicates < 0.001 % in clean; freshness `realtime` met |
| A5 Sessions + engagement | `ap-sessionizer`, `fact_sessions`, `fact_daily_activity`, DAU/WAU/MAU, retention, churn | A4 | Worked examples in the Pipeline and Warehouse parts reproduce exactly |
| A6 Serving + metric API | ClickHouse metric store, materializer, `MetricsService` | A5 | LiveOps TRL and config-service guardrail calls served p99 ≤ 200 ms, `stale` flag correct |
| A7 Domain facts | progression, quest, instance/encounter, item/loot/reward, group, liveops, security facts | A4 | Every adopted metric of sibling specs computes on fixture data |
| A8 Alerts + dashboards | `ap-alert-evaluator`, `ops.alert.fired`, rules, boards | A6, A7 | Each rule fires and resolves on synthetic data; routes delivered |
| A9 Economy | currency/market facts, snapshots, inflation index, Gini, velocity, FSR | Economy payloads (interim contract) | Worked examples reproduce; residual check equals Security INV-8 result on fixtures |
| A10 Batch detectors host | feature tables, scheduling, `SubmitBatchDetections` | A7, A9, Security S9 | Security worked examples (trade funnel 23.6) reproduce; idempotent resubmission |
| A11 Experimentation | assignment recomputation, analysis library, readouts | A6, config-service | Re-bucketing matches config-service evaluation for 1 M random accounts; SRM detection works |
| A12 Privacy | `PrivacyService`, erasure and export workers, DLP scans, retention jobs | A4–A7 | Erasure removes all rows physically within SLA in integration test; export bundle complete |
| A13 Reports + scale | daily/weekly/seasonal reports, Phase 3 load test, cost dashboards | all | Phase 3 peak sustained 1 h with lag < 60 s; cost per CCU-day within target |

---

## 3. Invariants checklist

| ID | Invariant | Where enforced | Test |
| --- | --- | --- | --- |
| AN-1 | Every accepted event appears in `raw` exactly once per `event_id` after compaction | raw writer + compactor | property P1, integration I2 |
| AN-2 | No `p3_sensitive` value and no full IP is ever written to Iceberg or ClickHouse | scrubbers, schema annotations | privacy suite, DLP scan |
| AN-3 | `account_id`/`session_id` on client events always equal the token's identity | `telemetry-ingest` | U5, I1 |
| AN-4 | `p2_personal` columns exist only in `restricted` in the home dc_region and are deleted after 30 days | pipeline routing, retention job | privacy suite |
| AN-5 | A metric's ratio at a coarse grain equals Σ numerators / Σ denominators of finer grains | materializer | property P4 |
| AN-6 | `m_gold_ledger_residual` = 0 on consistent fixtures, and any injected unbalanced credit makes it ≠ 0 | economy models | property P5 |
| AN-7 | Erasure leaves zero rows for subject IDs in all row-level tables after snapshot expiry | privacy worker | I7 |
| AN-8 | Schema versions within a major are backward compatible; retired versions are rejected | registry | U2, P2 |
| AN-9 | Deterministic re-bucketing equals config-service evaluation | experiments library | P6 |
| AN-10 | Alerts carry no subject IDs; subject-level findings go only through `SubmitBatchDetections` | evaluator, schema of `ops.alert.fired` | U9 |
| AN-11 | Automation-facing metrics (`trl_allowed`, `guardrail_allowed`) are server-sourced, except crash rate with its guards | catalog CI | U10 |
| AN-12 | Metric responses during an SLA breach are flagged `stale` | MetricsService | I5 |

---

## 4. Test matrix

### 4.1 Unit

| # | Case | Expected |
| --- | --- | --- |
| U1 | Envelope with `character_id: null` | SDK omits the field; validator rejects explicit `null` |
| U2 | Register v2 of `quest.instance.completed` that adds a required field in major 1 | `CheckCompatibility` fails with `required_added` |
| U3 | `occurred_at` 10 min in the future from a server producer | rejected E4 |
| U4 | Client batch header offset +45 s | all `occurred_at` shifted by −45 s, `_client_occurred_at` preserved |
| U5 | Client event carrying another account's `account_id` | overwritten by token identity, flagged `context_mismatch` |
| U6 | Survey free text "mail me at a@b.com or call 555-0100" | stored as "mail me at [redacted:email] or call [redacted:phone]" in `restricted` only |
| U7 | `client.integrity.reported` | full report to anticheat-service; bus copy lacks `report_signature` |
| U8 | Sessionization worked example (Pipeline part) | merged 150 min; active 1.83 h; played 2.22 h |
| U9 | `ops.alert.fired` payload builder given a subject list | emits `subject_count` only |
| U10 | Catalog CI with a `client_sourced` metric marked `trl_allowed` | build fails (crash rate whitelisted) |
| U11 | Inflation index worked example | 106.4 and +2.31 % w/w |
| U12 | Gini worked example | 0.64; all-zero wealth → 0 |
| U13 | Retention worked example | 45.2 % / 24.1 % / 11.8 % |
| U14 | Sample size worked example | 13,067 per arm |
| U15 | Flow classification of each Registry source/sink code | matches the flow-class table; unknown code → reject model build |

### 4.2 Property-based

| # | Property |
| --- | --- |
| P1 | For any multiset of events with duplicates (same `event_id`, same body), clean contains each `event_id` once |
| P2 | For any schema evolution consisting only of allowed changes, all historical fixture payloads validate against the newest version of the major |
| P3 | For any random sequence of heartbeats and session events, play sessions never overlap for one character and `active ≤ played ≤ merged duration` |
| P4 | For random partitions of a day into hours, ratio metrics computed from summed parts equal the day computation |
| P5 | For any balanced random ledger, residual = 0; adding one unmatched credit of x makes residual = x |
| P6 | For random `account_id` and salt, offline bucket = config-service bucket |
| P7 | Gini ∈ [0, 1 − 1/n] for non-negative wealth; invariant under scaling all wealth by a constant |
| P8 | Laspeyres index = 100 when all prices equal base; scaling all prices by k yields 100k |
| P9 | Scrubber output never matches the p3/IP DLP regex set for random generated inputs containing such values |

### 4.3 Integration

| # | Scenario | Expected |
| --- | --- | --- |
| I1 | Game client (headless test harness) → gateway → ingest → bus → raw/clean | events queryable in ClickHouse ≤ 60 s; identity stamped |
| I2 | Outbox relay crash mid-batch, restart | no loss; duplicates removed in clean |
| I3 | Late client batch 3 days old | partitions marked dirty; daily metrics restated |
| I4 | Quest flow from Quest spec fixtures (offered → accepted → objectives → completed) | `fact_quest_instances` row complete; funnel metrics correct |
| I5 | Pause `ap-enricher` for 6 min | `MetricsService` returns `stale = true`; `pipeline_freshness` alert fires; config-service holds |
| I6 | Inject 3× hourly gold from one source on a realm | `economy_gold_created_spike` and `economy_source_spike` fire, routes include `anticheat_service`, resolve after 2 clean windows |
| I7 | Erasure for an account with characters, trades, and survey text | zero rows after snapshot expiry; counterparty fields "erased"; audit record appended |
| I8 | Erasure with open enforcement case | security rows moved to `restricted_hold`, others erased |
| I9 | Rollout guardrail sample via `GetGuardrailSample` for a 5 % account rollout | x/n per side match manual SQL on fixtures |
| I10 | Batch detector run twice for same window | single detection per subject in anticheat-service (idempotent) |

### 4.4 Load

| # | Scenario | Target |
| --- | --- | --- |
| L1 | Phase 2 peak 6,300 events/s for 2 h, then 3× burst for 10 min | ingest p99 ≤ 150 ms; lag p95 ≤ 60 s; zero loss |
| L2 | Phase 3 planning peak 31,500 events/s for 1 h | same targets with documented scale-out (partitions, shards) |
| L3 | Maintenance re-login wave: 25,000 sessions starting in 5 min | sessionizer keeps up; DAU panel correct |
| L4 | Dashboard concurrency: 100 users on live boards | p95 query ≤ 2 s |
| L5 | Daily batch on 25 months of Phase 3 data | daily models finished by reset + 2 h |

---

## 5. Acceptance criteria

1. Every Registry event family has a registered schema (owning-spec payload or interim projection) and a data contract.
2. Every metric name proposed in sibling specs resolves to exactly one catalog entry or a documented alias (Warehouse and metrics part, Duplicate resolution).
3. All worked examples in this spec reproduce in unit tests.
4. Freshness SLAs and event-loss targets hold in L1.
5. Privacy suite: zero p3 values, zero raw IPs, p2 only in `restricted`, erasure SLA met.
6. Every alert rule in the Dashboards, alerting and privacy part fires and resolves in an integration test and routes correctly.
7. LiveOps TRL `metric()` and config-service guardrails work end to end against the MetricsService.
8. Security batch detector worked examples reproduce through the batch host.

---

## 6. Pitfalls

- **Averaging ratios.** Daily retention averaged across realms is not global retention; sum numerators and denominators.
- **UTC days vs reset days.** Activity days follow the dc_region daily reset; mixing in UTC midnight shifts DAU by up to 22 hours of activity in `apac`.
- **Counting transfers as faucets.** `market_sale` and `trade_receive` move gold between players; including them inflates faucets and the FSR.
- **Trusting the client clock or IDs.** Always use ingest-stamped identity and corrected time.
- **Using unregistered events as canonical.** Registered progression and raid events may feed their catalog metrics; `instance.mythic_progress` remains blocked until its snapshot payload and producer contract are registered.
- **Per-account experiments on power or economy.** Use realm randomization.
- **Joining restricted data into modeled tables.** p2 values must never be copied out of `restricted`; only aggregates leave.
- **Deleting by `DELETE` without snapshot expiry.** Iceberg deletes are logical until snapshots expire and files are rewritten.
- **Alert storms.** Always scope rules (`scope_dims`) and use the fingerprint; never alert per character.
- **Silent schema drift.** Unknown fields are stripped in prod; watch `fields_stripped` counts after every release.

---

## 7. Do / Don't for coding agents

| Do | Don't |
| --- | --- |
| Use Registry names exactly: services, events, enums, prefixes, source/sink codes | Invent event names, topics, or enum values; propose them in Registry additions instead |
| Read payload fields from the owning spec's event tables and cite them in the metric file | Redefine another spec's payload or add fields to it from the pipeline side |
| Keep one SQL model per metric with tests from the worked examples | Hard-code metric formulas in dashboards or alert rules |
| Store money as integer copper end to end | Use floating point for currency amounts (floats only for derived ratios and indexes) |
| Deduplicate by `event_id` at every layer | Assume exactly-once delivery |
| Classify every new field with `x-privacy-class` and a transform | Write a p2/p3 value outside `restricted`, or log payloads in job logs |
| Flag metrics `provisional` when inputs come from interim contracts | Mark a metric `trl_allowed` if it depends on client telemetry |
| Emit `ops.alert.fired` only from `ap-alert-evaluator` | Put account or character IDs in alerts |
| Route subject-level findings through `SubmitBatchDetections` | Apply or recommend sanctions from analytics code |
| Recompute experiment assignment deterministically | Add a per-request exposure call to config-service hot paths |
| Restate reports with a version note | Silently overwrite published report numbers |
