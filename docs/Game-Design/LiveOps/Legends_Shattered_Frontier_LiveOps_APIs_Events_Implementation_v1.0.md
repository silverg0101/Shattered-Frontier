# Legends of the Shattered Frontier — LiveOps Part 4: APIs, Events and Implementation
## Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per Shared Registry v1.0.

- Entry file: [LiveOps Architecture](../Legends_Shattered_Frontier_LiveOps_Architecture_v1.0.md)
- Registry: [Shared Registry v1.0](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

---

## 1. gRPC APIs

Conventions (Registry, Service registry): every mutating RPC takes `idempotency_key` (≤ 128 chars, retained ≥ 7 days, replay returns the original response). Admin RPCs additionally take `AdminContext`. Errors use gRPC status codes with a domain `reason` string (e.g., `FAILED_PRECONDITION / cap_exceeded`).

```protobuf
syntax = "proto3";
package lsf.liveops_service.v1;

message AdminContext {
  string actor_account_id = 1;           // acc_ of operator
  repeated string approver_account_ids = 2;
  string reason_code = 3;
  string ticket_ref = 4;
}

message Cohort {
  string environment = 1;                 // dev|ptr|staging|prod
  repeated string dc_regions = 2;         // na|eu|apac|sa
  repeated string realm_ids = 3;
  uint32 min_character_level = 4;
}
```

### 1.1 `lsf.liveops_service.v1.LiveOpsService`

| RPC | Request (key fields) | Response | Callers | Notes |
| --- | --- | --- | --- | --- |
| `CreateEvent` | `idempotency_key`, `AdminContext`, `definition_id`, `Cohort`, `starts_at`, `ends_at` (or `arming_ends_at`), `param_overrides` | `event_id`, `state=draft`, `faucet_estimate`, `approval_tier`, `violations[]` | admin-portal | Validates caps; overrides only within definition envelope |
| `ApproveEvent` | `idempotency_key`, `AdminContext`, `event_id`, `expected_version` | `state=scheduled` | admin-portal | Approver ≠ creator for tier ≥ 2 |
| `RevokeApproval` | `event_id`, `expected_version` | `state=draft` | admin-portal | Only before `starts_at` |
| `CancelEvent` | `event_id`, `reason_code` | `state=cancelled` | admin-portal | Not allowed when `active` |
| `EndEvent` | `event_id`, `outcome=ended_early` | `state=ended` | admin-portal | Removes overlays |
| `GetEvent` / `ListEvents` | filters: state, cohort, time range, season | events | admin-portal, gateway (public subset) | |
| `EstimateFaucet` | `definition_id`, `Cohort`, params | per-realm `E_gold`, `B_gold`, max compliant multipliers | admin-portal | Pure; no side effects |
| `CreateSeason` | `definition_id`, per-region `starts_at` | `season_id`, `state=upcoming` | admin-portal | Pushes rotation schedule to instance-service |
| `ExtendSeason` | `season_id`, `weeks` (≤ 4) | new region windows | admin-portal | Tier 3 approval |
| `GetSeasonProgress` | `season_id`, `character_id` | `sp_total`, `tier`, lane grants, weekly caps | gateway (client), quest-service (read) | |
| `GetActiveSeason` | `environment`, `dc_region` | season summary | gateway, all services | Cached |
| `CreateRelease` / `CutCandidate` | builds, `content_version`, migrations, `min_client_build` | `release_id` | CI (service identity), admin-portal | |
| `DeployToStage` | `release_id`, `stage`, scope, `maintenance_window_id` | deployment handle | admin-portal (`release_manager`) | Orchestrates DevOps pipeline |
| `RollbackRelease` | `release_id`, `stage`, scope | state `rolled_back` | admin-portal | Restores previous release & content version |
| `ScheduleMaintenance` / `ExtendMaintenance` | scope, window, kind | `maintenance_window_id` | admin-portal | |
| `SetRealmRole` | `realm_id`, `canary|standard` | ok | admin-portal | One canary per region |
| `PublishAnnouncement` / `WithdrawAnnouncement` | channels, cohort, locales, window | `announcement_id` | admin-portal, internal automation | |
| `GetMotd` | `realm_id`, `locale` | text, `announcement_id` | gateway | Cached ≤ 60 s |
| `CreateSurvey` / `PublishSurvey` / `CloseSurvey` | survey definition | `survey_id` | admin-portal | |
| `GetSurveyOffer` | `account_id`, `trigger_moment` | survey or none | gateway | Applies sampling & frequency cap |
| `CreateCompensation` / `SubmitCompensation` / `ApproveCompensation` / `StartIssuance` / `RetryFailed` / `CancelCompensation` | see Store, Compensation and Community, Compensation grants | `compensation_id`, state, counts | admin-portal | |
| `GetCompensation` | `compensation_id` | progress | admin-portal | |
| `ListCalendar` | time range, cohort, `audience=internal|player` | merged calendar items | admin-portal, gateway | |
| `GetStatus` | `dc_region` | components and `service_status` | status page publisher, gateway | |

### 1.2 `lsf.config_service.v1.ConfigService`

| RPC | Request (key fields) | Response | Callers | Notes |
| --- | --- | --- | --- | --- |
| `RegisterKeys` | key declarations (build-time) | accepted / diff | CI | Rejects incompatible re-declarations (type change) |
| `GetSnapshot` | `environment`, `service`, `realm_id`, `client_visible_only` | `config_version`, keys, rules, overlays, kills | SDKs, gateway | |
| `WatchConfig` (server stream) | `since_config_version`, filter | stream of deltas | SDKs, gateway | Priority lane for kills/rollbacks |
| `AckVersion` | `process_id`, `config_version` | ok | SDKs | |
| `UpdateFlagRules` | `idempotency_key`, `AdminContext`, `key`, `environment`, `rules[]`, `expected_config_version` | `change_id`, `config_version` | admin-portal | Creates rollout if required |
| `PublishTuning` | `idempotency_key`, `AdminContext`, `key`, `environment`, `value`, `expected_tuning_version` | `change_id`, `tuning_version` | admin-portal | Validates schema & invariants |
| `Kill` / `Unkill` | `key`, `environment`, cohort | `change_id` | admin-portal, automation (enforcement_lead playbooks) | Kill single-person; unkill two-person |
| `ApplyOverlay` / `RemoveOverlay` | `owner_ref` (`lop_`), key, cohort, value, `expires_at` | `overlay_id` | liveops-service only | Value ≤ declared max |
| `CreateRollout` / `StartRollout` / `AdvanceRollout` / `PauseRollout` / `ResumeRollout` / `RollbackRollout` | `rollout_id` | state | admin-portal, internal evaluator | |
| `ReportGuardrailSample` | `rollout_id` or `owner_ref`, metric, treatment/control counts | evaluation result | analytics-pipeline | |
| `GetPropagation` | `config_version` | ack ratio by service/realm | admin-portal | |
| `GetHistory` | key, environment, range | changes | admin-portal, audit | |
| `Evaluate` (debug) | key, context | value + matched rule | admin-portal | Never used on hot paths |

### 1.3 `lsf.store_service.v1.StoreService`

| RPC | Request (key fields) | Response | Callers | Notes |
| --- | --- | --- | --- | --- |
| `GetCatalog` | `account_id`, `dc_region`, `locale` | offers with prices, placements, owned flags | gateway | Emits `store.offer.viewed` on offer detail view only |
| `ViewOffer` | `account_id`, `offer_id`, `surface` | offer detail | gateway | Telemetry hook |
| `CreatePurchase` | `idempotency_key`, `account_id`, `offer_id`, `recipient_account_id?`, `expected_price` | `purchase_id`, state, checkout URL (crown packs) | gateway | Price mismatch → `ABORTED / price_changed` |
| `HandlePspWebhook` | raw body, signature headers | 2xx/4xx | PSP (via gateway edge) | Signature verified; dedup by `psp_event_id` |
| `GetPurchase` / `ListPurchases` | `account_id` | purchases | gateway, admin-portal | |
| `RequestRefund` | `purchase_id`, `kind=self_service` | state | gateway | Policy checks |
| `RefundPurchase` | `AdminContext`, `purchase_id`, kind | state | admin-portal (`store_manager`) | |
| `UpsertOfferPrice` / `UpsertPlacement` | `AdminContext`, offer, region, price/slot, window | ok | admin-portal | Pricing guardrails |
| `SetAccountStoreHold` | `AdminContext`, `account_id`, reason | ok | admin-portal, enforcement-service | |
| `Reconcile` (internal) | range | discrepancies | scheduler | Every 15 min |

### 1.4 Calls made by LiveOps services to other services (contract summary)

| Target | RPC (owned by target spec) | Used for |
| --- | --- | --- |
| `reward-service` | `IssueGrant`, `RevokeGrant`, `GetGrant` | Season, event, compensation, store entitlements |
| `economy-service` | `Credit`, `Debit`, `GetWallet` | Crowns, season token conversion/expiry |
| `quest-service` | `ScheduleCampaign`, `EndCampaignWindow`, `CancelCampaign`, `GetCampaign` | Campaign windows |
| `ai-director-service` | `RequestWorldEvent`, `CancelWorldEventRequests`, `SetDirectorBias` | World event interplay |
| `instance-service` | `SetRotationSchedule`, `GetRotation` | Affixes and raid tiers |
| `content-catalog-service` | `ValidatePack`, `ActivateContentVersion`, `CheckRollbackSafety` | Releases |
| `world-service` | `DrainRealms`, `GetRealms`, `SetRealmRole` | Maintenance, canaries |
| `gateway` | `SetLoginGate`, `SetMinClientBuild`, `PushClientNotice` | Maintenance, launcher coordination |
| `character-service`, `auth-service` | `ListActiveCharacters`, `ListSessionsOverlapping` | Compensation targeting |
| `anticheat-service` | `ReportSignal`, `GetRiskScore` | Fraud and abuse |
| `audit-service` | `Append` | Every admin mutation |

Where these RPCs are not yet named in the target spec, they are LiveOps requirements on that spec and must be confirmed there.

## 2. Event payloads

All events use the Registry envelope (Registry, Envelope); only `payload` is defined here. `schema_version = 1` for all. Topic `domain.liveops`, `domain.config`, `domain.store`; partition key in brackets. Privacy classes: operational events about operator actions are `p1_pseudonymous` (operator `acc_` ids); store purchase events are `p2_personal` at envelope level because they link an account to spending.

### 2.1 `liveops.event.scheduled` [key `event_id`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `event_id` | string (`lop_`) | yes | Runtime event | p0_public |
| `definition_id` | string | yes | `liveops_event/<slug>` | p0_public |
| `kind` | `liveops_event_kind` | yes | | p0_public |
| `season_id` | string (`ssn_`) | no | | p0_public |
| `cohort` | object | yes | `{environment, dc_regions[], realm_ids[], min_character_level}` | p0_public |
| `starts_at` / `ends_at` | timestamp | cond. | Absent for armed triggered events | p0_public |
| `arming_ends_at` | timestamp | cond. | Triggered/dynamic | p0_public |
| `modifiers` | array | yes | `[{kind, value, scope}]` resolved | p0_public |
| `approval_tier` | int | yes | 1–3 | p1_pseudonymous |
| `approved_by` | string[] (`acc_`) | yes | Operators | p1_pseudonymous |
| `faucet_estimate_copper` | int64 | no | Sum `E_gold` across realms per day | p1_pseudonymous |

### 2.2 `liveops.event.started` [key `event_id`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `event_id` | string | yes | | p0_public |
| `definition_id` | string | yes | | p0_public |
| `kind` | enum | yes | | p0_public |
| `realm_ids` | string[] | yes | Realms where started | p0_public |
| `excluded_realms` | object[] | no | `[{realm_id, reason}]` | p0_public |
| `resolved_params` | object | yes | Final modifier values (dynamic computed) | p0_public |
| `trigger_inputs` | object | cond. | TRL input values at firing | p1_pseudonymous |
| `overlay_change_ids` | string[] (`cfg_`) | yes | Config overlays applied | p1_pseudonymous |
| `ends_at` | timestamp | yes | | p0_public |

### 2.3 `liveops.event.ended` [key `event_id`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `event_id` | string | yes | | p0_public |
| `definition_id` | string | yes | | p0_public |
| `outcome` | enum | yes | `completed`, `ended_early`, `guardrail_stopped`, `completed_with_campaign`, `cancelled` | p0_public |
| `ended_by` | string (`acc_`) | no | Operator when manual | p1_pseudonymous |
| `guardrail` | object | no | `{metric, value, threshold, realm_id}` | p1_pseudonymous |
| `participants` | int | yes | Characters with ≥ 1 point | p0_public |
| `tier_grants` | int | yes | Reward grants issued | p0_public |
| `event_gold_created_copper` | int64 | no | Attributed gold | p1_pseudonymous |
| `duration_seconds` | int | yes | Actual active duration | p0_public |

### 2.4 `liveops.season.started` [key `season_id`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `season_id` | string (`ssn_`) | yes | | p0_public |
| `definition_id` | string | yes | `season/<slug>` | p0_public |
| `ordinal` | int | yes | Season number | p0_public |
| `dc_region` | enum | yes | Region that started (one event per region) | p0_public |
| `starts_at` / `ends_at` | timestamp | yes | Region window | p0_public |
| `raid_tier` | object | yes | Raids and difficulty open weeks | p0_public |
| `track_tiers` | int | yes | 100 | p0_public |

### 2.5 `liveops.season.ended` [key `season_id`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `season_id` | string | yes | | p0_public |
| `dc_region` | enum | yes | | p0_public |
| `characters_with_progress` | int | yes | | p0_public |
| `tier_distribution` | object | yes | `{p10, p50, p90, completed_ratio}` | p0_public |
| `tokens_converted` | int64 | yes | Total tokens converted | p1_pseudonymous |
| `tokens_expired` | int64 | yes | Total tokens expired | p1_pseudonymous |
| `gold_from_conversion_copper` | int64 | yes | | p1_pseudonymous |
| `extended_weeks` | int | yes | 0–4 | p0_public |

### 2.6 `liveops.release.deployed` [key `release_id`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `release_id` | string (`rel_`) | yes | | p0_public |
| `version` | string | yes | Semantic version | p0_public |
| `stage` | `release_stage` | yes | | p0_public |
| `scope` | object | yes | Realms/regions | p0_public |
| `deployment_kind` | enum | yes | `deploy`, `rollback` (until `liveops.release.rolled_back` merges) | p0_public |
| `server_build` | string | yes | | p0_public |
| `client_build` / `min_client_build` | int | no | | p0_public |
| `content_version` | int64 | yes | | p0_public |
| `maintenance_window_id` | string (`mnt_`) | no | | p0_public |
| `downtime_seconds` | int | yes | 0 for rolling | p0_public |
| `release_manager` | string (`acc_`) | yes | | p1_pseudonymous |

### 2.7 `liveops.announcement.published` [key `announcement_id`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `announcement_id` | string (`ann_`) | yes | | p0_public |
| `channels` | string[] | yes | `announcement_channel` values | p0_public |
| `cohort` | object | yes | | p0_public |
| `priority` | enum | yes | `info`, `important`, `critical` | p0_public |
| `locales` | string[] | yes | Locales published (not texts) | p0_public |
| `linked_ref` | string | no | `lop_`/`ssn_`/`rel_`/`mnt_`/`cmpg_` | p0_public |
| `starts_at` / `ends_at` | timestamp | yes/no | | p0_public |
| `published_by` | string (`acc_`) | yes | | p1_pseudonymous |

### 2.8 `liveops.survey.published` [key `survey_id`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `survey_id` | string (`srv_`) | yes | | p0_public |
| `question_count` | int | yes | | p0_public |
| `question_ids` | string[] | yes | Stable ids used by `client.survey.submitted` | p0_public |
| `cohort` | object | yes | | p0_public |
| `sample_rate` | number | yes | | p0_public |
| `trigger_moment` | string | yes | | p0_public |
| `starts_at` / `ends_at` | timestamp | yes | | p0_public |
| `has_incentive` | bool | yes | | p0_public |

### 2.9 `liveops.compensation.granted` [key `compensation_id`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `compensation_id` | string (`cmpg_`) | yes | | p1_pseudonymous |
| `reason_code` | enum | yes | | p0_public |
| `incident_ref` | string | no | | p1_pseudonymous |
| `tier` | int | yes | 1–4 | p0_public |
| `bundle_id` | string | yes | `reward/<slug>` | p0_public |
| `recipients_total` / `delivered` / `failed_permanent` | int | yes | Counts | p1_pseudonymous |
| `gold_total_copper` | int64 | yes | Sum credited | p1_pseudonymous |
| `crowns_total` | int64 | yes | 0 if none | p1_pseudonymous |
| `final_state` | enum | yes | `completed`, `partially_failed` | p1_pseudonymous |
| `approved_by` | string[] | yes | | p1_pseudonymous |

### 2.10 `config.flag.changed` [key `key`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `change_id` | string (`cfg_`) | yes | | p1_pseudonymous |
| `key` | string | yes | `ff.*` | p0_public |
| `environment` | enum | yes | | p0_public |
| `config_version` | int64 | yes | | p0_public |
| `change_kind` | enum | yes | See Config and Releases, Versioning | p0_public |
| `kill_switch` | bool | yes | | p0_public |
| `before` / `after` | object | yes | Rules summary `{default, rules:[{priority, cohort, value}]}` | p0_public |
| `rollout_id` | string | no | | p1_pseudonymous |
| `owner_ref` | string | no | `lop_` for overlays | p0_public |
| `actor` / `approvers` | string / string[] | yes | `acc_` | p1_pseudonymous |
| `reason_code` | string | yes | | p1_pseudonymous |

### 2.11 `config.tuning.published` [key `key`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `change_id` | string (`cfg_`) | yes | | p1_pseudonymous |
| `key` | string | yes | `tune.*` | p0_public |
| `environment` | enum | yes | | p0_public |
| `tuning_version` | int | yes | New version | p0_public |
| `previous_tuning_version` | int | no | | p0_public |
| `config_version` | int64 | yes | | p0_public |
| `change_kind` | enum | yes | `update`, `rollout_step`, `overlay_apply`, `overlay_remove`, `rollback`, `auto_rollback` | p0_public |
| `diff` | object | yes | Scalar before/after or changed rows (≤ 64 KB; larger → `diff_ref`) | p0_public |
| `change_class` | enum | yes | | p0_public |
| `rollout_id` / `owner_ref` | string | no | | p1_pseudonymous |
| `actor` / `approvers` / `reason_code` | | yes | | p1_pseudonymous |

### 2.12 `store.offer.viewed` [key `account_id`]

Telemetry-grade; sampled at 100 % for detail views, not emitted for grid impressions.

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `offer_id` | string | yes | | p0_public |
| `surface` | enum | yes | `store_home`, `featured`, `category`, `search`, `season_track`, `deep_link` | p0_public |
| `price_crowns` | int | cond. | Displayed price | p0_public |
| `discount_ratio` | number | no | | p0_public |
| `already_owned` | bool | yes | | p1_pseudonymous |
| `wallet_crowns_bucket` | enum | yes | `0`, `1_499`, `500_1999`, `2000_plus` (bucketed, never exact) | p1_pseudonymous |

### 2.13 `store.purchase.completed` [key `account_id`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `purchase_id` | string (`pur_`) | yes | | p1_pseudonymous |
| `offer_id` | string | yes | | p0_public |
| `category` | enum | yes | | p0_public |
| `price_crowns` | int | cond. | Crowns offers | p1_pseudonymous |
| `price_minor` / `currency_code` | int64 / string | cond. | Crown packs | p2_personal |
| `crowns_credited` | int | cond. | Crown packs (base + bonus) | p1_pseudonymous |
| `is_gift` | bool | yes | | p1_pseudonymous |
| `recipient_account_id` | string | cond. | Gifts | p2_personal |
| `reward_grant_id` | string (`rwg_`) | cond. | Crowns offers | p1_pseudonymous |
| `txn_ids` | string[] | yes | Economy transactions | p1_pseudonymous |
| `first_purchase` | bool | yes | Account's first ever | p1_pseudonymous |

### 2.14 `store.purchase.refunded` [key `account_id`]

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `purchase_id` | string | yes | | p1_pseudonymous |
| `offer_id` | string | yes | | p0_public |
| `refund_kind` | enum | yes | `self_service`, `support`, `psp`, `chargeback` | p1_pseudonymous |
| `final_state` | enum | yes | `refunded`, `charged_back` | p1_pseudonymous |
| `crowns_reversed` | int | yes | | p1_pseudonymous |
| `crowns_shortfall` | int | yes | Crowns already spent and not recoverable | p1_pseudonymous |
| `entitlements_revoked` | string[] (`rwg_`) | yes | | p1_pseudonymous |
| `amount_minor` / `currency_code` | int64 / string | cond. | Real-money | p2_personal |
| `actor` | string | cond. | Operator for support refunds | p1_pseudonymous |

### 2.15 Example

```json
{
  "event_id": "evt_01JAB2C3D4E5F6G7H8J9K0M1N2",
  "event_name": "liveops.event.ended",
  "schema_version": 1,
  "occurred_at": "2026-11-02T15:00:00.000Z",
  "producer": { "service": "liveops-service", "instance_id": "liveops-service-4a1c", "build": "1.5.2+b10542" },
  "environment": "prod",
  "dc_region": "na",
  "correlation_id": "lop_01JA9X0Y1Z2A3B4C5D6E7F8G9H",
  "causation_id": "evt_01JAB2B9Z8Y7X6W5V4T3S2R1Q0",
  "content_version": 1851,
  "privacy_class": "p1_pseudonymous",
  "payload": {
    "event_id": "lop_01JA9X0Y1Z2A3B4C5D6E7F8G9H",
    "definition_id": "liveops_event/bonus_gold_weekend",
    "outcome": "guardrail_stopped",
    "guardrail": { "metric": "m_event_gold_created", "value": 912000000, "threshold": 900000000, "realm_id": "na-1" },
    "participants": 18234,
    "tier_grants": 9120,
    "event_gold_created_copper": 912000000,
    "duration_seconds": 151200
  }
}
```

Note the envelope `event_id` (`evt_`) and the payload `event_id` (`lop_`) are distinct; consumers deduplicate on the envelope field.

## 3. Analytics hooks

| Question | Events | Metrics (`m_<snake>`, Analytics-owned definitions) |
| --- | --- | --- |
| Do events work? | `liveops.event.*`, `quest.campaign.*`, `world.event.*`, participation via reward events | `m_event_participation_rate`, `m_event_coverage_ratio`, `m_event_dau_lift` (DAU during event vs 28-day baseline), `m_world_event_request_fulfillment_rate` |
| Are events safe economically? | `economy.currency.credited` with `correlation_id = lop_`, `liveops.event.ended` | `m_event_gold_created`, `m_gold_created_per_active_hour`, `m_marks_time_to_cap_hours`, `m_gold_inflation_index_wow` |
| Season health | `liveops.season.*`, season progress updates, `economy.currency.*` for `currency/season_tokens` | `m_season_track_completion_rate`, `m_season_tier_p50`, `m_season_tokens_expired_ratio` |
| Config safety | `config.flag.changed`, `config.tuning.published`, guardrail samples | `m_config_propagation_seconds`, `m_config_kill_propagation_seconds`, `m_config_rollback_seconds`, `m_config_auto_rollback_precision` |
| Release quality | `liveops.release.deployed`, `client.crash.reported`, `ops.service.health_changed` | `m_release_change_failure_rate`, `m_release_maintenance_ratio`, `m_maintenance_overrun_rate`, `m_predownload_ratio`, `m_realm_unplanned_downtime_minutes` |
| Store health | `store.*`, `economy.currency.*` (crowns) | `m_store_payer_conversion`, `m_store_refund_rate`, `m_store_chargeback_rate`, `m_store_purchase_failure_rate`, `m_crowns_stranded_p50` |
| Player sentiment | `client.survey.submitted`, `liveops.survey.published` | `m_survey_nps`, `m_survey_response_rate` |
| Retention | `auth.session.*`, `character.world.entered` | `m_d1_retention`, `m_d7_retention`, `m_d30_retention`, `m_dau`, `m_mau`, `m_dau_mau_ratio`, `m_churn_rate_30d`, `m_reactivation_rate_30d` |
| Compensation | `liveops.compensation.granted`, `reward.grant.*` with `causation_id = cmpg_` | `m_compensation_delivery_latency_p99`, `m_compensation_gold_share` |

Attribution rules: services crediting rewards caused by a LiveOps event set `correlation_id = lop_…` (passed via the reward grant request); multiplier-attributed gold is estimated as `base × (M − 1)` per credit when the overlay was active, using the `config_version` stamped on the credit. Guardrail metrics required by rollouts must be available at ≤ 2-minute latency in the serving layer; season and KPI metrics are daily.

## 4. Security considerations

### 4.1 Threats and controls

| Threat | Example | Controls |
| --- | --- | --- |
| GM abuse (insider) | GM grants items or gold to own/friends' characters; GM refunds to accomplice | No arbitrary item creation (catalog bundles only); `gm_grant` two-person + QA-only; self/related-account block via relation graph; per-GM daily limits; every action in `audit-service` (WORM); weekly anomaly review of GM grant patterns; JIT elevation |
| Compromised operator account | Attacker enables a 10× multiplier | Hardware MFA; caps enforced in code (2.0 XP max, overlay `max` in `config-service`); tier approvals; anomalous-login alerts on admin SSO |
| Flag misconfiguration | Wrong cohort enables unfinished feature globally; tuning typo (gold scale 1000 instead of 100) | `value_schema` ranges; > 25 % relative change → tier 3; rollouts with guardrails; automatic rollback; config freeze after releases; diff preview in admin-portal |
| Overlay leak | liveops-service crash leaves multiplier on | Overlay hard `expires_at`; reconciliation every minute comparing active events vs overlays |
| Event exploit | Players farm participation points with bots; stacking multipliers | Points use server-authoritative domain events only; daily caps per earn rule; non-stacking resolution; combined XP ceiling; anticheat signals on outliers (> 4σ point velocity) |
| Trigger manipulation | Players deliberately depress a metric to trigger catch-up | Triggers use realm aggregates with hysteresis (`for 2d`), cooldowns, per-season activation limits; dynamic envelopes small |
| Compensation farming | Creating characters during outage to collect grants | Recipient set frozen; characters created after incident start excluded; per-account cap |
| Store fraud | Stolen cards buying crowns; ATO spending crowns; gifting laundering | PSP 3-DS, velocity caps, new-account caps, risk gating, gift limits, chargeback clawback, store holds |
| Payment webhook spoofing | Forged "payment succeeded" | PSP signature verification, replay window, amount/currency re-check, dedup by PSP event id |
| Pay-to-win regression | New offer smuggles stat item | Three-layer validation (catalog, store, reward-service); validation hash pinned per offer per content version |
| Release tampering | Unsigned content pack or manifest | Packs signed by catalog; manifests signed (Client delivery spec); `release_manager` approval; builds from CI identity only |
| Data exposure in admin portal | Bulk export of player emails | p2/p3 fields require JIT + reason; no bulk export of p2/p3; field-level audit |

### 4.2 Server validation rules (must be enforced in services, not UI)

1. `config-service` rejects any value outside `value_schema`, any overlay on a non-`liveops_overridable` flag, any overlay value above the key's cap.
2. `liveops-service` recomputes caps and faucet estimates at approval **and** at activation.
3. Consumers (progression, loot, economy, reward) clamp event multipliers to the hard caps defined in Events and Seasons even if config says otherwise (defense in depth).
4. `store-service` re-validates offer eligibility, price, limits, sanctions, and the validation hash at `CreatePurchase`; `reward-service` rejects non-cosmetic contents for `store_purchase`.
5. Compensation recipients are computed server-side from authoritative data; uploaded lists are intersected with existing characters and capped.
6. Every admin RPC checks role, JIT elevation, two-person constraints, and self/related-account exclusion server-side.

### 4.3 Reporting to `anticheat-service`

| Signal (`ReportSignal.kind`) | Condition |
| --- | --- |
| `store_velocity` | > 5 crown pack purchases / 24 h, or new-account cap hit |
| `store_chargeback` | Any chargeback; includes linked accounts via gifts |
| `store_gift_graph` | ≥ 5 distinct senders to one recipient in 7 days, or gifts to accounts with risk ≥ 600 |
| `event_point_outlier` | Participation point velocity > 4σ above realm mean for 2 hours |
| `compensation_claim_anomaly` | Account linked to many recipients via shared devices |
| `gm_action_anomaly` | GM grant/refund patterns (also sent to `enforcement_lead`) |

`anticheat-service` folds signals into the risk score (Security spec, Risk score); enforcement decisions are `enforcement-service`'s.

## 5. Agent implementation guide

### 5.1 Repository / module layout

```text
/BackendServices
  /liveops-service
    /api            proto: lsf/liveops_service/v1/*.proto
    /domain         events/, seasons/, releases/, community/, compensation/, calendar/
    /trl            lexer, parser, type checker, evaluator (pure, no I/O)
    /economy        faucet estimator, caps (pure)
    /integrations   reward, economy, quest, director, instance, catalog, world, gateway clients
    /workers        scheduler (state transitions), trigger evaluator, participation consumer,
                    season end job, compensation issuer, overlay reconciler, status deriver
    /migrations     schema liveops
    /tests          unit/, property/, integration/, load/
  /config-service
    /api            lsf/config_service/v1
    /domain         keys, rules, resolution, overlays, kills, rollouts, guardrails
    /push           WatchConfig fan-out
    /migrations     schema config
  /store-service
    /api            lsf/store_service/v1
    /domain         catalog, pricing guardrails, purchase saga, refunds, fraud rules
    /psp            adapter interface + one provider adapter + fake for tests
    /migrations     schema store
  /libs/config-sdk  snapshot cache, local evaluation, disk fallback, ack, metrics
/Tools/admin-portal  GM section (web UI calling admin RPCs only)
/Content/validators/liveops   liveops_event, season, store_offer validators (run in content-catalog-service)
```

Language and frameworks follow the Backend spec; this layout is logical.

### 5.2 Build order and milestones

| Milestone | Scope | Exit criteria |
| --- | --- | --- |
| M1 Config core | `config` schema, key registration, rules, resolution, `GetSnapshot`, `WatchConfig`, config-sdk with disk fallback, kills | Resolution property tests pass; kill propagation p99 ≤ 5 s in load test with 500 SDK clients |
| M2 Rollouts | Rollout plans, guardrail evaluator, auto-rollback, freeze | Worked example (Config and Releases, Guardrail evaluation) reproduced in tests; rollback ≤ 90 s |
| M3 Events | `liveops` events schema, scheduler, overlays, caps, faucet estimator, TRL, participation, `liveops.event.*` | Bonus weekend end-to-end in integration env; guardrail stop works |
| M4 Seasons | Seasons, region windows, SP ledger, track grants, rotations push, token conversion/expiry | Season end job idempotent under replay; conversion worked example reproduced |
| M5 Campaign & director interplay | ScheduleCampaign, RequestWorldEvent, cancellation | Contract tests with quest/director fakes |
| M6 Releases | Release records, stage gates, maintenance flow, canary bake, content activation, launcher coordination | Staging rehearsal of maintenance sequence with fakes |
| M7 Community | Announcements, MOTD, surveys, status page | Locale gating; frequency cap tests |
| M8 Compensation | Targeting, freeze, issuance worker, retries, approvals | 1 M recipient load test at 2,000 grants/s; replay produces zero duplicates |
| M9 Store | Catalog, pricing guardrails, crowns saga, PSP adapter, refunds, chargebacks, fraud, reconciliation | No-pay-to-win validator suite; saga chaos tests |
| M10 GM tools | admin-portal GM section, RBAC, JIT, two-person | Permission matrix tests per role |

### 5.3 Invariants checklist

1. Exactly one `active` season per environment; region windows aligned to weekly reset.
2. No overlay exists without an `active` owning event (except during ≤ 5 min expiry grace).
3. Every overlay value ≤ declared cap; every multiplier key resolves to ≤ hard cap in consumers.
4. `cancelled` events never had `active` state.
5. Season token wallet balance after end job = 0 for every character of that season's region.
6. Every crowns debit has exactly one matching grant or refund credit (`causation_id = pur_`).
7. No `store_purchase` grant contains stat-budget items, XP, LP, skill gains, gold, marks, or season tokens.
8. Every compensation recipient receives at most one grant per compensation (idempotency key).
9. Every admin mutation has an audit record with actor, reason, and approvers satisfying the tier.
10. `config_version` strictly increases; tuning versions immutable; rollback creates a new version.
11. Kill switch evaluation always wins over overlays and rules.
12. Percentage bucketing is monotone: raising `percentage_bp` never removes an account from the cohort.

### 5.4 Test matrix

| Type | Area | Concrete cases |
| --- | --- | --- |
| Unit | TRL | Parse/typecheck each example rule; `for 2d` hysteresis; stale metric → false; node limit exceeded → compile error; `delta_pct` with zero baseline → 0 |
| Unit | Faucet | Bonus gold weekend worked example → `E_gold ≈ 1.705 × 10⁹`, compliant `M_gold = 1.05` |
| Unit | Season | `token_value(100) = 2,030`, `token_value(30) = 345`; catch-up applies at week 10 tier 72 and not at tier 76; SP weekly cap |
| Unit | Compensation tiers | `tier(29)=0`, `tier(30)=1`, `tier(145)=2`, `tier(1440)=4`; `gold_reward(60,2)=95,285` |
| Unit | Guardrail | z-test worked example → warning at z = 2.95, breach at 3.4 |
| Unit | Pricing | 50 % floor; bonus ratio 0.20; reference-price 28/90 rule |
| Property | Config resolution | For random rule sets and contexts: kill → safe_value; result independent of rule insertion order given priorities; bucketing monotone; same account+key → same bucket |
| Property | Overlay resolution | Max wins for multiplier keys regardless of event order; removing an overlay restores prior value exactly |
| Property | Event state machine | Random sequences of commands never reach `active` from `cancelled`, never skip approval |
| Property | Purchase saga | Random failure injection between steps: final state satisfies invariant 6 |
| Integration | Bonus weekend | Create → approve → start → overlays visible in SDK fake → guardrail sample → auto end; events emitted in order |
| Integration | Season end | Two regions end at different resets; job crash mid-way and restart; balances zero; one `liveops.season.ended` per region |
| Integration | Campaign | ScheduleCampaign idempotent replay returns same `cmp_`; `end_with_campaign` ends event |
| Integration | Director decline | Declined request does not fail the event; retried ≤ 1/day |
| Integration | Maintenance | Login gate → drain → deploy fake → activation → canary bake → open; overrun extension and status page update |
| Integration | Store | Crown pack via PSP fake webhook (valid, duplicate, bad signature, amount mismatch); crowns offer with grant failure → re-credit |
| Integration | Refund/chargeback | Self-service within/after 14 days; chargeback with crowns spent → shortfall and revocations |
| Integration | Compensation | 10,000 recipients, kill switch mid-run, resume, replay entire run → zero duplicates |
| Load | config push | 5,000 SDK streams, 50 changes/s, kill p99 ≤ 5 s |
| Load | Participation consumer | 20,000 events/s sustained, lag ≤ 30 s |
| Load | Compensation | 1,000,000 recipients at 2,000 grants/s |
| Load | Store | 500 purchases/s peak (launch sale) with PSP fake latency 300 ms |
| Security | Admin | Every RPC × every role matrix; self-grant blocked; approver = requester blocked |

### 5.5 Acceptance criteria

- All `liveops.*`, `config.*`, `store.*` events validate against registered schemas and carry the Registry envelope.
- Hard caps cannot be exceeded through any API, including direct `config-service` overlay calls.
- Season end, compensation issuance, and purchase saga are idempotent under full replay.
- Kill switch p99 ≤ 5 s and auto-rollback ≤ 90 s demonstrated in load tests.
- The no-pay-to-win validator rejects a fixture offer containing each forbidden content type.
- Every admin action produces an audit record; audit gaps fail CI integration tests.

### 5.6 Pitfalls

- Using the envelope `event_id` and LiveOps `event_id` (`lop_`) interchangeably — name the payload field explicitly and dedup on the envelope.
- Applying tuning mid-tick on zone servers; always swap at tick boundaries.
- Evaluating percentage rollouts without an `account_id` (server-global keys) — they must use realm cohorts.
- Treating dc_region season windows as one global instant.
- Forgetting that weekly marks cap is absolute; marks multipliers must not touch the cap key.
- Letting `admin-portal` hold business logic; all checks live server-side.
- Granting rewards directly via inventory/economy instead of `reward-service`.
- Reading season token balances from a cache during the end job (use the economy-service authoritative wallet).
- Using floating point for money or crowns; all integer minor units.

### 5.7 Do / Don't for coding agents

| Do | Don't |
| --- | --- |
| Use Registry names, enums, prefixes, and event names exactly | Invent new event names or enum values; propose them in Registry additions instead |
| Keep TRL, faucet estimator, caps, and resolution as pure functions with exhaustive tests | Put I/O or clock reads inside pure modules (inject `now`) |
| Pass `idempotency_key` on every mutating call and persist results ≥ 7 days | Retry mutating calls with a new key |
| Stamp `correlation_id = lop_/cmpg_/pur_` on downstream requests | Emit per-recipient `liveops.compensation.granted` events |
| Clamp multipliers in consumers as well as producers | Trust admin-portal validation alone |
| Write audit records in the same unit of work as the mutation (outbox) | Write audit asynchronously without guarantees |
| Use an outbox table for domain events | Publish events before the DB transaction commits |
| Reference other specs by short name and section title | Copy formulas from other specs with changed constants |
