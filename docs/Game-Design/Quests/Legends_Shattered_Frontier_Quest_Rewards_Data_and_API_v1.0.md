# Legends of the Shattered Frontier — Quest Rewards, Data and API

Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per Shared Registry v1.0.

Back-links: [Quest entry](../Legends_Shattered_Frontier_Quest_Architecture_v1.0.md) · [Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

---

## 1. Reward formulas

All XP, gold, marks, and LP amounts come from the registry scaling functions (`xp_reward`, `gold_reward`, `marks_reward`, `lp_reward`) evaluated by `reward-service`. `quest-service` decides only the **level L** and the **weights**. Item rewards follow the registry power ladder and the Itemization spec stat-budget rules.

### 1.1 Effective weight

```
w_eff = clamp( w_base × w_type × d_mult × g_mult × w_scarcity × c_factor , 0 , w_ceiling )

w_component(k) = w_eff × r_mult(repeat_policy, k) × over_level(k)      for k ∈ {xp, gold, marks, lp}
```

| Symbol | Meaning | Default | Tuning key |
| --- | --- | --- | --- |
| `w_base` | Authored weight in the definition (`reward.weight`); a standard quest = 1.0 | 0.1–5.0 | definition |
| `w_type` | Quest type weight | story 1.25, dynamic 0.8, exploration 0.9, crafting 0.9, faction 1.0, dungeon 2.0, raid 3.0 | `tune.quest.type_weight_<type>` |
| `d_mult` | Difficulty | story 0.75, normal 1.0, veteran 1.3, heroic 1.6, mythic 2.0 | `tune.quest.difficulty_mult_<value>` |
| `g_mult` | Group bonus `1 + 0.05 × (n_credited − 1)`, ≤ 1.20 | 1.0 | `tune.quest.group_bonus_per_member` |
| `w_scarcity` | Generated quests only: `1 + 0.25 × clamp(scarcity − 1, 0, 1)` | 1.0 | `tune.quest.scarcity_bonus` |
| `c_factor` | Consolation `p × 0.75` on source expiry; heroic under-manned `0.5`; else 1.0 | 1.0 | `tune.quest.consolation_factor` |
| `w_ceiling` | 1.5 for generated quests; 4.99 for authored quests without `economy_review_id`; 10.0 with review | — | `tune.quest.generated_max_weight`, `tune.quest.authored_max_weight` |
| `r_mult` | Repeat multiplier per component (§1.2) | — | `tune.quest.repeat_mult_<policy>_<component>` |
| `over_level` | XP only: `1` if `Lc ≤ max + 5`, else `max(0.1, 1 − 0.15 × (Lc − max − 5))` | — | `tune.quest.over_level_xp_slope` (0.15) |

`L` passed to the functions = `clamp(Lc, level_range.min, level_range.max)`, where `Lc` is the character level at the moment of the turn-in or completion transaction. This follows the registry rule "L clamped to the content's level range".

Rounding: the registry functions use `round`, defined here as half away from zero. Any weight with more than 4 decimals is truncated to 4 before it is passed.

Constraint: the validator rejects definitions whose `w_base × w_type × d_mult` ≥ 5.0 without `economy_review_id`. This is the registry rule that weights at or above 5.0 require LiveOps economy review.

### 1.2 Repeat multipliers

| `repeat.policy` | xp | gold | marks | lp | Notes |
| --- | --- | --- | --- | --- | --- |
| `once` | 1.0 | 1.0 | 1.0 | 1.0 | LP only on `once` quests |
| `daily` | 0.5 | 0.6 | 0.35 | 0 | Resets at the daily reset (registry §7.3); marks factor per §2.3 |
| `weekly` | 1.0 | 1.0 | 1.0 | 0 | Weekly reset; usually authored with `w_base` 2–4 |
| `repeatable` | 0.25 | 0.25 | 0 | 0 | Unlimited; cooldown ≥ 300 s |
| `per_source` (generated) | 1.0 | 1.0 | 1.0 | 0 | After `dynamic_daily_full_reward` (15) completions per day: 0.25 for all components |

Marks are granted only if `Lc ≥ 60` (registry). `economy-service` enforces `FRONTIER_MARKS_WEEKLY_CAP`. If the cap truncates a marks grant, `reward-service` reports it and quest-service records `marks_capped = true` for analytics.

### 1.3 Daily and weekly limits

| Limit | Default | Tuning key |
| --- | --- | --- |
| Daily quests completed per character per day (all daily quests combined) | 12 | `tune.quest.daily_quest_limit` |
| Faction daily quests per faction per day | 4 | `tune.quest.faction_daily_limit` |
| Dynamic quests at full reward per day | 15 | `tune.quest.dynamic_daily_full_reward` |
| Dynamic quests per day (hard; beyond this no offers) | 40 | `tune.quest.dynamic_daily_hard_limit` |
| Weekly quests per character per week | 8 | `tune.quest.weekly_quest_limit` |
| Active quests in log | 25 | `tune.quest.max_active` |
| Choose-option reputation delta | ±500 | `tune.quest.max_choice_rep_delta` |
| Control influence per region per week | 20 | `tune.quest.max_control_influence_weekly` |

Counters live in `quest.repeat_counters` keyed by `(character_id, counter_key, period_start)`. The period start is computed from the character's realm datacenter reset schedule.

### 1.4 Item rewards

- Item level: `4 × L` (levelling content). Dungeon and raid quest items use the ladder entry for their content (e.g. dungeon `veteran` quest reward ilvl 415).
- Rarity ceiling: `rare` for open-world quests; `epic` only for `dungeon`/`raid` quests whose content row allows it.
- Choice bundles (`reward.options`) let players pick a role-appropriate item. All options must resolve to the same ilvl and rarity.
- Recipes, cosmetics, and titles are allowed in bundles. Account-wide unlocks go through progression-service (Character spec).
- `currency/crowns` must never appear in any quest bundle. The validator blocks it.

### 1.5 Worked examples

Assume illustrative Character spec values `xp_to_next(22) = 9,000` and `xp_to_next(65) = 60,000`.

**A. Story quest, L 22, normal, solo, once, `w_base` 1.0**

- `w_eff = 1.0 × 1.25 × 1.0 × 1.0 = 1.25`.
- XP = `round(1.25 × 9,000 / 12)` = 938.
- Gold = `round(1.25 × 100 × 23^1.5)` = `round(1.25 × 11,030.4)` = 13,788 copper (≈ 1 g 37 s 88 c).
- LP = `round(1.25 × 2)` = round(2.5) = 3 (half away from zero).

**B. Faction daily, Lc 65 (range 60–70), veteran, 3-player group, `w_base` 1.0**

- `g_mult = 1.10`, so `w_eff = 1.0 × 1.0 × 1.3 × 1.10 = 1.43`.
- `w_xp = 1.43 × 0.5 = 0.715`. XP = `round(0.715 × 60,000 / 12)` = 3,575.
- `w_gold = 1.43 × 0.6 = 0.858`. `66^1.5 = 536.19`, so gold = `round(0.858 × 53,619)` = 46,005 copper (≈ 4 g 60 s).
- `w_marks = 1.43 × 0.35 = 0.5005`. Marks = `round(0.5005 × 50)` = round(25.03) = 25.
- LP = 0 (daily).

**C. Over-level XP**

A character at Lc 40 turns in a quest with range 20–28. `L = 28`. `over_level = max(0.1, 1 − 0.15 × (40 − 28 − 5)) = max(0.1, −0.05) = 0.1`. Gold uses `L = 28` with no over-level reduction.

## 2. Economic balancing targets

The Character spec owns total XP to cap; the Economy spec owns total gold flow. The Quest spec owns the **quest share** of each.

### 2.1 XP share

| Progression band | Quest share of levelling XP | Remainder (indicative) |
| --- | --- | --- |
| 1 (L 1–10) | 70 % | kills 20 %, gathering/crafting/exploration 10 % |
| 2–5 (L 11–50) | 60 % | kills, dungeons, world events |
| 6–9 (L 51–90) | 50 % | dungeons grow |
| 10 (L 91–100) | 45 % | dungeons, raids prep, world events |
| Overall 1–100 | 55 % ± 5 | — |

Implied number of standard quests per level: `n_q(band) = share × 12 / avg(w_xp)`. With an average `w_xp ≈ 1.1` (mix of story 1.25, dynamic 0.8, dailies 0.5–0.65):

| Band | n_q per level | Median quest time target | Quest minutes per level |
| --- | --- | --- | --- |
| 1 | 7.6 | 5 min | 38 |
| 2–5 | 6.5 | 8 min | 52 |
| 6–9 | 5.5 | 10 min | 55 |
| 10 | 4.9 | 12 min | 59 |

Time-per-quest targets by type (median, solo at level):

| Type | Median minutes | P90 minutes |
| --- | --- | --- |
| story | 8 | 15 |
| dynamic | 6 | 12 |
| exploration | 10 | 20 |
| crafting (intro) | 12 | 25 |
| faction | 8 | 15 |
| dungeon (run-gated) | run time + 5 | — |
| raid | lockout-gated | — |

### 2.2 Gold faucet share

| Population | Quest share of gold created (all `quest_reward` source) | Dynamic share within quests | Campaign share |
| --- | --- | --- | --- |
| Levelling (Lc < 100) | 30 % (25–35) | ≤ 40 % | ≤ 10 % in campaign weeks |
| At cap | 15 % (10–20) | ≤ 50 % | ≤ 10 % |

Measured as `m_quest_gold_faucet_share = Σ economy.currency.credited(source=quest_reward, currency/gold) / Σ economy.currency.credited(currency/gold, all sources)` over 7 days, segmented by level band.

**Feedback rule.** At each season boundary, if the measured share is outside its range for two consecutive weeks, Quest adjusts `tune.quest.type_weight_dynamic` and `tune.quest.repeat_mult_daily_gold` by at most ±10 % per season. Quest never changes the registry scale constants; those belong to Economy.

### 2.3 Marks share

At cap, quests should supply ≤ 35 % of `FRONTIER_MARKS_WEEKLY_CAP` (≤ 525 of 1,500) for a player completing the faction-daily limit plus weeklies. Without a daily marks factor, 4 faction dailies × 5 days × 72 marks ≈ 1,440, far over target. The daily marks multiplier `tune.quest.repeat_mult_daily_marks` (0.35) gives 4 × 5 × 25 = 500 ≤ 525 ✓. The remaining headroom is small, so weekly quests at cap should take marks from a reduced marks component (authored `marks_weight_override ≤ 0.5`). Most weekly marks should come from Dungeon sources (Dungeon spec).

### 2.4 Completion-rate targets

| Metric | Target | Alert |
| --- | --- | --- |
| Story acceptance → completion | ≥ 80 % | < 65 % for a quest over 7 days |
| Dynamic acceptance → completion | ≥ 60 % | < 40 % per template |
| Abandon rate (any) | ≤ 10 % | > 20 % per quest |
| Objective drop-off (largest single step loss in a chain) | ≤ 15 % | > 25 % |
| Choice split in branches | each option ≥ 20 % | < 10 % (narrative review) |

## 3. Persistent schema (PostgreSQL `quest`)

```sql
CREATE SCHEMA IF NOT EXISTS quest;

-- Quest instances (qst_)
CREATE TABLE quest.quest_instances (
  quest_instance_id      text PRIMARY KEY CHECK (quest_instance_id LIKE 'qst\_%'),
  realm_id               text NOT NULL,
  character_id           text NOT NULL,
  quest_id               text NOT NULL,              -- quest/<slug> or template-derived id
  generated_quest_id     text NULL,                  -- FK added after quest.generated_quests
  definition_revision    int  NOT NULL,
  content_version        bigint NOT NULL,
  quest_type             text NOT NULL CHECK (quest_type IN ('story','dynamic','exploration','crafting','faction','raid','dungeon')),
  difficulty             text NOT NULL CHECK (difficulty IN ('story','normal','veteran','heroic','mythic')),
  state                  text NOT NULL CHECK (state IN ('offered','active','ready_to_turn_in','completed','failed','abandoned','expired')),
  state_reason           text NULL,
  shared_by_character_id text NULL,
  offered_at             timestamptz NOT NULL,
  offer_expires_at       timestamptz NULL,
  accepted_at            timestamptz NULL,
  deadline_at            timestamptz NULL,
  ready_at               timestamptz NULL,
  terminal_at            timestamptz NULL,
  turn_in_pending        boolean NOT NULL DEFAULT false,
  reward_grant_id        text NULL CHECK (reward_grant_id IS NULL OR reward_grant_id LIKE 'rwg\_%'),
  w_eff                  numeric(8,4) NULL,
  reward_level           smallint NULL,
  version                int NOT NULL DEFAULT 0      -- optimistic concurrency
);
-- one non-terminal instance per character per quest (or per generated quest)
CREATE UNIQUE INDEX quest_instances_one_open
  ON quest.quest_instances (character_id, quest_id, COALESCE(generated_quest_id, ''))
  WHERE state IN ('offered','active','ready_to_turn_in');
CREATE INDEX quest_instances_char_state ON quest.quest_instances (character_id, state);
CREATE INDEX quest_instances_deadline ON quest.quest_instances (deadline_at) WHERE state = 'active' AND deadline_at IS NOT NULL;

CREATE TABLE quest.objective_progress (
  quest_instance_id text NOT NULL REFERENCES quest.quest_instances ON DELETE CASCADE,
  objective_key     text NOT NULL,
  objective_type    text NOT NULL,
  order_group       smallint NOT NULL,
  required          int NOT NULL CHECK (required BETWEEN 1 AND 500),
  current           int NOT NULL DEFAULT 0 CHECK (current BETWEEN 0 AND required),
  optional          boolean NOT NULL,
  completed_at      timestamptz NULL,
  last_event_id     text NULL,
  PRIMARY KEY (quest_instance_id, objective_key)
);

-- Choices: append-only
CREATE TABLE quest.choices (
  character_id      text NOT NULL,
  quest_id          text NOT NULL,
  choice_key        text NOT NULL,
  option            text NOT NULL,
  quest_instance_id text NOT NULL,
  made_at           timestamptz NOT NULL,
  superseded_by_aud text NULL,                         -- aud_ id when GM reverted
  PRIMARY KEY (character_id, quest_id, choice_key)
);

CREATE TABLE quest.character_flags (
  character_id text NOT NULL,
  flag         text NOT NULL,
  value        bigint NOT NULL,
  set_by_qst   text NULL,
  updated_at   timestamptz NOT NULL,
  PRIMARY KEY (character_id, flag)
);

CREATE TABLE quest.exclusive_locks (
  character_id text NOT NULL,
  group_key    text NOT NULL,
  chosen_ref   text NOT NULL,
  locked_at    timestamptz NOT NULL,
  PRIMARY KEY (character_id, group_key)
);

CREATE TABLE quest.repeat_counters (
  character_id  text NOT NULL,
  counter_key   text NOT NULL,     -- e.g. 'quest/<slug>', 'daily_all', 'faction_daily:faction/<slug>', 'dynamic_full'
  period_start  timestamptz NOT NULL,
  count         int NOT NULL DEFAULT 0,
  PRIMARY KEY (character_id, counter_key, period_start)
);

CREATE TABLE quest.completions (     -- lifetime "has completed" lookup for prerequisites
  character_id  text NOT NULL,
  quest_id      text NOT NULL,
  first_completed_at timestamptz NOT NULL,
  times         int NOT NULL DEFAULT 1,
  PRIMARY KEY (character_id, quest_id)
);

-- Turn-in saga log
CREATE TABLE quest.turn_in_steps (
  quest_instance_id text NOT NULL REFERENCES quest.quest_instances,
  step              text NOT NULL CHECK (step IN ('consume','grant','reputation','influence','flags','chain')),
  status            text NOT NULL CHECK (status IN ('pending','done','failed')),
  attempts          int NOT NULL DEFAULT 0,
  last_error        text NULL,
  updated_at        timestamptz NOT NULL,
  PRIMARY KEY (quest_instance_id, step)
);

-- Generated quests (gqd_)
CREATE TABLE quest.generated_quests (
  generated_quest_id text PRIMARY KEY CHECK (generated_quest_id LIKE 'gqd\_%'),
  realm_id           text NOT NULL,
  template_id        text NOT NULL,
  template_revision  int  NOT NULL,
  content_version    bigint NOT NULL,
  request_id         text NOT NULL,
  reason             text NOT NULL,
  source_ref         text NULL,
  world_region_id    text NOT NULL,
  definition         jsonb NOT NULL,          -- fully bound quest definition (immutable)
  signature          bytea NOT NULL,
  w_base_bound       numeric(8,4) NOT NULL,
  state              text NOT NULL CHECK (state IN ('published','withdrawn','expired')),
  published_at       timestamptz NOT NULL,
  expires_at         timestamptz NOT NULL,
  text_id            bigint NULL
);
CREATE UNIQUE INDEX generated_quests_sig ON quest.generated_quests (signature) WHERE state = 'published';
CREATE INDEX generated_quests_region ON quest.generated_quests (realm_id, world_region_id, state);
CREATE INDEX generated_quests_source ON quest.generated_quests (source_ref) WHERE state = 'published';
ALTER TABLE quest.quest_instances ADD CONSTRAINT quest_instances_gqd_fk
  FOREIGN KEY (generated_quest_id) REFERENCES quest.generated_quests (generated_quest_id);

CREATE TABLE quest.generation_requests (
  request_id     text PRIMARY KEY,
  realm_id       text NOT NULL,
  requester      text NOT NULL,                 -- service identity
  received_at    timestamptz NOT NULL,
  result         jsonb NOT NULL                 -- replay response
);

CREATE TABLE quest.generated_texts (
  text_id        bigserial PRIMARY KEY,
  cache_key      bytea NOT NULL UNIQUE,
  locale         text NOT NULL,
  origin         text NOT NULL CHECK (origin IN ('precomputed','llm')),
  fields         jsonb NOT NULL,
  model_ref      text NULL,
  prompt_version text NULL,
  classifier_version text NULL,
  review_state   text NOT NULL CHECK (review_state IN ('auto_approved','queued','approved','rejected','hidden')),
  report_count   int NOT NULL DEFAULT 0,
  created_at     timestamptz NOT NULL
);

-- Campaigns (cmp_)
CREATE TABLE quest.campaigns (
  campaign_id      text PRIMARY KEY CHECK (campaign_id LIKE 'cmp\_%'),
  campaign_def_id  text NOT NULL,             -- campaign/<slug>
  definition_revision int NOT NULL,
  realm_id         text NOT NULL,
  liveops_event_id text NULL CHECK (liveops_event_id IS NULL OR liveops_event_id LIKE 'lop\_%'),
  state            text NOT NULL CHECK (state IN ('scheduled','active','stage_complete','completed','failed','archived')),
  current_stage    smallint NOT NULL DEFAULT 0,
  starts_at        timestamptz NOT NULL,
  claim_until      timestamptz NULL,
  version          int NOT NULL DEFAULT 0
);

CREATE TABLE quest.campaign_stages (
  campaign_id     text NOT NULL REFERENCES quest.campaigns,
  stage           smallint NOT NULL,
  population_p    int NULL,
  fair_share_cp   int NULL,
  threshold_cp    bigint NULL,
  progress_cp_x2  bigint NOT NULL DEFAULT 0,   -- half-point precision
  opened_at       timestamptz NULL,
  deadline_at     timestamptz NULL,
  closed_at       timestamptz NULL,
  outcome         text NULL CHECK (outcome IN ('reached','failed')),
  PRIMARY KEY (campaign_id, stage)
);

CREATE TABLE quest.campaign_contributions (
  campaign_id   text NOT NULL,
  stage         smallint NOT NULL,
  character_id  text NOT NULL,
  raw_cp        bigint NOT NULL DEFAULT 0,
  cp_x2         bigint NOT NULL DEFAULT 0,      -- accepted CP × 2
  overflow_cp   bigint NOT NULL DEFAULT 0,
  tier          text NULL,
  reward_grant_id text NULL,
  updated_at    timestamptz NOT NULL,
  PRIMARY KEY (campaign_id, stage, character_id),
  FOREIGN KEY (campaign_id, stage) REFERENCES quest.campaign_stages
);

CREATE TABLE quest.contribution_log (          -- per action, idempotent
  idempotency_key text PRIMARY KEY,
  campaign_id     text NOT NULL,
  stage           smallint NOT NULL,
  character_id    text NOT NULL,
  action          text NOT NULL,
  raw_cp          int NOT NULL,
  accepted_cp_x2  int NOT NULL,
  occurred_at     timestamptz NOT NULL
);

-- Infrastructure
CREATE TABLE quest.inbox (
  event_id     text NOT NULL,
  character_id text NOT NULL DEFAULT '',
  processed_at timestamptz NOT NULL,
  PRIMARY KEY (event_id, character_id)
);   -- pruned after 14 days (domain topic retention)

CREATE TABLE quest.signal_inbox (
  signal_id    text PRIMARY KEY,
  producer     text NOT NULL,
  processed_at timestamptz NOT NULL
);

CREATE TABLE quest.outbox (
  outbox_id    bigserial PRIMARY KEY,
  event_id     text NOT NULL UNIQUE,
  event_name   text NOT NULL,
  partition_key text NOT NULL,
  envelope     jsonb NOT NULL,
  created_at   timestamptz NOT NULL,
  published_at timestamptz NULL
);

CREATE TABLE quest.idempotency_keys (
  idempotency_key text PRIMARY KEY,
  rpc            text NOT NULL,
  response       bytea NOT NULL,
  created_at     timestamptz NOT NULL      -- retained ≥ 7 days (IDEMPOTENCY_RETENTION)
);
```

Partitioning: `quest_instances` is hash-partitioned by `character_id` (16 partitions at launch). Terminal instances older than 180 days move to `quest_instances_archive` (same shape) by a nightly job. `completions` and `choices` keep lifetime state, so prerequisites never need the archive.

Redis keys: `qidx:{realm}:{chr}` (objective index), `cmp:{cmp}:{stage}:progress` (display counter), `cmp:{cmp}:{stage}:rate:{chr}` (rolling-hour CP, TTL 3600), `giver:{realm}:{chr}:{npc}` (dialog state cache, TTL 10 s).

## 4. gRPC API (`lsf.quest_service.v1`)

All mutating RPCs carry `idempotency_key`. Player-originated calls arrive through `gateway` (quest log UI) or `zone-server` (in-world interactions) with the authenticated `character_id` taken from the session, never from the request body.

```protobuf
syntax = "proto3";
package lsf.quest_service.v1;

service QuestService {
  // Player-facing (via gateway / zone-server)
  rpc ListQuestLog(ListQuestLogRequest) returns (ListQuestLogResponse);
  rpc GetQuestInstance(GetQuestInstanceRequest) returns (QuestInstance);
  rpc ListOffers(ListOffersRequest) returns (ListOffersResponse);            // per giver / board / auto offers
  rpc AcceptQuest(AcceptQuestRequest) returns (QuestInstance);
  rpc DeclineQuest(DeclineQuestRequest) returns (QuestInstance);
  rpc AbandonQuest(AbandonQuestRequest) returns (QuestInstance);
  rpc TurnInQuest(TurnInQuestRequest) returns (TurnInResult);               // zone-server only (needs proximity)
  rpc MakeChoice(MakeChoiceRequest) returns (MakeChoiceResult);             // zone-server only
  rpc ShareQuest(ShareQuestRequest) returns (ShareQuestResult);
  rpc TrackQuest(TrackQuestRequest) returns (Empty);                        // UI pin, non-authoritative

  // Server-to-server
  rpc ReportSignals(ReportSignalsRequest) returns (ReportSignalsResult);    // zone-server, instance-server
  rpc GetGiverStates(GetGiverStatesRequest) returns (GetGiverStatesResponse);
  rpc ProposeGeneratedQuests(GenerationRequest) returns (GenerationResult); // ai-director-service only; proxies faction pressure
  rpc InvalidateSource(InvalidateSourceRequest) returns (InvalidateSourceResult);

  // Campaigns
  rpc GetCampaign(GetCampaignRequest) returns (Campaign);
  rpc ContributeToCampaign(ContributeRequest) returns (ContributionResult); // zone-server
  rpc GetMyContribution(GetMyContributionRequest) returns (ContributionSummary);

  // Admin (admin-portal; every call audited via audit-service)
  rpc AdminGrantQuestState(AdminGrantQuestStateRequest) returns (QuestInstance);
  rpc AdminRevertChoice(AdminRevertChoiceRequest) returns (Empty);
  rpc AdminWithdrawGenerated(AdminWithdrawGeneratedRequest) returns (Empty);
  rpc AdminAdjustCampaignThreshold(AdminAdjustThresholdRequest) returns (Campaign);
  rpc AdminRebuildCharacterQuests(AdminRebuildRequest) returns (AdminRebuildResult);
}

message ObjectiveProgress { string objective_key = 1; string type = 2; int32 current = 3; int32 required = 4; bool optional = 5; bool complete = 6; int32 order_group = 7; }
message QuestInstance {
  string quest_instance_id = 1; string quest_id = 2; string generated_quest_id = 3;
  string quest_type = 4; string difficulty = 5; string state = 6; string state_reason = 7;
  repeated ObjectiveProgress objectives = 8; string accepted_at = 9; string deadline_at = 10;
  string reward_preview_bundle_id = 11; double w_eff_preview = 12; string shared_by_character_id = 13;
}
message AcceptQuestRequest { string idempotency_key = 1; string character_id = 2; string quest_instance_id = 3; }
message TurnInQuestRequest { string idempotency_key = 1; string character_id = 2; string quest_instance_id = 3; string npc_entity_id = 4; string reward_option_bundle_id = 5; string signal_id = 6; }
message TurnInResult { QuestInstance instance = 1; string reward_grant_id = 2; }
message MakeChoiceRequest { string idempotency_key = 1; string character_id = 2; string quest_instance_id = 3; string choice_key = 4; string option = 5; string signal_id = 6; }
message MakeChoiceResult { QuestInstance instance = 1; repeated string applied_consequences = 2; }

message Signal {
  string signal_id = 1; string signal_kind = 2; repeated string character_ids = 3;
  string npc_entity_id = 4; string npc_id = 5; string object_ref = 6; string area_id = 7;
  double distance_m = 8; double presence_ratio = 9; string occurred_at = 10; string server_instance_id = 11;
  map<string, string> attrs = 12;
}
message ReportSignalsRequest { repeated Signal signals = 1; }
message ReportSignalsResult { repeated string accepted_signal_ids = 1; repeated string rejected_signal_ids = 2; }

message GenerationRequest {
  string request_id = 1; string realm_id = 2; string world_region_id = 3; repeated string zone_ids = 4;
  string reason = 5; string source_ref = 6; FactionContext faction_context = 7; repeated ResourceContext resource_context = 8;
  int32 level_min = 9; int32 level_max = 10; int32 active_players_estimate = 11; int32 desired_count = 12; int32 ttl_s = 13;
  repeated string template_hints = 14;
}
message FactionContext { string controlling_faction_id = 1; bool contested = 2; string war_state = 3; }
message ResourceContext { string resource_id = 1; double scarcity_index = 2; }
message GenerationResult { repeated string generated_quest_ids = 1; repeated Rejection rejected = 2; int32 remaining_region_budget = 3; }
message Rejection { string template_id = 1; string stage = 2; string reason = 3; }

message ContributeRequest { string idempotency_key = 1; string character_id = 2; string campaign_id = 3; string action = 4; repeated ItemRef items = 5; int64 amount_copper = 6; string evidence_event_id = 7; }
message ItemRef { string item_instance_id = 1; int32 quantity = 2; }
message ContributionResult { int64 accepted_cp_x2 = 1; int64 raw_cp = 2; double share = 3; string tier_preview = 4; int64 stage_progress_cp = 5; int64 stage_threshold_cp = 6; }
```

Error model (gRPC status → reason in `google.rpc.ErrorInfo.reason`):

| Status | Reasons |
| --- | --- |
| `FAILED_PRECONDITION` | `QUEST_WRONG_STATE`, `PREREQ_NOT_MET`, `EXCLUSIVE_LOCKED`, `REPEAT_LIMIT`, `ACTIVE_CAP`, `CAMPAIGN_NOT_ACTIVE`, `HOLD_CHECK_FAILED` |
| `PERMISSION_DENIED` | `NOT_OWNER`, `SIGNAL_PRODUCER_NOT_ALLOWED`, `NOT_IN_PARTY` |
| `OUT_OF_RANGE` | `TOO_FAR_FROM_GIVER` |
| `RESOURCE_EXHAUSTED` | `GEN_RATE_LIMIT`, `CAMPAIGN_RATE_GATE` |
| `NOT_FOUND` | `QUEST_NOT_FOUND`, `DEFINITION_NOT_IN_PACK` |
| `ALREADY_EXISTS` | `CHOICE_ALREADY_MADE` (returns the recorded choice) |

Allowed callers (enforced by mTLS identity):

| RPC group | Callers |
| --- | --- |
| Player log RPCs | gateway, zone-server |
| `TurnInQuest`, `MakeChoice`, `ReportSignals`, `ContributeToCampaign` | zone-server, instance-server |
| `ProposeGeneratedQuests`, `InvalidateSource` | ai-director-service only; faction-service supplies context to the director, which proxies faction-pressure requests (AI spec, Factions part, Faction AI) |
| `GetGiverStates` | zone-server |
| Admin | admin-portal (with GM role claims) |

## 5. Event payloads (`quest.*`)

All events use the registry envelope unchanged. `account_id`, `character_id`, `realm_id`, `zone_id`, and `correlation_id` are set in the envelope when known. The envelope `privacy_class` is the highest class among the payload fields. All quest payload fields are `p0_public` or `p1_pseudonymous`; no quest event carries `p2`/`p3` data. Topic `domain.quest`: key `character_id` for instance, objective, and choice events; key `campaign_id` for campaign events; key `generated_quest_id` for `quest.template.generated`. `schema_version = 1` for all.

Common fields used by several events:

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `quest_instance_id` | string `qst_` | yes | Instance | p1_pseudonymous |
| `quest_id` | string content id | yes | Definition (or `gqd_`-derived id `quest/gen_<template_slug>`) | p0_public |
| `generated_quest_id` | string `gqd_` | no | Present for generated quests | p0_public |
| `quest_type` | enum `quest_type` | yes | — | p0_public |
| `difficulty` | enum `difficulty` | yes | — | p0_public |
| `definition_revision` | int | yes | Pinned revision | p0_public |
| `character_level` | int | yes | Level at event time | p1_pseudonymous |

### 5.1 `quest.instance.offered`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| common fields | — | yes | as above | — |
| `trigger_kind` | string | yes | Trigger kind (§3.1 of the Model part) | p0_public |
| `giver_npc_id` | string | no | `npc/<slug>` | p0_public |
| `shared_by_character_id` | string `chr_` | no | Share source | p1_pseudonymous |
| `offer_expires_at` | timestamp | no | — | p0_public |

### 5.2 `quest.instance.accepted`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| common fields | — | yes | — | — |
| `accepted_at` | timestamp | yes | — | p0_public |
| `deadline_at` | timestamp | no | Time-limited quests | p0_public |
| `party_id` | string `pty_` | no | Party at accept | p1_pseudonymous |
| `active_count` | int | yes | Active quests after accept | p1_pseudonymous |

### 5.3 `quest.objective.progressed`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `quest_instance_id`, `quest_id`, `quest_type` | — | yes | — | — |
| `objective_key` | string | yes | — | p0_public |
| `objective_type` | string | yes | Catalog type | p0_public |
| `delta` | int | yes | May be negative only for `build` destruction / `collect` / hold recheck | p0_public |
| `current` | int | yes | After applying | p0_public |
| `required` | int | yes | — | p0_public |
| `evidence_kind` | enum `event`, `signal`, `snapshot`, `rpc` | yes | — | p0_public |
| `evidence_ref` | string | yes | Source `event_id` or `signal_id` (also envelope `causation_id`) | p1_pseudonymous |
| `recheck` | bool | no | True for a hold-check correction | p0_public |

Volume control: progressed events for the same `(instance, objective)` are coalesced within 2 s windows (`tune.quest.progress_event_coalesce_ms`), so 10 kills in an AoE pull produce one event with `delta = 10`.

### 5.4 `quest.objective.completed`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `quest_instance_id`, `quest_id`, `quest_type` | — | yes | — | — |
| `objective_key`, `objective_type` | string | yes | — | p0_public |
| `seconds_since_accept` | int | yes | — | p0_public |
| `instance_ready` | bool | yes | True if this completion made the instance `ready_to_turn_in` or `completed` | p0_public |

### 5.5 `quest.instance.completed`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| common fields | — | yes | — | — |
| `completion_mode` | enum `turn_in`, `auto`, `consolation`, `admin` | yes | — | p0_public |
| `reward_grant_id` | string `rwg_` | no | Absent if zero-reward consolation | p1_pseudonymous |
| `reward_bundle_id` | string | yes | Selected bundle | p0_public |
| `reward_level` | int | yes | Clamped L | p0_public |
| `w_eff` | number | yes | Effective weight | p0_public |
| `w_components` | object `{xp, gold, marks, lp}` | yes | Component weights | p0_public |
| `repeat_policy` | string | yes | — | p0_public |
| `n_credited` | int | yes | Group size used for `g_mult` | p0_public |
| `duration_s` | int | yes | Accept → complete | p0_public |
| `chain_id`, `chain_step` | string, int | no | — | p0_public |

### 5.6 `quest.instance.turned_in`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `quest_instance_id`, `quest_id`, `quest_type` | — | yes | — | — |
| `turn_in_npc_id` | string | yes | NPC used (may be a fallback) | p0_public |
| `used_fallback` | bool | yes | — | p0_public |
| `reward_option_bundle_id` | string | no | Selected option | p0_public |
| `consumed_items` | array `{item_id, quantity}` | no | Delivered items (sink `quest_turn_in`) | p0_public |
| `reward_grant_id` | string `rwg_` | yes | — | p1_pseudonymous |

### 5.7 `quest.instance.failed`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| common fields | — | yes | — | — |
| `failure_reason` | enum `timer`, `escort_target_died`, `left_area`, `exclusive_branch_taken`, `source_resolved_failed`, `condition` | yes | — | p0_public |
| `progress_ratio` | number 0–1 | yes | `p` | p0_public |
| `retry_available_at` | timestamp | no | — | p0_public |

### 5.8 `quest.instance.abandoned`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| common fields | — | yes | — | — |
| `reason` | enum `player_abandoned`, `declined`, `admin` | yes | — | p0_public |
| `from_state` | enum `offered`, `active`, `ready_to_turn_in` | yes | — | p0_public |
| `progress_ratio` | number | yes | — | p0_public |
| `seconds_since_accept` | int | no | — | p0_public |

### 5.9 `quest.instance.expired`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| common fields | — | yes | — | — |
| `from_state` | enum | yes | — | p0_public |
| `expiry_cause` | enum `offer_timeout`, `source_ended`, `turn_in_grace`, `withdrawn` | yes | — | p0_public |
| `source_ref` | string | no | `wev_` etc. | p0_public |
| `progress_ratio` | number | yes | — | p0_public |

### 5.10 `quest.choice.made`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `quest_instance_id`, `quest_id` | — | yes | — | — |
| `choice_key` | string | yes | — | p0_public |
| `option` | string | yes | — | p0_public |
| `consequences` | array `{kind, target, value}` | yes | Applied consequences (bounded) | p0_public |
| `exclusive_group` | string | no | Group locked | p0_public |
| `reverted` | bool | yes | True for GM reversal records | p0_public |
| `audit_id` | string `aud_` | no | For reversals | p1_pseudonymous |

### 5.11 `quest.template.generated`

Envelope has no `character_id`.

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `generated_quest_id` | string `gqd_` | yes | — | p0_public |
| `template_id`, `template_revision` | string, int | yes | — | p0_public |
| `request_id` | string | yes | Director request | p0_public |
| `reason` | string | yes | — | p0_public |
| `source_ref` | string | no | — | p0_public |
| `world_region_id` | string | yes | — | p0_public |
| `objective_types` | string[] | yes | — | p0_public |
| `difficulty` | enum | yes | — | p0_public |
| `w_base_bound` | number | yes | Weight including scarcity | p0_public |
| `expires_at` | timestamp | yes | — | p0_public |
| `text_origin` | enum `precomputed`, `llm` | yes | — | p0_public |
| `rejected_candidates` | int | yes | Count rejected in this request | p0_public |

### 5.12 `quest.campaign.contributed`

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `campaign_id` | string `cmp_` | yes | — | p0_public |
| `campaign_def_id` | string | yes | — | p0_public |
| `stage` | int | yes | — | p0_public |
| `action` | string | yes | `kill`, `deliver_item`, `donate_gold`, `complete_quest` | p0_public |
| `raw_cp` | int | yes | — | p1_pseudonymous |
| `accepted_cp_x2` | int | yes | — | p1_pseudonymous |
| `character_total_cp_x2` | int | yes | Accepted stage total | p1_pseudonymous |
| `capped` | enum `none`, `soft`, `hard`, `rate_gate` | yes | — | p1_pseudonymous |
| `stage_progress_cp` | int | yes | Display counter after this contribution | p0_public |

Character contributions are coalesced per 10 s per character (`tune.quest.contribution_event_coalesce_ms`).

### 5.13 `quest.campaign.stage_reached`

Envelope has no `character_id`.

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `campaign_id`, `campaign_def_id` | string | yes | — | p0_public |
| `stage` | int | yes | — | p0_public |
| `outcome` | enum `reached`, `failed` | yes | — | p0_public |
| `final` | bool | yes | Last stage | p0_public |
| `threshold_cp` | int | yes | Locked `T_s` | p0_public |
| `population_p` | int | yes | `P_s` | p0_public |
| `progress_cp` | int | yes | Final recomputed progress | p0_public |
| `participants` | int | yes | Characters with accepted CP > 0 | p0_public |
| `tier_counts` | object | yes | `{bronze, silver, gold}` | p0_public |
| `duration_h` | number | yes | Stage open → close | p0_public |
| `world_effect_hint` | string | no | For ai-director-service | p0_public |

## 6. Analytics hooks

Analytics (Analytics spec) owns metric definitions. These are the quest metrics the Quest domain needs, with their source events.

| Metric | Definition | Source events | Use |
| --- | --- | --- | --- |
| `m_quest_acceptance_rate` | accepted / offered per quest | offered, accepted | Offer relevance |
| `m_quest_completion_rate` | completed / accepted | accepted, completed | Difficulty and clarity |
| `m_quest_abandon_rate` | abandoned(from active) / accepted | abandoned | Pain points |
| `m_quest_median_minutes` | median `duration_s` / 60 by quest and type | completed | Time targets (§2.1) |
| `m_quest_objective_dropoff` | Largest step loss between objective completions | objective.completed | Chain design |
| `m_quest_xp_share` | XP with `source = quest_reward` / total XP by band | progression.xp.granted (+ causation) | §2.1 target |
| `m_quest_gold_faucet_share` | §2.2 | economy.currency.credited | Economy balance |
| `m_quest_marks_share` | Quest marks / weekly marks | economy.currency.credited | §2.3 |
| `m_quest_choice_split` | Option distribution per choice | choice.made | Narrative review |
| `m_generated_quest_rejection_rate` | rejected / candidates by stage, template | template.generated (+ ops log) | Template health |
| `m_generated_quest_uptake` | accepted instances per gqd | template.generated, accepted | Director tuning |
| `m_llm_text_fallback_rate` | fallback / LLM attempts | text worker metrics | Guardrail health |
| `m_llm_text_cache_hit_rate` | cache hits / lookups | text worker metrics | Cost |
| `m_llm_text_reject_rate` | reviewer rejections / reviewed | admin-portal review | Safety |
| `m_campaign_participation_rate` | participants / `P_s` | stage_reached | §2.5 of Generation part |
| `m_campaign_stage_hours` | `duration_h` | stage_reached | Threshold calibration |
| `m_campaign_cap_hit_rate` | Characters at hard cap / participants | contributed | Cap tuning |
| `m_quest_turn_in_pending_age` | Age of instances with `turn_in_pending` | ops metric | Saga health (alert > 15 min) |
| `m_quest_event_lag_ms` | `processed_at − occurred_at` p99 | consumer metric | Pipeline health (alert > 5,000 ms) |

Dashboards: quest funnel per chain, generated-content health, campaign live board (progress vs. expected curve), and faucet shares. Alerts route to `ops.alerts`.

## 7. Security considerations

Threats specific to quests, the server rules that close them, and what is reported to `anticheat-service`. Detectors and scoring are owned by the Security spec.

| # | Threat | Server rule | Report to anticheat-service |
| --- | --- | --- | --- |
| 1 | **Objective spoofing**: a client forges progress or turn-in | No client-callable progress RPC exists. Progress comes only from domain events and allowlisted service signals (mTLS identity). `TurnInQuest`/`MakeChoice` accepted only from zone-server, with server-measured distance. `character_id` taken from the session. | Rejected signals from unexpected producers. Turn-in attempts beyond distance (> 3 per 10 min). |
| 2 | **Replayed events or signals** double-count progress | Inbox dedup on `event_id(+character_id)` and `signal_id`. Progress clamped to `required`. | — (ops metric) |
| 3 | **Duplicate rewards** from turn-in retries or races | Turn-in idempotency key `qst:turn_in`. CAS on instance `version`. Unique open-instance index. | Repeated idempotent-replay bursts from one session |
| 4 | **AFK farming of escort/defend** | Presence-ratio and participation thresholds measured by the server. Kill credit requires `credited_character_ids` (damage/heal contribution, Combat spec). Dynamic daily full-reward limit. | Characters completing escort/defend with presence at threshold ± 2 % in > 80 % of completions over 7 days |
| 5 | **Kill-credit leeching** through shared party credit | Shared credit requires the same objective active, the same party, and 100 m radius. Party joins within 10 s before a kill are not credited (`tune.quest.share_join_grace_s`). | Party-hopping patterns |
| 6 | **Generated-quest rerolling** (abandon until a lucrative one appears) | 900 s abandon cooldown per gqd. Offers per board fixed per seed bucket. Weight ceiling 1.5. Hard daily dynamic limit 40. | Abandon rate > 50 % with > 20 abandons/day |
| 7 | **Steering the director** (players manipulating world state to spawn easy quests) | Region rate limits. Feasibility rules. Template cooldowns. Influence per character per week capped. | Coordinated influence spikes by linked accounts |
| 8 | **Contribution arbitrage** (cheap market items converted to CP) | `unit_value = min(vendor, market median 7d)`. Gold contributions level-scaled. Soft/hard caps. Hourly rate gate. | Rate-gate hits, `capped = hard` streaks |
| 9 | **Self-dealing trade objectives** | `trade` objectives ignore counterparties with the same account or linked accounts (Security spec account linkage). Minimum value. | Flagged pairs (also `security.trade.flagged` from anticheat) |
| 10 | **Choice exploitation** (abandon after choice to re-roll consequences) | Choices are permanent and survive abandonment. Consequences idempotent per `(qst, consequence)`. | — |
| 11 | **Prompt injection or abuse via LLM text** | No player text in prompts. Structured output. Allowlisted entities. Classifier. Fail closed. Kill switch. | — (content safety pipeline) |
| 12 | **Admin abuse** | All admin RPCs audited (`audit-service`). Threshold adjustments only downward. Choice reversal requires a second GM approval for > 10 characters. | — |
| 13 | **Botting quest loops** (repeatables) | Repeatable multiplier 0.25 with cooldown. `detector/quest_rate_anomaly` (proposed): completions/hour > P99.9 for level band, or inter-completion timing variance < threshold. | Feature vector per character hourly |

Signals sent to anticheat-service: rejected RPCs (reason codes above), plus an hourly per-character feature summary `{completions_by_type, median_interval_s, interval_cv, presence_ratios, abandon_count, rate_gate_hits}`. Consequences are decided by the Security spec. Quest only enforces local rules and limits. On an enforcement rollback (`enforcement.rollback.executed` with `progression_rollback`), quest-service marks affected instances `completed` with an `admin` reversal record and does not re-grant. Reward revocation goes through `reward-service` (`reward.grant.revoked`).
