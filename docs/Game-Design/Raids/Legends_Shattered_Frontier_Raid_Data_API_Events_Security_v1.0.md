# Legends of the Shattered Frontier — Raid Part 3: Data, API, Events, Analytics and Security
### Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

> Status: Design target; not implemented. Names per Shared Registry v1.0.
>
> Entry: [Raid Architecture](../Legends_Shattered_Frontier_Raid_Architecture_v1.0.md) · Registry: [Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md)

This part specifies the raid tables in the `group` and `instance` schemas, the raid gRPC extensions of `group-service` and `instance-service`, event payloads for `raid.*` and the raid fields on `encounter.*`, `instance.*`, and `group.*` events, analytics hooks, and security considerations. Base tables and base payloads (`instance_runs`, `encounter_attempts`, `lockouts`, `parties`, base `encounter.*`/`instance.*` fields) belong to the **Dungeon spec**; raid tables reference them by foreign key and raid payload fields live under a `raid` object so they never collide with Dungeon fields.

---

## 1. Schemas (PostgreSQL)

### 1.1 Schema `group` (group-service, raid parts)

```sql
-- Raid groups. Hot state mirrors to Redis key rgp:{raid_group_id}; PostgreSQL is the durable snapshot.
CREATE TABLE "group".raid_groups (
  raid_group_id            text PRIMARY KEY CHECK (raid_group_id LIKE 'rgp\_%'),
  realm_id                 text NOT NULL,
  state                    text NOT NULL CHECK (state IN ('forming','active','disbanded')),  -- registry: raid_group_state
  leader_character_id      text NOT NULL,
  loot_mode                text NOT NULL DEFAULT 'personal'
                             CHECK (loot_mode IN ('personal','group_need_greed','master_loot','council')),
  loot_mode_effective_at   timestamptz NOT NULL DEFAULT now(),   -- 60 s cooldown (Part 1)
  loot_master_character_id text,
  loot_threshold_rarity    text NOT NULL DEFAULT 'epic'
                             CHECK (loot_threshold_rarity IN ('common','uncommon','rare','epic','legendary','mythic')),
  target_raid_id           text,              -- raid/<slug>
  target_difficulty        text CHECK (target_difficulty IN ('normal','veteran','heroic','mythic')),
  guild_id                 text,              -- gld_ when >= guild_credit_share of members
  formed_from_party_id     text,              -- pty_ if converted
  version                  bigint NOT NULL DEFAULT 0,
  created_at               timestamptz NOT NULL DEFAULT now(),
  disbanded_at             timestamptz,
  CHECK (loot_mode NOT IN ('master_loot','council') OR loot_master_character_id IS NOT NULL)
);

CREATE TABLE "group".raid_group_members (
  raid_group_id   text NOT NULL REFERENCES "group".raid_groups ON DELETE CASCADE,
  character_id    text NOT NULL,
  account_id      text NOT NULL,
  raid_rank       text NOT NULL CHECK (raid_rank IN ('leader','assistant','member')),
  subgroup        smallint NOT NULL CHECK (subgroup BETWEEN 1 AND 5),
  combat_role     text NOT NULL CHECK (combat_role IN ('tank','healer','damage','support')),
  is_council      boolean NOT NULL DEFAULT false,
  joined_at       timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY (raid_group_id, character_id)
);
-- One active group per character (parties enforce the same in their own table; group-service checks both in one Redis Lua script).
CREATE UNIQUE INDEX raid_group_members_one_active ON "group".raid_group_members (character_id);
CREATE INDEX raid_group_members_subgroup ON "group".raid_group_members (raid_group_id, subgroup);

CREATE TABLE "group".raid_ready_checks (
  ready_check_id   text PRIMARY KEY,           -- ULID (no registry prefix; internal)
  raid_group_id    text NOT NULL REFERENCES "group".raid_groups,
  initiated_by     text NOT NULL,
  started_at       timestamptz NOT NULL,
  completed_at     timestamptz,
  all_ready        boolean,
  missing_capabilities text[] NOT NULL DEFAULT '{}'
);

CREATE TABLE "group".raid_ready_check_responses (
  ready_check_id  text NOT NULL REFERENCES "group".raid_ready_checks ON DELETE CASCADE,
  character_id    text NOT NULL,
  response        text NOT NULL CHECK (response IN ('ready','not_ready','no_response')),
  in_instance     boolean NOT NULL,
  ilvl_equipped   integer,
  responded_at    timestamptz,
  PRIMARY KEY (ready_check_id, character_id)
);

CREATE TABLE "group".raid_listings (
  listing_id       text PRIMARY KEY,
  raid_group_id    text NOT NULL REFERENCES "group".raid_groups,
  realm_id         text NOT NULL,
  raid_id          text NOT NULL,
  difficulty       text NOT NULL CHECK (difficulty IN ('normal','veteran','heroic','mythic')),
  roles_wanted     jsonb NOT NULL,             -- {"tank":1,"healer":2,"damage":4,"support":0}
  min_ilvl         integer,
  note             text CHECK (length(note) <= 140),
  expires_at       timestamptz NOT NULL
);
CREATE INDEX raid_listings_browse ON "group".raid_listings (realm_id, raid_id, difficulty, expires_at);
```

### 1.2 Schema `instance` (instance-service, raid parts)

```sql
-- Raid extension of the Dungeon spec's instance.instance_runs (1:1).
CREATE TABLE instance.raid_runs (
  run_id              text PRIMARY KEY REFERENCES instance.instance_runs(run_id),
  raid_id             text NOT NULL,                       -- raid/<slug>
  raid_group_id       text NOT NULL,                       -- rgp_
  group_size_class    text NOT NULL CHECK (group_size_class IN ('raid_small','raid_large')),
  current_difficulty  text NOT NULL CHECK (current_difficulty IN ('normal','veteran','heroic','mythic')),
  save_mode           text NOT NULL DEFAULT 'none' CHECK (save_mode IN ('none','progress')),
  resolve_opt_out     boolean NOT NULL DEFAULT false,
  week_key            text NOT NULL,                       -- e.g. 'na:2026-W42' (dc_region weekly reset)
  extension_generation smallint NOT NULL DEFAULT 0,
  CHECK (current_difficulty <> 'mythic' OR save_mode = 'progress')
);

CREATE TABLE instance.raid_run_encounter_states (
  run_id         text NOT NULL REFERENCES instance.raid_runs,
  difficulty     text NOT NULL,
  encounter_id   text NOT NULL,                            -- encounter/<slug>
  encounter_state text NOT NULL CHECK (encounter_state IN ('dormant','idle','engaged','defeated','resetting')),
  defeated_attempt_id text,                                -- att_
  defeated_at    timestamptz,
  PRIMARY KEY (run_id, difficulty, encounter_id)
);

-- Raid extension of the Dungeon spec's instance.encounter_attempts (1:1).
CREATE TABLE instance.raid_attempt_details (
  attempt_id        text PRIMARY KEY REFERENCES instance.encounter_attempts(attempt_id),
  run_id            text NOT NULL,
  difficulty        text NOT NULL,
  n_eng             smallint NOT NULL CHECK (n_eng BETWEEN 1 AND 25),
  scaling           jsonb NOT NULL,     -- {"s":0.7,"hp_mult":0.6876,"H":4558967,"adds":4,"add_hp_corr":1.05,"targets":3,"cres":2}
  seed              bigint NOT NULL,
  tuning_version    bigint NOT NULL,
  resolve_pct       numeric(4,2) NOT NULL DEFAULT 0,
  phase_reached     text NOT NULL,
  min_boss_hp_frac  numeric(6,5) NOT NULL,
  duration_ms       integer,
  eligible_character_ids text[] NOT NULL DEFAULT '{}',
  ineligible        jsonb NOT NULL DEFAULT '[]',  -- [{"character_id":..,"reason":"loot_lockout|afk|late_join|progress_locked"}]
  validation_status text NOT NULL DEFAULT 'pending' CHECK (validation_status IN ('pending','passed','failed')),
  validation_flags  text[] NOT NULL DEFAULT '{}'
);

CREATE TABLE instance.raid_mechanic_failures (
  attempt_id     text NOT NULL REFERENCES instance.raid_attempt_details,
  mechanic_kind  text NOT NULL,      -- registry: raid_mechanic_kind
  mechanic_id    text NOT NULL,      -- ability/<slug> that produced it
  instance_seq   integer NOT NULL,
  character_id   text,               -- null for raid-attributed failures
  phase          text NOT NULL,
  server_tick    bigint NOT NULL,
  lethal         boolean NOT NULL,
  PRIMARY KEY (attempt_id, instance_seq, mechanic_id, character_id)
);

-- Raid columns for the Dungeon spec's instance.lockouts (1:1 extension).
CREATE TABLE instance.raid_lockout_details (
  lockout_id        text PRIMARY KEY REFERENCES instance.lockouts(lockout_id),   -- lck_
  lockout_kind      text NOT NULL CHECK (lockout_kind IN ('loot','progress')),
  raid_id           text NOT NULL,
  difficulty        text NOT NULL,
  encounter_id      text,             -- required for loot
  run_id            text,             -- required for progress
  extension_count   smallint NOT NULL DEFAULT 0 CHECK (extension_count <= 3),
  CHECK ((lockout_kind = 'loot' AND encounter_id IS NOT NULL) OR (lockout_kind = 'progress' AND run_id IS NOT NULL))
);

CREATE TABLE instance.raid_progress (
  raid_progress_id  text PRIMARY KEY CHECK (raid_progress_id LIKE 'rpr\_%'),
  owner_kind        text NOT NULL CHECK (owner_kind IN ('guild','raid_group')),
  owner_id          text NOT NULL,
  realm_id          text NOT NULL,
  raid_id           text NOT NULL,
  difficulty        text NOT NULL,
  tier              smallint NOT NULL,
  bosses_defeated   smallint NOT NULL DEFAULT 0,
  version           bigint NOT NULL DEFAULT 0,
  updated_at        timestamptz NOT NULL DEFAULT now(),
  UNIQUE (owner_kind, owner_id, raid_id, difficulty, tier)
);

CREATE TABLE instance.raid_progress_encounters (
  raid_progress_id  text NOT NULL REFERENCES instance.raid_progress ON DELETE CASCADE,
  encounter_id      text NOT NULL,
  first_kill_at     timestamptz,
  first_kill_attempt_id text,
  kill_count        integer NOT NULL DEFAULT 0,
  wipe_count_before_first_kill integer NOT NULL DEFAULT 0,
  best_kill_ms      integer,
  best_kill_attempt_id text,
  best_min_hp_frac  numeric(6,5),
  PRIMARY KEY (raid_progress_id, encounter_id)
);

CREATE TABLE instance.raid_first_kills (
  scope            text NOT NULL CHECK (scope IN ('realm','dc_region')),
  scope_id         text NOT NULL,                -- realm_id or dc_region
  raid_id          text NOT NULL,
  difficulty       text NOT NULL,
  encounter_id     text NOT NULL,
  attempt_id       text NOT NULL,
  raid_progress_id text NOT NULL,
  ended_at         timestamptz NOT NULL,
  tuning_version   bigint NOT NULL,
  roster           text[] NOT NULL,
  status           text NOT NULL CHECK (status IN ('held','recorded','revoked')),
  recorded_at      timestamptz,
  PRIMARY KEY (scope, scope_id, raid_id, difficulty, encounter_id, attempt_id)
);
CREATE UNIQUE INDEX raid_first_kills_one_recorded
  ON instance.raid_first_kills (scope, scope_id, raid_id, difficulty, encounter_id) WHERE status = 'recorded';

CREATE TABLE instance.raid_world_effect_applications (
  application_key  text PRIMARY KEY,             -- world:{realm}:{effect_id}:{week_key} or world:{realm}:{effect_id}:once
  realm_id         text NOT NULL,
  raid_id          text NOT NULL,
  effect_id        text NOT NULL,
  effect_type      text NOT NULL CHECK (effect_type IN ('faction_influence','narrative_flag','world_event_trigger','regional_buff','vendor_unlock')),
  magnitude        numeric(8,2),
  inputs           jsonb NOT NULL,               -- {"I_week":103,"kills":{...}}
  event_id         text NOT NULL,                -- evt_ of raid.world_effect.applied
  reversal_of      text,
  applied_at       timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE instance.raid_bonus_roll_commitments (
  character_id   text NOT NULL,
  week_key       text NOT NULL,
  raid_id        text NOT NULL,
  difficulty     text NOT NULL,
  encounter_id   text NOT NULL,
  txn_id         text,              -- economy-service txn_ once debited at kill
  status         text NOT NULL CHECK (status IN ('committed','spent','released')),
  PRIMARY KEY (character_id, week_key, raid_id, difficulty, encounter_id)
);
```

Notes: live-lockout uniqueness ("at most one `active`/`extended` loot lockout per character, raid, difficulty, encounter") is enforced in instance-service inside the kill transaction using `SELECT … FOR UPDATE` on the base `lockouts` rows (the Dungeon spec owns the base unique index on `(character_id, content_id, difficulty, scope_key) WHERE state IN ('active','extended')`; raid sets `scope_key` = `encounter_id` for loot and `'progress'` for progress lockouts). All tables are realm-partitioned by `realm_id` hash in production (Database spec).

---

## 2. gRPC API extensions

### 2.1 `lsf.group_service.v1` — RaidGroupService

```protobuf
syntax = "proto3";
package lsf.group_service.v1;

service RaidGroupService {
  rpc CreateRaidGroup(CreateRaidGroupRequest) returns (RaidGroup);
  rpc ConvertPartyToRaid(ConvertPartyToRaidRequest) returns (RaidGroup);
  rpc InviteToRaid(InviteToRaidRequest) returns (InviteResult);
  rpc RespondToRaidInvite(RespondToRaidInviteRequest) returns (RaidGroup);
  rpc LeaveRaidGroup(LeaveRaidGroupRequest) returns (LeaveRaidGroupResponse);
  rpc RemoveRaidMember(RemoveRaidMemberRequest) returns (RaidGroup);
  rpc MoveRaidMember(MoveRaidMemberRequest) returns (RaidGroup);      // subgroup move/swap
  rpc SetRaidRank(SetRaidRankRequest) returns (RaidGroup);
  rpc TransferRaidLeadership(TransferRaidLeadershipRequest) returns (RaidGroup);
  rpc SetRaidLootSettings(SetRaidLootSettingsRequest) returns (RaidGroup);
  rpc SetRaidTarget(SetRaidTargetRequest) returns (RaidGroup);
  rpc StartRaidReadyCheck(StartRaidReadyCheckRequest) returns (ReadyCheck);
  rpc RespondRaidReadyCheck(RespondRaidReadyCheckRequest) returns (ReadyCheck);
  rpc DisbandRaidGroup(DisbandRaidGroupRequest) returns (DisbandRaidGroupResponse);
  rpc CreateRaidListing(CreateRaidListingRequest) returns (RaidListing);
  rpc BrowseRaidListings(BrowseRaidListingsRequest) returns (BrowseRaidListingsResponse);
  rpc GetRaidGroup(GetRaidGroupRequest) returns (RaidGroup);
  // internal (instance-service, loot-service)
  rpc GetRaidGroupSnapshot(GetRaidGroupSnapshotRequest) returns (RaidGroupSnapshot);
}

message RaidMember {
  string character_id = 1; string raid_rank = 2; int32 subgroup = 3;
  string combat_role = 4; bool is_council = 5; bool online = 6;
}
message RaidGroup {
  string raid_group_id = 1; string state = 2; string leader_character_id = 3;
  repeated RaidMember members = 4; string loot_mode = 5; string loot_master_character_id = 6;
  string loot_threshold_rarity = 7; string target_raid_id = 8; string target_difficulty = 9;
  string guild_id = 10; int64 version = 11; string loot_mode_effective_at = 12;
}
message SetRaidLootSettingsRequest {
  string idempotency_key = 1; string actor_character_id = 2; string raid_group_id = 3; int64 expected_version = 4;
  string loot_mode = 5; string loot_master_character_id = 6; repeated string council_character_ids = 7;
  string loot_threshold_rarity = 8;
}
message MoveRaidMemberRequest {
  string idempotency_key = 1; string actor_character_id = 2; string raid_group_id = 3; int64 expected_version = 4;
  string character_id = 5; int32 to_subgroup = 6; string swap_with_character_id = 7;
}
message RaidGroupSnapshot { RaidGroup group = 1; string as_of = 2; }   // used at engage and at kill
```

| RPC | Authz | Key errors |
| --- | --- | --- |
| `SetRaidLootSettings` | leader | `ENCOUNTER_ENGAGED`, `VERSION_CONFLICT`, `LOOT_MASTER_NOT_ASSISTANT`, `COUNCIL_SIZE_INVALID` (2–4) |
| `MoveRaidMember` | leader, assistant | `ENCOUNTER_ENGAGED`, `SUBGROUP_FULL`, `VERSION_CONFLICT` |
| `SetRaidRank` | leader | `ASSISTANT_LIMIT` (4) |
| `ConvertPartyToRaid` | party leader | `ALREADY_IN_RAID`, `PARTY_IN_INSTANCE_QUEUE` |
| `InviteToRaid` | leader, assistant | `RAID_FULL` (25), `TARGET_IN_GROUP`, `PROGRESS_LOCKED_HINT` (warning, not error) |
| `StartRaidReadyCheck` | leader, assistant | `READY_CHECK_COOLDOWN` |

All mutating RPCs take `idempotency_key` and `expected_version`; responses on replay return the original result.

### 2.2 `lsf.instance_service.v1` — RaidService

```protobuf
syntax = "proto3";
package lsf.instance_service.v1;

service RaidService {
  // player-facing via gateway
  rpc CheckRaidEligibility(CheckRaidEligibilityRequest) returns (RaidEligibility);
  rpc RequestRaidRun(RequestRaidRunRequest) returns (RaidRun);          // wraps Dungeon CreateRun with raid fields
  rpc SetRaidRunDifficulty(SetRaidRunDifficultyRequest) returns (RaidRun);
  rpc ResetRaidRun(ResetRaidRunRequest) returns (ResetRaidRunResponse);
  rpc GetRaidLockouts(GetRaidLockoutsRequest) returns (GetRaidLockoutsResponse);
  rpc ExtendRaidLockout(ExtendRaidLockoutRequest) returns (RaidLockout);
  rpc CancelRaidLockoutExtension(CancelRaidLockoutExtensionRequest) returns (RaidLockout);
  rpc CommitBonusRoll(CommitBonusRollRequest) returns (BonusRollCommitment);
  rpc GetRaidProgress(GetRaidProgressRequest) returns (RaidProgress);
  rpc ListRealmFirsts(ListRealmFirstsRequest) returns (ListRealmFirstsResponse);
  // internal: instance-server → instance-service
  rpc ReportRaidAttemptStarted(ReportRaidAttemptStartedRequest) returns (Ack);
  rpc ReportRaidPhaseChanged(ReportRaidPhaseChangedRequest) returns (Ack);
  rpc ReportMechanicFailures(ReportMechanicFailuresRequest) returns (Ack);   // batched ≤ 1 s
  rpc ReportRaidAttemptEnded(ReportRaidAttemptEndedRequest) returns (RaidAttemptResult);
  // internal: security / GM
  rpc InvalidateRaidAttempt(InvalidateRaidAttemptRequest) returns (InvalidateRaidAttemptResponse);
}

message RequestRaidRunRequest {
  string idempotency_key = 1; string raid_group_id = 2; string raid_id = 3; string difficulty = 4;
  string save_mode = 5;          // "none" | "progress" (heroic only; forced for mythic)
  bool resolve_opt_out = 6;
}
message RaidEligibility {
  string character_id = 1; bool can_enter = 2; repeated string reasons = 3;   // ATTUNEMENT_REQUIRED, PROGRESS_LOCKED, LEVEL_TOO_LOW
  repeated string loot_locked_encounter_ids = 4; string progress_locked_run_id = 5;
}
message ReportRaidAttemptEndedRequest {
  string idempotency_key = 1;    // "attempt-end:{attempt_id}"
  string attempt_id = 2; string outcome = 3;   // defeated | wiped | abandoned
  int32 duration_ms = 4; string phase_reached = 5; double min_boss_hp_frac = 6;
  repeated string present_character_ids = 7; KillEvidence evidence = 8;
}
message KillEvidence {
  int64 total_boss_damage = 1; int64 boss_max_hp = 2; double boss_max_distance_from_spawn_m = 3;
  repeated PhaseMark phases = 4; repeated ParticipantStats participants = 5; int64 seed = 6;
  repeated string anomaly_flags = 7;  // from instance-server self-checks
}
message ParticipantStats {
  string character_id = 1; int64 damage_done = 2; int64 healing_done = 3; int32 casts = 4;
  double active_fraction = 5; double max_speed_ratio = 6; bool alive_at_end = 7;
}
message InvalidateRaidAttemptRequest {
  string idempotency_key = 1; string attempt_id = 2; string reason_code = 3; string case_id = 4; // cas_
}
```

| RPC | Behavior |
| --- | --- |
| `ReportRaidAttemptEnded` | Single transaction: persist attempt + raid details, compute eligible set, write loot lockouts (and progress lockouts), set encounter defeated, update `rpr_`, insert `raid_first_kills` as `held`; commit; then publish events and call `reward-service.IssueGrant` per eligible character (idempotency `raidkill:{attempt_id}:{character_id}`). Retries are safe. |
| `ExtendRaidLockout` | Only for `progress` lockouts in `active`; rejects if `extension_count` = 3 or within 60 s of reset. |
| `InvalidateRaidAttempt` | Sets outcome `invalidated` (base table), revokes grants via `reward-service.RevokeGrant`, deletes the attempt's loot lockouts (restoring eligibility), revokes first kills, publishes reversals of world effects, subtracts RaidProgress. Requires security or GM role; audited via audit-service. |
| `CommitBonusRoll` | Records commitment; debit happens at kill via economy-service with registered sink `raid_bonus_roll`; released on wipe or reset. |

---

## 3. Event payloads

All events use the Registry envelope unchanged (`privacy_class` of the envelope = the highest class among payload fields). Raid-owned events below; Dungeon-owned events receive an additive `raid` object.

### 3.1 `raid.progress.first_kill_recorded` (owned)

Topic `domain.raid`, key `raid_progress_id`. Published after the first-kill hold.

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `scope` | string | yes | `realm` or `dc_region` | p0_public |
| `scope_id` | string | yes | realm ID or datacenter region | p0_public |
| `raid_id` | string | yes | `raid/<slug>` | p0_public |
| `encounter_id` | string | yes | `encounter/<slug>` | p0_public |
| `difficulty` | string | yes | `heroic` or `mythic` | p0_public |
| `attempt_id` | string | yes | `att_` of the defeating attempt | p1_pseudonymous |
| `run_id` | string | yes | `run_` | p1_pseudonymous |
| `raid_progress_id` | string | yes | `rpr_` credited | p0_public |
| `credited_kind` | string | yes | `guild` or `raid_group` | p0_public |
| `credited_id` | string | yes | `gld_` or `rgp_` | p0_public (guild) / p1_pseudonymous (group) |
| `roster_character_ids` | array<string> | yes | Participants | p1_pseudonymous |
| `ended_at` | timestamp | yes | Attempt end (server) | p0_public |
| `kill_duration_ms` | int | yes | Attempt duration | p0_public |
| `wipes_before_kill` | int | yes | From `rpr_` | p0_public |
| `tuning_version` | int | yes | For pre/post-nerf display | p0_public |
| `previous_revoked_attempt_id` | string | no | Set when promoted after a revocation | p1_pseudonymous |

```json
{ "scope": "realm", "scope_id": "na-1", "raid_id": "raid/ashen_citadel",
  "encounter_id": "encounter/ashen_citadel_cinder_king", "difficulty": "mythic",
  "attempt_id": "att_01JA0P8Q5S3N7K2M9V4X6B1C8D", "run_id": "run_01JA0M2C7H3B8N4P6Q1R5S9T0V",
  "raid_progress_id": "rpr_01J9ZZ4D6F8H1K3M5P7R9T2V4X", "credited_kind": "guild",
  "credited_id": "gld_01J9X5A7C9E1G3J5L7N9Q2S4U6", "roster_character_ids": ["chr_01J9…"],
  "ended_at": "2026-11-20T03:12:44.512Z", "kill_duration_ms": 498120, "wipes_before_kill": 287,
  "tuning_version": 5531 }
```

### 3.2 `raid.world_effect.applied` (owned)

Topic `domain.raid`, key `realm_id`.

| Field | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `application_key` | string | yes | Idempotency key (`world:{realm}:{effect_id}:{week_key}` or `…:once`) | p0_public |
| `raid_id` | string | yes | Source raid | p0_public |
| `effect_id` | string | yes | From raid `world_links` | p0_public |
| `effect_type` | string | yes | `faction_influence`, `narrative_flag`, `world_event_trigger`, `regional_buff`, `vendor_unlock` | p0_public |
| `world_region_id` | string | cond | Required for `faction_influence`, `regional_buff` | p0_public |
| `faction_id` | string | cond | Required for `faction_influence` | p0_public |
| `influence_points` | number | cond | `faction_influence` magnitude (Part 2, Weekly influence formula) | p0_public |
| `narrative_flag` | string | cond | For `narrative_flag` | p0_public |
| `world_event_id` | string | cond | For `world_event_trigger` | p0_public |
| `action` | string | cond | `resolve_series`, `start_series` | p0_public |
| `buff_id`, `duration_days` | string, int | cond | For `regional_buff` | p0_public |
| `week_key` | string | yes | Reset week | p0_public |
| `inputs` | object | yes | `{ "I_week": number, "kills_by_difficulty": {..} }` or triggering `attempt_id` | p0_public |
| `reversal_of` | string | no | `event_id` reversed | p0_public |

### 3.3 Registered raid/group events

| Event | Key fields (all p0/p1) |
| --- | --- |
| `raid.progress.updated` | `raid_progress_id`, `owner_kind`, `owner_id`, `raid_id`, `difficulty`, `encounter_id`, `kill_count`, `bosses_defeated`, `attempt_id`, `best_kill_ms` |
| `raid.first_kill.revoked` | `scope`, `scope_id`, `encounter_id`, `difficulty`, `attempt_id`, `reason_code`, `case_id` (p2_personal) |
| `group.raid_group.formed` | `raid_group_id`, `realm_id`, `formed_from_party_id`, `member_count`, `guild_id` |
| `group.raid_group.disbanded` | `raid_group_id`, `reason` (`leader`, `empty`, `idle`), `lifetime_s`, `runs_count` |
| `group.raid_group.updated` | `raid_group_id`, `change` (`rank`, `subgroup`, `loot_settings`, `leader`), `actor_character_id`, `before`, `after`, `version` |

### 3.4 Raid fields on Dungeon-owned events

Each adds an optional `raid` object to the Dungeon spec payload (additive, `schema_version` unchanged for consumers ignoring it).

**`encounter.attempt.started`**

| Field (`raid.*`) | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `raid_id` | string | yes | `raid/<slug>` | p0_public |
| `raid_group_id` | string | yes | `rgp_` | p1_pseudonymous |
| `group_size_class` | string | yes | `raid_small`/`raid_large` | p0_public |
| `n_eng` | int | yes | Engaged size | p0_public |
| `scaling` | object | yes | `s`, `hp_mult`, `H`, `adds`, `targets`, `cres` | p0_public |
| `seed` | int | yes | Attempt RNG seed | p1_pseudonymous |
| `tuning_version` | int | yes | Active tuning | p0_public |
| `resolve_pct` | number | yes | Frontier Resolve applied | p0_public |
| `composition` | object | yes | Count per `combat_role` | p0_public |
| `ready_check_passed_within_120s` | bool | yes | Analytics | p0_public |
| `pull_number` | int | yes | Attempts on this encounter by this `rpr_` owner this tier | p0_public |

**`encounter.phase.changed`** — `raid.phase_from`, `raid.phase_to` (string, p0), `raid.trigger` (`hp_below`, `time_elapsed`, `objective`, `enrage`; p0), `raid.boss_hp_frac` (number, p0), `raid.alive_count` (int, p0), `raid.elapsed_ms` (int, p0).

**`encounter.attempt.defeated`** / **`encounter.attempt.wiped`**

| Field (`raid.*`) | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `raid_id`, `difficulty`, `n_eng` | string, string, int | yes | Context | p0_public |
| `phase_reached` | string | yes | Last phase | p0_public |
| `min_boss_hp_frac` | number | yes | Best progress | p0_public |
| `duration_ms` | int | yes | Attempt length | p0_public |
| `enrage_reached` | string | yes | `none`, `soft`, `hard` | p0_public |
| `wipe_cause` | string | wiped only | `mechanic`, `enrage`, `attrition`, `leash`, `unknown` (top attributed cause) | p0_public |
| `top_failed_mechanics` | array<object> | wiped only | Up to 3 `{mechanic_kind, mechanic_id, count, lethal_count}` | p0_public |
| `eligible_count` | int | defeated only | Reward-eligible characters | p0_public |
| `ineligible` | array<object> | defeated only | `{character_id, reason}` | p1_pseudonymous |
| `validation_status` | string | defeated only | `pending`/`passed` at publish (always `passed` for kills that grant) | p0_public |
| `rpr_id` | string | yes | Credited progress record | p0_public |
| `cres_used` | int | yes | Combat resurrections used | p0_public |

**`encounter.mechanic.failed`** (batched; one event per attempt per second with up to 50 failures)

| Field (`raid.*`) | Type | Req | Description | privacy_class |
| --- | --- | --- | --- | --- |
| `failures` | array<object> | yes | `{instance_seq, mechanic_kind, mechanic_id, character_id?, phase, server_tick, lethal}` | p1_pseudonymous |
| `difficulty`, `n_eng` | string, int | yes | Context | p0_public |

**`instance.run.created`** — `raid.raid_id`, `raid.raid_group_id`, `raid.difficulty`, `raid.save_mode`, `raid.group_size_class`, `raid.week_key`, `raid.resolve_opt_out` (all p0 except group ID p1).

**`instance.lockout.created` / `.extended` / `.reset`** — `raid.lockout_kind` (p0), `raid.raid_id` (p0), `raid.encounter_id` (loot; p0), `raid.run_id` (progress; p1), `raid.extension_count` (p0), `raid.defeated_encounter_ids` (progress; p0).

**`group.ready_check.completed`** (group-service; Dungeon-owned event) — `raid.raid_group_id` (p1), `raid.all_ready` (p0), `raid.responses` (array `{character_id, response, subgroup, combat_role, in_instance, ilvl_equipped}`; p1), `raid.missing_capabilities` (p0).

---

## 4. Analytics hooks

Metric definitions are registered with the Analytics spec; dimension set unless noted: `raid_id`, `encounter_id`, `difficulty`, `group_size_class`, `n_eng` bucket, `dc_region`, `tier_week`, `tuning_version`.

| Metric | Definition | Source events | Use / alert |
| --- | --- | --- | --- |
| `m_raid_wipe_funnel` | Per encounter: share of attempts reaching each phase (phase_1 → … → defeated) | `encounter.attempt.started`, `encounter.phase.changed`, `encounter.attempt.defeated/wiped` | Identify the wall phase; tuning reviews |
| `m_raid_mechanic_failure_rate` | Failures of `mechanic_id` / mechanic instances spawned, split by `lethal` | `encounter.mechanic.failed`, attempt events | Flag mechanics with > 25% failure on normal after week 2 (unclear telegraph) |
| `m_raid_wipe_cause_share` | Share of wipes by `wipe_cause` | `encounter.attempt.wiped` | Enrage share > 40% on heroic ⇒ gear wall |
| `m_raid_attempts_to_first_kill` | Pulls before first kill per `rpr_` | `encounter.attempt.*` | Targets in Part 2, Participation targets |
| `m_raid_kill_rate_first_week` | Groups with a kill / groups with ≥ 10 pulls, first tier week | attempts | Nerf trigger (normal/veteran) |
| `m_raid_kill_duration_p50` | Median kill time vs `T_target` | `encounter.attempt.defeated` | Health tuning drift |
| `m_raid_weekly_participation` | Level-100 weekly actives with ≥ 1 raid kill | defeated + character activity | Engagement |
| `m_raid_flex_size_distribution` | Histogram of `n_eng` | attempt started | Size-class health |
| `m_raid_items_per_kill` | Items assigned per eligible character per kill (by `loot_mode`) | `loot.drop.assigned`, defeated | Loot parity between modes (target ≤ 5% gap) |
| `m_raid_tier_set_weeks` | Weeks from first raid kill to 4-pc / 5-pc | `item.instance.created` (set items), lockouts | Validates Part 2 targets |
| `m_raid_pity_trigger_rate` | `loot.pity.triggered` / eligible kills | loot events | Bad-luck health |
| `m_raid_council_override_rate` | Council finalizes with override / council assignments | `loot.drop.assigned` | Ninja-loot signal |
| `m_raid_lockout_extend_rate` | Progress lockouts extended / active at reset | `instance.lockout.extended` | Mythic progression health |
| `m_raid_resolve_uptake` | Kills with Frontier Resolve > 0 / all kills | attempt started | Catch-up effectiveness |
| `m_raid_world_influence_week` | `influence_points` per realm/link | `raid.world_effect.applied` | World impact balance |
| `m_raid_first_kill_invalidations` | Revoked first kills | `raid.first_kill.revoked` | Integrity |
| `m_raid_ready_check_pull_share` | Pulls with passing ready check in prior 120 s | attempt started | UX |

Wipe funnel query sketch (warehouse): per `encounter_id`, `difficulty`, count distinct `attempt_id` per `phase_to` from `encounter.phase.changed`, divided by attempts started; defeated attempts form the last step. Dashboards: "Raid Tier Health" (per boss funnel, attempts-to-kill, kill rate by week), "Raid Loot Parity", "Raid Integrity" (invalidations, override rates, detections).

---

## 5. Security considerations

### 5.1 Threats

| Threat | Description | Controls | Report to anticheat-service |
| --- | --- | --- | --- |
| Boss leash / pathing exploits | Pulling a boss out of its arena, onto unreachable geometry, or into evade loops to cheese mechanics | Arena volume + leash margin; evade on leave; attempt `invalidated`; boss pathing never leaves arena navmesh | `detector/raid_leash_exploit` (high) with attempt, positions |
| Mechanic bypass via geometry / LOS | Standing in unintended spots to avoid soaks/beams | Server-side ray casts and arena volumes; "safe spot" lint in content QA; per-encounter `no_safe_zone` volumes | `detector/raid_out_of_bounds` (medium) |
| Phase skip by burst / damage injection | Skipping phases via exploited damage or forged packets | `min_duration` clamps; damage from server-calculated abilities only; kill validation: `total_boss_damage` ≈ `boss_max_hp` ± 1%, no single participant > 3× median damage-role DPS for the ilvl | `detector/raid_damage_anomaly` (high) |
| Speed / teleport to dodge mechanics | Movement hacks | Security spec, speed envelope; `max_speed_ratio` > 1.15 in evidence ⇒ validation flag | Relayed from movement detector |
| Kill-credit farming / AFK leeching | AFK characters taking loot slots and lockouts | AFK rule (Part 2, Eligibility): no casts & no movement for last 60% ⇒ ineligible | `detector/raid_afk_eligibility` (info) |
| Loot ninja-ing | Leader changes loot mode or loot master right before a kill; master assigns to self or alts | 60 s settings cooldown; changes blocked while engaged; council quorum; recipient must be eligible; leader failover never transfers loot master; all changes in `group.raid_group.updated` and audit-service | Pattern: settings change < 120 s before kill + self-assignment ⇒ `detector/raid_loot_ninja` (medium); player reports attach `rgp_`, `lcs_` |
| Account sharing for progression | One skilled player logs in to other accounts to secure kills, realm firsts, titles | Session device/location changes during an active progress lockout; same device fingerprint driving > 1 account in one raid week; abrupt performance shift per account (mechanic failure rate z-score) | `detector/raid_account_sharing` (medium) to anticheat; Security spec decides sanctions (`progression_rollback`) |
| Carry-selling for gold | Selling kills/loot for gold. In-game gold carries are **permitted**; selling for real money (RMT) is not | Eligible-trade window limits loot transfer to kill participants; trades of gold to a raid member within 24 h before/after a run where that payer received council/master loot are tagged; high-volume patterns flagged | `security.trade.flagged` via economy-service; `detector/raid_carry_rmt_pattern` (low→medium) feeding risk score |
| Lockout manipulation | Joining/leaving at kill time to dodge lockouts or double-dip | Eligibility uses presence at engage AND kill; lockouts written atomically before rewards; idempotent grant keys | `security.action.rejected` on duplicate grant attempts |
| Forged instance-server reports | Compromised server or replayed RPC | mTLS service identity; instance-server may only report for runs it hosts (`srvi_` ↔ `run_` binding); idempotent attempt IDs | `security.integrity.failed` |
| Realm-first fraud | Exploit-assisted first kills | 15-min hold, automated validation, GM review queue for mythic `dc_region` firsts, revocation path | Revocations logged to audit-service |

### 5.2 Kill validation (normative)

Run by instance-service on `ReportRaidAttemptEnded` with outcome `defeated`, using `KillEvidence`:

1. `boss_max_distance_from_spawn_m` ≤ arena radius + leash margin.
2. `|total_boss_damage − boss_max_hp| / boss_max_hp ≤ 0.01`, where `boss_max_hp` must equal the scaled `H` stored in `raid_attempt_details.scaling` (accounting for healing absorbs recorded by the server).
3. `duration_ms ≥ Σ phase min_duration` and ≥ `tune.raid.min_kill_fraction` (0.40) × `T_target` × 1000.
4. Every participant `max_speed_ratio ≤ 1.15`.
5. No participant damage > `tune.raid.max_dps_ratio` (3.0) × `dps_ref(ilvl_equipped)` sustained over the attempt.
6. `anomaly_flags` empty.

Failure of 1, 2, or 6 ⇒ `validation_status = failed`, outcome `invalidated`, no lockouts/rewards, detection raised. Failure of 3–5 ⇒ kill counts but is flagged (`validation_flags`), detection raised, realm-first hold extended to manual review. Validation results never block players silently: the client shows "Kill under review" for held first kills.

### 5.3 Invalidation workflow

```mermaid
sequenceDiagram
  participant AC as anticheat-service / GM
  participant INS as instance-service
  participant RS as reward-service
  participant BUS as event-bus
  participant AUD as audit-service
  AC->>INS: InvalidateRaidAttempt(att_, reason, cas_)
  INS->>INS: outcome invalidated; delete loot lockouts; subtract rpr_; revoke first kill
  INS->>RS: RevokeGrant(raidkill:att_:chr_ …)
  RS-->>BUS: reward.grant.revoked
  INS-->>BUS: raid.first_kill.revoked, raid.world_effect.applied (reversal_of)
  INS->>AUD: append audit record
```

Item recovery from revoked grants follows Itemization spec and Security spec rollback rules (`item_rollback`); raid never deletes items itself.
