# Legends of the Shattered Frontier — Shared Registry
## Glossary, Entity Registry, Service Registry, Event Taxonomy, Shared Constants
### Version 1.0 — Technical Design Authority baseline (2026-10-17)

---

## 0. Purpose and authority

This registry is the **single source of truth for names** used across all design specifications. Every specification, service, schema, event, and agent-written repository MUST use the identifiers defined here exactly (case, separators, and spelling).

Rules:

1. **Names are binding; behavior is owned by the domain specification.** This file fixes *what things are called*, their ID formats, owners, and canonical lifecycle states. The owning specification (column "Owning spec") defines behavior, formulas, and full schemas.
2. **No silent additions.** A specification or repository that needs a new service, entity, ID prefix, event, enum value, or shared constant MUST list it in its "Registry additions" appendix. The registry maintainer merges approved additions here. Until merged, an addition is *proposed* and must not be consumed by another domain.
3. **Conflict resolution.** If a domain specification contradicts this registry on a name, this registry wins. If two domain specifications contradict each other on behavior, the owning specification for that concept (Section 9) wins.
4. **Design target, not implementation.** These documents describe intended designs. The current Node.js proof of concept (`server/`, `public/`) does not implement them; see `docs/dependency-map.md`.

Specification files (all under `docs/Game-Design/`):

| Short name | Entry file | Part folder |
| --- | --- | --- |
| Registry | `Legends_Shattered_Frontier_Shared_Registry_v1.0.md` | — |
| Character | `Legends_Shattered_Frontier_Character_Progression_Architecture_v1.0.md` | `Character-Progression/` |
| Itemization | `Legends_Shattered_Frontier_Itemization_Architecture_v1.0.md` | `Itemization/` |
| Quest | `Legends_Shattered_Frontier_Quest_Architecture_v1.0.md` | `Quests/` |
| Dungeon | `Legends_Shattered_Frontier_Dungeon_Architecture_v1.0.md` | `Dungeons/` |
| Raid | `Legends_Shattered_Frontier_Raid_Architecture_v1.0.md` | `Raids/` |
| Security | `Legends_Shattered_Frontier_Security_and_Anti_Cheat_Architecture_v1.0.md` | `Security/` |
| LiveOps | `Legends_Shattered_Frontier_LiveOps_Architecture_v1.0.md` | `LiveOps/` |
| Analytics | `Legends_Shattered_Frontier_Analytics_Architecture_v1.0.md` | `Analytics/` |
| Combat | `Legends_Shattered_Frontier_Combat_System_v0.1.md` (Version 1.0 expanded edition; path kept) | `Combat/` |
| Economy | `Legends_Shattered_Frontier_Economy_Architecture_v1.0.md` | `Economy/` |
| World | `Legends_Shattered_Frontier_World_Simulation_Architecture_v1.0.md` | `World-Simulation/` |
| AI | `Legends_Shattered_Frontier_AI_Architecture_v1.0.md` | `AI/` |
| Crafting | `Legends_Shattered_Frontier_Resource_and_Crafting_Architecture_v1.0.md` | `Crafting/` |
| Backend | `Legends_Shattered_Frontier_Backend_Architecture_v1.0.md` | `Backend/` |
| Network, Database, Building, Construction, Game Master, DevOps, Client delivery | existing `*_v1.0.md` / `*_v0.1.md` files | — |

---

## 1. Naming conventions

| Thing | Convention | Example |
| --- | --- | --- |
| Service name | kebab-case, ends in `-service` unless infrastructure or UE server | `progression-service`, `zone-server` |
| gRPC package | `lsf.<service_snake>.v<major>` | `lsf.progression_service.v1` |
| Database schema (PostgreSQL) | service name in snake_case without `_service` | `progression`, `inventory` |
| Table / column | snake_case, plural table names | `skill_values.skill_id` |
| Runtime ID | `<prefix>_<ULID>` (26-char Crockford base32 ULID) | `chr_01J9ZK3Q8M4V6T2N5X7B1C0D9E` |
| Content definition ID | `<kind>/<slug>`; slug is lowercase snake_case, immutable once published | `item/iron_longsword`, `skill/smithing` |
| Domain / telemetry event | `<domain>.<entity>.<verb_past>` — exactly three lowercase segments, snake_case within a segment | `quest.instance.completed` |
| Feature flag key | `ff.<domain>.<snake_name>` | `ff.loot.pity_enabled` |
| Tuning key | `tune.<domain>.<snake_name>` | `tune.progression.skill_gain_base_rate` |
| Metric (analytics) | `m_<snake_name>` | `m_d7_retention` |
| Enum values | lowercase snake_case | `ready_to_turn_in` |
| Timestamps | UTC, RFC 3339 with milliseconds in JSON; `timestamptz` in PostgreSQL | `2026-10-17T15:00:00.000Z` |
| Money | integer minor units (see §7) — never floating point | `amount_copper: 125000` |

IDs are opaque: services MUST NOT parse meaning from a ULID beyond its prefix and creation time.

---

## 2. Glossary

Terms are listed alphabetically. *Bold* terms are entity names (§3).

| Term | Definition |
| --- | --- |
| **Account** | A player's login identity. Owns characters, account-wide unlocks, premium currency, and sanctions. |
| Account-wide unlock | A progression reward recorded on the account rather than a character (titles, cosmetics, mounts, recipes flagged account-wide). Owned by Character spec. |
| **Achievement** | A one-time recorded accomplishment with criteria; may grant a reward bundle. |
| Affix | A named stat or effect modifier attached to an item instance. Kinds: `primary`, `secondary`, `utility`, `set`. Defined by an **Affix Definition**. |
| Attempt | See **Encounter Attempt**. |
| Attribute | One of the eight primary stats: `str`, `dex`, `con`, `int`, `wis`, `wil`, `cha`, `luck`. Formulas owned by Combat spec; growth owned by Character spec. |
| Band | See Progression Band and Skill Band. |
| Binding | Item ownership restriction: `none`, `on_equip`, `on_pickup`, `account`. |
| Biome | Large world area type. Launch biomes: `greenlands`, `frozen_north`, `desert_wastes`, `corrupted_frontier`. |
| **Campaign** | A server-wide, multi-stage quest objective (live quest event) with communal progress. Owned by Quest spec, scheduled by LiveOps. |
| Case | See **Enforcement Case**. |
| Cell | Smallest world simulation partition inside a zone (World spec). |
| **Character** | A playable avatar belonging to an account on one realm. |
| Character Level | Integer 1–100 (`LEVEL_CAP`), derived from lifetime XP. Used by combat formulas (Combat spec). |
| Combat Role | Self-declared group role: `tank`, `healer`, `damage`, `support`. Classless; declared for matchmaking only. |
| Content Pack | A versioned, signed bundle of content definitions published by `content-catalog-service`. Identified by `content_version` (monotonic integer). |
| Content Definition | Static authored data (item, skill, quest, dungeon, raid, NPC, loot table, reward bundle, affix, achievement, event). Identified by a content definition ID. |
| Datacenter Region | Deployment geography: `na`, `eu`, `apac`, `sa`. Not to be confused with world Region. |
| Detection | A single anti-cheat finding produced by a detector. Owned by Security spec. |
| Difficulty | Instance difficulty: `story`, `normal`, `veteran`, `heroic`, `mythic` (see §6). |
| Domain Event | A durable fact published on the event bus by the owning service; consumed by other services and by analytics. |
| Dungeon | Instanced or open-world cooperative content for 1–10 players (Dungeon spec). |
| Encounter | An authored combat challenge inside a dungeon or raid, usually a boss. Defined by an **Encounter Definition**. |
| **Encounter Attempt** | One pull of an encounter from engage to defeat or wipe. |
| **Enforcement Case** | A record grouping detections, reports, and evidence for a subject, leading to sanctions or dismissal. |
| Escrow | Temporary hold on items/currency during trade, market listing, or mail; item status `escrowed`. |
| Extended Party | A group of 6–10 players for 5–10-player dungeon variants. |
| Feature Flag | Runtime on/off or percentage switch from `config-service`, key `ff.*`. |
| Frontier Marks | Weekly-capped progression currency from dungeons, quests, and world events (`currency/frontier_marks`). |
| Item Definition | Static template for an item (`item/<slug>`). |
| **Item Instance** | A concrete owned item (or stack) with an `itm_` ID, rolled affixes, durability, binding, and provenance. |
| Item Level (ilvl) | Integer power rating of an item instance; drives stat budget. See §8 power ladder. |
| Kill switch | A feature flag that disables a feature or content globally within seconds (LiveOps). |
| Learning Points (LP) | Currency granted by XP/levels and spent on skill training, mastery nodes, and specialization (Character spec). |
| Legendary Item | A rarity-`legendary` item obtained through a multi-stage acquisition path (Itemization spec). |
| LiveOps Event | A scheduled, triggered, or dynamic operated event (bonus weekend, holiday, invasion series). Owned by LiveOps spec. Distinct from World Event. |
| **Lockout** | A per-character record limiting rewards from an instance difficulty until the next reset. |
| Loot Table | Weighted, nested drop definition (`loot/<slug>`) rolled by `loot-service`. |
| Mastery Tree | Per-skill tree of passive/active nodes unlocked with LP at skill bands (Character spec). |
| Mythic Level | Scaling level 1–20 (`MYTHIC_LEVEL_MAX`) for mythic dungeons. |
| Party | Group of up to 5 players (`PARTY_MAX`). |
| Pity | Bad-luck protection counter that raises drop chance after unsuccessful rolls (Itemization spec). |
| Prestige Rank | Post-cap repeatable progression rank earned by XP after level 100 (Character spec). |
| Progression Band | Level bracket of 10 levels: band 1 = levels 1–10 … band 10 = levels 91–100. |
| Progression Track | Skill category: `combat`, `crafting`, `gathering`, `social`, `leadership`, `exploration`. |
| Raid | Large-scale instanced endgame content for 10 or 20 players (Raid spec). |
| **Raid Group** | A group of up to 25 players formed for a raid (subgroups of 5). |
| Rarity | `common`, `uncommon`, `rare`, `epic`, `legendary`, `mythic`. |
| Realm | One persistent world copy within a datacenter region, e.g. `na-1`. All character state is realm-scoped. |
| Region (world) | A geographic subdivision of a biome containing zones (World spec). |
| Reset | Daily and weekly boundaries per datacenter region (§7.3). |
| **Reward Bundle** | Content definition (`reward/<slug>`) describing a set of rewards (items, loot table rolls, currency, XP, reputation, LP, unlocks). |
| **Reward Grant** | A runtime, idempotent issuance of a reward bundle to a character. |
| Risk Score | Rolling 0–1000 anti-cheat score per account (Security spec). |
| Run | See **Instance Run**. |
| **Instance Run** | One provisioned copy of a dungeon or raid for a group, from creation to close. |
| Sanction | An enforcement action against an account/character (§6). |
| **Season** | A time-boxed (≈12-week) content and reward period with its own track and rotations. |
| Server authority | Rule that the server validates and decides every gameplay-relevant outcome; the client only requests. |
| Skill | One of the canonical skills in §5, value 0–1000 (`SKILL_MAX`). |
| Skill Band | `novice` 0–99, `apprentice` 100–299, `journeyman` 300–499, `expert` 500–699, `master` 700–899, `legendary` 900–1000. |
| Skill Lock | Per-skill direction: `up`, `down`, `locked` (Ultima Online style). |
| Specialization | A chosen focus that raises a skill's effective cap and unlocks a mastery tree tier (Character spec). |
| Stat Budget | The total stat value an item may carry, from ilvl, slot, and rarity (Itemization spec). |
| Telemetry Event | An event emitted only for analytics (client or server), not used for service-to-service state. Uses the same envelope and naming. |
| Tuning Table | A versioned table of balance values published via `config-service` (`tune.*`). |
| World Event | A simulation event directed by `ai-director-service` (invasion, world boss, caravan attack). Distinct from LiveOps Event. |
| XP | Experience points; lifetime XP determines Character Level and grants LP. |
| Zone | A unit of world simulation run by one `zone-server` process (World spec). |

---

## 3. Entity registry

Runtime entities have an ID prefix. Content definitions use `<kind>/<slug>`.

### 3.1 Runtime entities

| Entity | ID prefix | Owning service | Owning spec | Store |
| --- | --- | --- | --- | --- |
| Account | `acc_` | auth-service | Backend | PostgreSQL `auth` |
| Session | `ses_` | auth-service | Backend | Redis + PostgreSQL `auth` |
| ElevationGrant (just-in-time staff elevation) | `elv_` | auth-service (staff access tables) | Security | PostgreSQL `auth` |
| Character | `chr_` | character-service | Backend / Character | PostgreSQL `character` |
| CharacterProgression (keyed by `character_id`: level, XP, LP, stage, prestige) | — | progression-service | Character | PostgreSQL `progression` |
| AttributeValue (keyed by `character_id`+`attribute`) | — | progression-service | Character | PostgreSQL `progression` |
| SpecializationSelection (keyed by `character_id`+`slot`) | — | progression-service | Character | PostgreSQL `progression` |
| AchievementProgress (keyed by `character_id`+`achievement_id`) | — | progression-service | Character | PostgreSQL `progression` |
| SkillValue (row, keyed by `character_id`+`skill_id`) | — | progression-service | Character | PostgreSQL `progression` |
| ProgressionLedgerEntry (XP/LP/skill changes) | `pgl_` | progression-service | Character | PostgreSQL `progression` |
| MasteryNodeUnlock | — | progression-service | Character | PostgreSQL `progression` |
| AchievementRecord | — | progression-service | Character | PostgreSQL `progression` |
| AccountUnlock | — | progression-service | Character | PostgreSQL `progression` |
| ItemInstance | `itm_` | inventory-service | Itemization | PostgreSQL `inventory` |
| Container | `ctn_` | inventory-service | Itemization | PostgreSQL `inventory` |
| ItemLedgerEntry (every ownership/state change) | `ilg_` | inventory-service | Itemization | PostgreSQL `inventory` (append-only) |
| LegendaryProgress | `lgp_` | inventory-service | Itemization | PostgreSQL `inventory` |
| LootRoll | `lrl_` | loot-service | Itemization | PostgreSQL `loot` |
| PityCounter (keyed by `character_id`+`pity_key`) | — | loot-service | Itemization | PostgreSQL `loot` |
| LootCouncilSession | `lcs_` | loot-service | Itemization (raid usage: Raid) | PostgreSQL `loot` |
| RewardGrant | `rwg_` | reward-service | Itemization | PostgreSQL `reward` |
| Wallet (keyed by owner+currency) | — | economy-service | Economy | PostgreSQL `economy` |
| CurrencyTransaction | `txn_` | economy-service | Economy | PostgreSQL `economy` (append-only) |
| MarketListing | `lst_` | economy-service | Economy | PostgreSQL `economy` |
| Trade | `trd_` | economy-service | Economy | PostgreSQL `economy` |
| QuestInstance | `qst_` | quest-service | Quest | PostgreSQL `quest` |
| GeneratedQuest (runtime-generated definition) | `gqd_` | quest-service | Quest | PostgreSQL `quest` |
| Campaign (runtime) | `cmp_` | quest-service | Quest | PostgreSQL `quest` |
| Party | `pty_` | group-service | Dungeon | Redis + PostgreSQL `group` |
| RaidGroup | `rgp_` | group-service | Raid | Redis + PostgreSQL `group` |
| QueueTicket | `qtk_` | group-service | Dungeon | Redis |
| DeserterRecord (keyed by `character_id`+`run_id`) | — | group-service | Dungeon | PostgreSQL `group` |
| MythicProgress (keyed by `character_id`+`season_id`+`content_id`) | — | instance-service | Dungeon | PostgreSQL `instance` |
| AffixRotation (keyed by `season_id`+`week_index`) | — | instance-service | Dungeon | PostgreSQL `instance` |
| RunCheckpoint (keyed by `run_id`+`checkpoint_seq`) | — | instance-service | Dungeon | PostgreSQL `instance` |
| InstanceRun | `run_` | instance-service | Dungeon (raid extensions in Raid) | PostgreSQL `instance` |
| EncounterAttempt | `att_` | instance-service (emitted by instance-server) | Dungeon / Raid | PostgreSQL `instance` |
| Lockout | `lck_` | instance-service | Dungeon / Raid | PostgreSQL `instance` |
| RaidProgress (guild/group progression record) | `rpr_` | instance-service | Raid | PostgreSQL `instance` |
| Guild | `gld_` | guild-service | Backend / Building | PostgreSQL `guild` |
| Claim | `clm_` | building-service | Building | PostgreSQL `building` |
| Structure (connected structural assembly or functional object) | `strc_` | building-service | Construction / Building | PostgreSQL `building` |
| Siege | `sge_` | building-service | Building | PostgreSQL `building` |
| ConstructionAction (durable coordinator receipt) | `cba_` | building-service | Construction Integration | PostgreSQL `building` |
| WorldEventInstance | `wev_` | ai-director-service | AI / World | PostgreSQL `director` |
| FactionStanding (keyed by `character_id`+`faction_id`) | — | faction-service | AI | PostgreSQL `faction` |
| FactionControl (keyed by `world_region_id`) | — | faction-service | AI | PostgreSQL `faction` |
| Detection | `det_` | anticheat-service | Security | PostgreSQL `anticheat` |
| IntegrityAttestation | `iat_` | anticheat-service | Security | PostgreSQL `anticheat` |
| EvidenceBundle | `evd_` | enforcement-service | Security | PostgreSQL `enforcement` + object storage |
| RollbackJob | `rbk_` | enforcement-service | Security | PostgreSQL `enforcement` |
| PlayerReport | `rpt_` | enforcement-service | Security | PostgreSQL `enforcement` |
| RiskScore (keyed by `account_id`) | — | anticheat-service | Security | Redis + PostgreSQL `anticheat` |
| EnforcementCase | `cas_` | enforcement-service | Security | PostgreSQL `enforcement` |
| Sanction | `snc_` | enforcement-service | Security | PostgreSQL `enforcement` |
| Appeal | `apl_` | enforcement-service | Security | PostgreSQL `enforcement` |
| AuditRecord | `aud_` | audit-service | Security | append-only store (PostgreSQL `audit` + object-storage WORM) |
| LiveOpsEvent (runtime schedule entry) | `lop_` | liveops-service | LiveOps | PostgreSQL `liveops` |
| Season (runtime) | `ssn_` | liveops-service | LiveOps | PostgreSQL `liveops` |
| Release | `rel_` | liveops-service | LiveOps | PostgreSQL `liveops` |
| MaintenanceWindow | `mnt_` | liveops-service | LiveOps | PostgreSQL `liveops` |
| Announcement | `ann_` | liveops-service | LiveOps | PostgreSQL `liveops` |
| Survey | `srv_` | liveops-service | LiveOps | PostgreSQL `liveops` |
| CompensationGrant | `cmpg_` | liveops-service (issues via reward-service) | LiveOps | PostgreSQL `liveops` |
| FlagChange / TuningVersion | `cfg_` | config-service | LiveOps | PostgreSQL `config` |
| StorePurchase | `pur_` | store-service | LiveOps | PostgreSQL `store` |
| TelemetryEvent | `evt_` | telemetry-ingest | Analytics | Kafka-compatible log → lakehouse |
| AlertInstance | `alr_` | analytics-pipeline | Analytics | PostgreSQL `analytics_pipeline` |
| Experiment | `exp_` | analytics-pipeline (assignment via config-service rules) | Analytics | PostgreSQL `analytics_pipeline` |
| ServerInstance (zone or instance server process) | `srvi_` | world-service | Backend / World | Redis |
| WorldEntity (runtime NPC/actor) | `ent_` | zone-server / instance-server | World / AI | memory only |
| PrivacyRequest (export / delete) | `prq_` | auth-service | Backend | PostgreSQL `auth` |
| WalletLien | `wln_` | economy-service | Economy | PostgreSQL `economy` |
| TransferHold | `thd_` | economy-service | Economy | PostgreSQL `economy` |
| FactionStandingLedgerEntry | `fsl_` | faction-service | AI | PostgreSQL `faction` (append-only) |
| CraftingAttempt | `crf_` | crafting-service | Crafting | PostgreSQL `crafting` |
| KnownRecipe (keyed by `character_id`+`recipe_id`) | — | crafting-service | Crafting | PostgreSQL `crafting` |
| HarvestRecord | `hrv_` | crafting-service (issued by zone-server) | Crafting | PostgreSQL `crafting` |
| Handoff (zone-to-zone transfer) | `hof_` | world-service | World | Redis |
| TeleportToken | `tpt_` | zone-server / instance-server | World | memory only |
| WorldEventRequest | `wer_` | ai-director-service | AI | PostgreSQL `director` |
| DirectorBiasWindow | `dbw_` | ai-director-service | AI | PostgreSQL `director` |
| GeneratedQuestRequest | `gqr_` | ai-director-service | AI | PostgreSQL `director` |
| NpcMemory (keyed by `realm_id`+`npc_id`+`character_id`) | — | ai-director-service | AI | PostgreSQL `director` (privacy participant) |
| TerrainEdit | `ted_` | world-service | World | PostgreSQL `world` |
| WorldAction (bounded staff/event world change) | `wac_` | world-service | World | PostgreSQL `world` |
| Realm (keyed by `realm_id`) | — | world-service (realm list from config-service) | World | PostgreSQL `world` |
| SpawnSlotState (keyed by `realm_id`+`zone_id`+`layer_index`+`group_id`+`slot_index`) | — | world-service | World | PostgreSQL `world` |
| RegionResourceState (keyed by `realm_id`+`world_region_id`+`resource_id`) | — | world-service | World | PostgreSQL `world` |
| PoiDiscovery (keyed by `character_id`+`poi_id`) | — | world-service | World | PostgreSQL `world` |
| WeatherState (keyed by `realm_id`+`world_region_id`) | — | world-service | World | PostgreSQL `world` |
| WorldBossSchedule (keyed by `realm_id`+`world_event_id`) | — | world-service | World | PostgreSQL `world` |
| RealmTransfer | `rtx_` | character-service | Backend | PostgreSQL `character` |
| MailMessage | `mal_` | chat-service | Backend | PostgreSQL `chat` |
| LegalHold | `hld_` | enforcement-service | Security (contract: Backend spec, Legal holds) | PostgreSQL `enforcement` |

### 3.2 Content definition kinds

All published by `content-catalog-service` inside versioned content packs.

| Kind | Example ID | Owning spec |
| --- | --- | --- |
| `skill` | `skill/smithing` | Character |
| `mastery` (tree) | `mastery/smithing` | Character |
| `specialization` | `specialization/weaponsmith` | Character |
| `achievement` | `achievement/first_legendary_skill` | Character |
| `title` | `title/the_unbroken` | Character |
| `item` | `item/iron_longsword` | Itemization |
| `affix` | `affix/of_the_bear` | Itemization |
| `item_set` | `item_set/frostforged_regalia` | Itemization |
| `legendary_path` | `legendary_path/ashbringer_reforged` | Itemization |
| `loot` (table) | `loot/sunken_vault_final_boss` | Itemization (tables), Dungeon/Raid (assignment) |
| `reward` (bundle) | `reward/frontier_founder_completion` | Itemization (schema), authoring domain (content) |
| `currency` | `currency/frontier_marks` | Economy |
| `recipe` | `recipe/iron_longsword` | Crafting |
| `resource` | `resource/iron_ore` | Crafting |
| `quest` | `quest/frontier_founder` | Quest |
| `quest_chain` | `quest_chain/founding_of_greenhold` | Quest |
| `quest_template` (for generation) | `quest_template/caravan_escort` | Quest |
| `campaign` | `campaign/shattering_anniversary` | Quest |
| `dungeon` | `dungeon/sunken_vault` | Dungeon |
| `raid` | `raid/ashen_citadel` | Raid |
| `encounter` | `encounter/ashen_citadel_warden` | Dungeon / Raid |
| `dungeon_affix` | `dungeon_affix/volcanic` | Dungeon |
| `pack` (trash pack template) | `pack/sv_tide_casters` | Dungeon |
| `objective` (randomized dungeon objective) | `objective/sv_free_prisoners` | Dungeon |
| `dungeon_event` | `dungeon_event/sv_ambush` | Dungeon |
| `structure` | `structure/stone_wall` | Building |
| `npc` | `npc/ash_wolf` | AI / Combat |
| `faction` | `faction/greenhold_wardens` | AI |
| `zone` | `zone/greenlands_01` | World |
| `world_region` | `world_region/greenlands_south` | World |
| `continent` | `continent/shattered_mainland` | World |
| `gathering_node` | `gathering_node/iron_vein` | Crafting |
| `salvage` (salvage profile) | `salvage/metal_weapon` | Crafting |
| `world_event` | `world_event/dragon_incursion` | AI / World |
| `liveops_event` | `liveops_event/harvest_festival` | LiveOps |
| `season` | `season/s1_shattered_dawn` | LiveOps |
| `store_offer` | `store_offer/frost_wolf_mount` | LiveOps |
| `ability` | `ability/shield_slam` | Combat (encounter-local abilities compile into it) |
| `effect` | `effect/ashlands_respite` | Combat (buffs, debuffs, procs, set bonuses, regional buffs) |
| `vendor` | `vendor/greenhold_smith` | Economy |
| `behavior_tree` | `behavior_tree/caster_kite` | AI |
| `utility_profile` | `utility_profile/caster` | AI |
| `schedule` (NPC daily routine) | `schedule/greenhold_baker` | AI |
| `spawnset` | `spawnset/greenlands_south_wolves` | AI (authoring) / World (execution) |
| `mount` | `mount/frost_wolf` | World (ownership via `account_unlock_kind = mount`) |
| `poi` | `poi/greenhold_watchtower` | World |
| `route` | `route/south_road_3` | World |
| `detector` | `detector/speed_envelope` | Security (published only in server-only content packs, never shipped to clients) |

Example slugs used in specifications are **illustrative** unless they appear in a canonical list in this registry (biomes §2, skills §5, currencies §7). Only `quest/frontier_founder` is canonical (implemented in the PoC).

### 3.3 Core entity relationships

```mermaid
erDiagram
    ACCOUNT ||--o{ CHARACTER : owns
    ACCOUNT ||--o{ SANCTION : receives
    ACCOUNT ||--|| RISK_SCORE : has
    ACCOUNT ||--o{ ACCOUNT_UNLOCK : earns
    CHARACTER ||--o{ SKILL_VALUE : has
    CHARACTER ||--o{ CONTAINER : owns
    CONTAINER ||--o{ ITEM_INSTANCE : holds
    ITEM_INSTANCE }o--|| ITEM_DEFINITION : instantiates
    ITEM_INSTANCE ||--o{ ITEM_LEDGER_ENTRY : history
    CHARACTER ||--o{ WALLET : owns
    CHARACTER ||--o{ QUEST_INSTANCE : tracks
    QUEST_INSTANCE }o--|| QUEST_DEFINITION : instantiates
    CHARACTER ||--o{ LOCKOUT : bound_by
    LOCKOUT }o--o| INSTANCE_RUN : created_by
    INSTANCE_RUN |o--o{ ENCOUNTER_ATTEMPT : contains
    INSTANCE_RUN }o--o| PARTY : hosts
    INSTANCE_RUN }o--o| RAID_GROUP : hosts
    REWARD_GRANT }o--|| REWARD_BUNDLE : issues
    REWARD_GRANT }o--|| CHARACTER : to
    REWARD_GRANT ||--o{ LOOT_ROLL : may_roll
    LOOT_ROLL }o--|| LOOT_TABLE : rolls
    DETECTION }o--|| ACCOUNT : about
    ENFORCEMENT_CASE ||--o{ DETECTION : groups
    ENFORCEMENT_CASE ||--o{ SANCTION : results_in
    CHARACTER ||--o{ FACTION_STANDING : holds
```

Open-world dungeons have no run: encounter attempts and cooldown lockouts there have a null `run_id` and reference the attempt instead (Dungeon spec, Lockout framework).

---

## 4. Service registry

All internal APIs are gRPC (`lsf.<service_snake>.v1`); externally reachable HTTP/WebSocket endpoints go through `gateway`. Real-time gameplay traffic goes client ↔ `zone-server` / `instance-server` over UDP (Network spec). Every mutating RPC takes an `idempotency_key` (string ≤ 128 chars); services persist keys for at least 7 days and return the original result on replay.

| Service | Kind | Owning spec | Owns (authoritative data) | Publishes domain(s) |
| --- | --- | --- | --- | --- |
| `gateway` | edge API | Backend / Network | routing, rate limits, token validation | — |
| `auth-service` | stateless + DB | Backend (account core) / Security (credentials, MFA, session binding, login risk, staff elevation) | accounts, sessions, MFA, login risk, staff elevation grants | `auth` |
| `character-service` | stateless + DB | Backend | character profile, appearance, location snapshot | `character` |
| `progression-service` | stateless + DB | Character | XP, level, LP, attributes, skills, locks, mastery, specializations, prestige, achievements, titles, account unlocks; Stable, owned creature progression/training/entitlements and deployment bindings (companion extension §12) | `progression` |
| `inventory-service` | stateless + DB | Itemization | item instances, containers, equipment, durability, binding, item ledger, legendary progress | `item`, `inventory` |
| `loot-service` | stateless + DB | Itemization | loot rolls, pity counters, group loot distribution | `loot` |
| `reward-service` | stateless + DB | Itemization | reward grants (idempotent fan-out to inventory/economy/progression/faction) | `reward` |
| `crafting-service` | stateless + DB | Crafting | recipes execution, crafting quality, salvage outputs | `crafting`, `gathering` |
| `economy-service` | stateless + DB | Economy | wallets, currency ledger, markets, trades, taxes, price indexes | `economy`, `market` |
| `quest-service` | stateless + DB | Quest | quest instances, generated quests, chains, choices, campaigns | `quest` |
| `group-service` | stateful (Redis) + DB | Dungeon (raid parts: Raid) | parties, raid groups, matchmaking queues, ready checks | `group` |
| `instance-service` | stateless + DB | Dungeon (raid parts: Raid) | instance runs, lockouts, affix rotations, raid progress, server allocation requests | `instance`, `encounter`, `raid` |
| `guild-service` | stateless + DB | Backend / Building | guilds, ranks, permissions, settlements link | `guild` |
| `building-service` | stateless + DB | Building / Construction | claims, structures, permissions, budgets, upkeep, sieges, archive manifests and construction commit decisions; package `lsf.building_service.v1` | `world` (`world.structure.*`), `building` |
| `chat-service` | stateful | Backend | channels, messages, moderation hooks | `chat` |
| `world-service` | stateful + DB (PostgreSQL `world`) | World | zone allocation, server registry, handoffs, world clock, realms runtime state, terrain edits, spawn slot state, regional resource state, POI discoveries, weather, world actions, world-boss schedules | `world` |
| `zone-server` | UE5.8 dedicated server | World / Network | live simulation of a zone; emits combat/gathering events | `combat` (`combat.npc.killed` durable on `domain.combat`; other `combat.*` telemetry), `gathering`, `world` |
| `instance-server` | UE5.8 dedicated server (instance mode of the zone-server build) | Dungeon / Raid | live simulation of one instance run; encounter runtime | `encounter`, `combat` |
| `content-catalog-service` | stateless + object storage | Backend | published content packs and definitions (includes NPC definitions); supports client-visible and server-only packs | `content` |
| `chat-service` mail | (part of `chat-service`) | Backend | mail messages (`mal_`); attachments held in inventory/economy escrow | `chat` |
| `ai-director-service` | stateful + DB (PostgreSQL `director`) | AI | world event instances and requests, director bias windows, spawn pressure, invasions, generated-quest requests, NPC memory | `world` |
| `faction-service` | stateless + DB | AI | faction standings (reputation), faction control of world regions | `faction` |
| `anticheat-service` | stream + DB | Security | detectors, detections, risk scores, integrity attestations | `security` |
| `enforcement-service` | stateless + DB | Security | cases, sanctions, appeals, rollbacks orchestration | `enforcement` |
| `audit-service` | append-only | Security | immutable audit trail of privileged and economic actions | `audit` |
| `liveops-service` | stateless + DB | LiveOps | event calendar, seasons, campaigns schedule, announcements, surveys, compensation | `liveops` |
| `config-service` | stateless + DB + push | LiveOps | feature flags, tuning tables, kill switches, reset schedules | `config` |
| `store-service` | stateless + DB | LiveOps | premium store catalog, offers, purchases (cosmetic/convenience only) | `store` |
| `telemetry-ingest` | edge collector | Analytics | client/server telemetry intake, validation, PII scrubbing | — |
| `analytics-pipeline` | stream/batch jobs | Analytics | enrichment, sessionization, aggregates, alert evaluation | `ops` (alerts) |
| `analytics-warehouse` | data platform | Analytics | lakehouse (Apache Iceberg on object storage) + ClickHouse serving layer | — |
| `event-bus` | infrastructure | Backend / Analytics | Kafka-compatible durable log carrying domain and telemetry events | — |
| `admin-portal` | web UI | LiveOps (GM tools) / Security (enforcement console) | none — calls service admin APIs; every action audited | — |

Authority rules (apply to every spec):

1. **Simulation authority:** `zone-server`/`instance-server` decide real-time outcomes (movement, hits, kills, node harvests, encounter phases).
2. **Durable authority:** the owning service is the only writer of its data. Servers request durable changes via RPC (e.g. instance-server → `reward-service`), never by writing another service's database.
3. **Item and currency creation** happens only in `inventory-service` (items) and `economy-service` (currency), and every creation carries a `source` code (§7.2) and a `causation_id`.
4. **Rewards** from quests, dungeons, raids, world events, achievements, LiveOps compensation, and seasons all flow through `reward-service`.

Cross-service RPC names fixed for consumers (owning specs define messages; specs marked *proposed* must confirm the contract when expanded):

| Service | RPCs | Defined in |
| --- | --- | --- |
| `reward-service` | `IssueGrant`, `RevokeGrant` | Itemization |
| `inventory-service` | `GrantItems`, `QuarantineItems`, `ReleaseQuarantine`, `RevokeItems` (`mode = destroy \| quarantine`), `ConvertTierItem`, `CreateCraftedItem`, `CreateGatheredItems`, `CreateVendorItems` (economy-service only; source `vendor_stock`), `MoveToBuyback`, `RestoreFromBuyback`, `RepairItems` (incl. kit mode, caller crafting-service), `RestoreMaxDurability`, `RerollAffix` | Itemization (vendor RPCs requested by Economy spec, NPC vendor pricing; repair/reroll RPCs requested by Crafting spec) |
| `progression-service` | `ApplyRollback` (scope by `pgl_` entries or `reward_grant_id`) | Character |
| `faction-service` | `GetStanding`, `AdjustStanding`, `AddControlInfluence`, `GetControl` | AI spec, Faction service (ratified 2026-10-17) |
| `world-service` | `AllocateServer`, `ReleaseServer`, `GetServer`, `RegisterServer`, `Heartbeat`, `RequestHandoff`, `CommitHandoff`, `AbortHandoff`, `ResolveZone`, `GetWorldClock`, `AdminMoveCharacter`, `DrainRealms`, `GetRealms`, `SetRealmRole`, `MigrateZone`, `LoadZoneState`, `CommitTerrainEdits`, `ReportZoneState`, `RecordDiscovery`, `GetRegionResourceState`, `GetWeather`, `GetWorldBossSchedule`, `DispatchZoneCommand`, `ApplyWorldAction`, `CancelWorldAction`, `RevertTerrainEdits` | World spec, world-service API and server allocation |
| `ai-director-service` | `RequestWorldEvent`, `CancelWorldEventRequests`, `SetDirectorBias`, `SubmitQuestOutcome`, `GetActiveWorldEvents`, `GetNarrativeFlags`, `ZoneLink` (bidirectional stream with zone-servers: `RegionSample`, `EventProgress`, NPC memory events ↔ directives, event commands), `GetNpcMemories`, `AdminStartWorldEvent`, `AdminAdvanceWorldEvent`, `AdminEndWorldEvent`, `AdminGetRegionState` | AI spec, World director and dynamic events part, gRPC contract |
| `gateway` (`lsf.gateway.v1`) | `SetLoginGate`, `ClearLoginGate`, `SetMinClientBuild`, `PushClientNotice` | Backend spec, Identity, gateway and characters part |
| `character-service` | `CreateCharacter`, `ListCharacters`, `GetCharacter`, `RenameCharacter`, `DeleteCharacter`, `RestoreCharacter`, `EnterWorld`, `ReportPresence`, `CheckpointCharacter`, `GetLocation`, `ListActiveCharacters`, `RequestRealmTransfer`, `GetRealmTransfer`, `CancelRealmTransfer`, `CharacterAdmin.ForceRename`, `CharacterAdmin.GetCharacterHistory` | Backend spec, Identity, gateway and characters part |
| `content-catalog-service` | `PublishPack`, `ValidatePack`, `ActivateContentVersion`, `GetActiveVersion`, `GetPack`, `GetDefinitions`, `CheckRollbackSafety`, `ContentAdmin.ListPacks`, `ContentAdmin.DiffVersions` | Backend spec, Service communication, persistence and content part |
| `economy-service` | `Credit`, `Debit`, `GetWallet`, `TransferCurrency`, `GetIndexPrices`, `PlaceLien`, `ReleaseLien`, `WriteOffLien`, `ApplyRestriction`, `LiftRestriction`, `ExportWalletSnapshot`, `QuoteNpcPrices`, `BuyFromVendor`, `SellToVendor`, `BuybackFromVendor` | Economy spec, gRPC API |
| `crafting-service` | `CraftItem`, `PreviewCraft`, `ResolveHarvest`, `ComputeSalvage`, `LearnRecipe`, `RevokeRecipe`, `ListKnownRecipes`, `RepairWithKit`, `ReforgeItem`, `RerollItemAffix`, `GetCraftLineage` | Crafting spec, Attempt flow and success |
| `building-service` | `EvaluateConstructionPermission`, `PreviewConstruction`, `CommitConstruction`, `GetConstructionAction`, `ReserveClaim`, `ChargeClaimMaintenance`, `ArchiveAndReleaseClaim`, `ResolveSiege`, `LoadClaimStructures` | Construction Integration §8; designed contracts, implementation pending |
| `inventory-service` construction extension | `PrepareConstructionMaterials`, `ApplyConstructionDecision`, `CompleteConstructionMaterials`, `GetConstructionParticipation` | Construction Integration §5; Inventory remains sole writer; designed contracts, implementation pending |
| `auth-service` | `SubmitPrivacyRequest`, `ConfirmPrivacyRequest`, `CancelPrivacyRequest`, `GetPrivacyRequest`, `AuthAdmin.OpenPrivacyRequest`, `CreateAccount`, `GetAccount`, `ResolveHomeRegion`, `ListSessionsOverlapping`, `EndSession`, `HoldReleased` (legal-hold callback) | Backend spec, Account deletion and privacy request workflow |
| every data-owning service | `PrivacyParticipant.ExportSubject`, `PrivacyParticipant.EraseSubject` | Backend spec, Account deletion and privacy request workflow |
| every data-owning service holding character data | `TransferParticipant.PrepareTransfer`, `StageTransfer`, `CommitTransfer`, `AbortTransfer` | Backend spec, Realm and character transfer |
| every service referencing content IDs | `ContentReferenceParticipant.CountReferences` | Backend spec, `content-catalog-service` |
| `enforcement-service` | `GetLegalHold` (returns `hld_` holds), `HoldReleased` callback to auth-service | Backend spec, Legal holds (ratified by Security spec, Audit operations and interfaces, 2026-10-17) |

Lockout uniqueness includes `scope_key` (`'instance'` default; encounter ID or `progress` for raids) — Dungeon spec, Lockout framework.

---

## 5. Canonical skills and progression tracks

Skill IDs are `skill/<slug>`. Range 0–1000. Track caps and specialization rules are in the Character spec.

| Track | Skills |
| --- | --- |
| `combat` (12) | `swordsmanship`, `axes`, `maces`, `polearms`, `daggers`, `archery`, `unarmed`, `shield_defense`, `tactics`, `magic`, `healing`, `resist_magic` |
| `crafting` (8) | `smithing`, `carpentry`, `tailoring`, `alchemy`, `engineering`, `cooking`, `jewelcrafting`, `inscription` |
| `gathering` (6) | `mining`, `logging`, `farming`, `herbalism`, `fishing`, `hunting` |
| `social` (3) | `trading`, `diplomacy`, `performance` |
| `leadership` (2) | `leadership`, `governance` |
| `exploration` (4) | `cartography`, `survival`, `stealth`, `archaeology` |

Workshop ↔ skill mapping (Crafting spec): Forge → smithing, jewelcrafting; Sawmill → carpentry; Alchemy Lab → alchemy, cooking; Tailor Shop → tailoring, inscription; Engineering Workshop → engineering.

---

## 6. Shared enumerations and canonical lifecycle states

Owning specs define transitions; these state names are fixed.

| Enum | Values |
| --- | --- |
| `rarity` | `common`, `uncommon`, `rare`, `epic`, `legendary`, `mythic` |
| `resource_tier` (Crafting) | `common`, `uncommon`, `rare`, `epic`, `legendary` (subset of rarity) |
| `item_category` | `weapon`, `armor`, `accessory`, `consumable`, `tool`, `siege_equipment`, `material`, `token`, `quest_item`, `cosmetic`, `container` |
| `equip_slot` | `main_hand`, `off_hand`, `two_hand`, `ranged`, `head`, `shoulders`, `chest`, `hands`, `waist`, `legs`, `feet`, `back`, `neck`, `ring_1`, `ring_2`, `trinket_1`, `trinket_2`, `tool` |
| `affix_kind` | `primary`, `secondary`, `utility`, `set` |
| `binding` | `none`, `on_equip`, `on_pickup`, `account` |
| `item_status` | `active`, `escrowed`, `broken`, `quarantined`, `consumed`, `salvaged`, `destroyed` |
| `difficulty` | `story`, `normal`, `veteran`, `heroic`, `mythic` (dungeons use `story`, `normal`, `veteran`, `mythic`; raids use `normal`, `veteran`, `heroic`, `mythic`) |
| `dungeon_type` | `story`, `open_world`, `instanced`, `mythic`, `seasonal` |
| `group_size_class` | `solo_party` (1–5), `extended_party` (6–10), `raid_small` (10; flex 6–10; no mythic difficulty at launch), `raid_large` (20; flex 10–25 below mythic; mythic fixed 20) |
| `combat_role` | `tank`, `healer`, `damage`, `support` |
| `skill_band` | `novice`, `apprentice`, `journeyman`, `expert`, `master`, `legendary` |
| `skill_lock` | `up`, `down`, `locked` |
| `progression_track` | `combat`, `crafting`, `gathering`, `social`, `leadership`, `exploration` |
| `quest_type` | `story`, `dynamic`, `exploration`, `crafting`, `faction`, `raid`, `dungeon` |
| `quest_instance_state` | `offered`, `active`, `ready_to_turn_in`, `completed`, `failed`, `abandoned`, `expired` |
| `instance_run_state` | `requested`, `provisioning`, `ready`, `in_progress`, `completed`, `failed`, `abandoned`, `closed` |
| `encounter_state` | `dormant`, `idle`, `engaged`, `defeated`, `resetting` |
| `encounter_attempt_outcome` | `defeated`, `wiped`, `abandoned`, `invalidated` |
| `lockout_state` | `active`, `extended`, `expired` |
| `reward_grant_state` | `pending`, `delivering`, `delivered`, `failed`, `revoked` |
| `loot_mode` | `personal`, `group_need_greed`, `master_loot`, `council` (raid only) |
| `world_event_state` | `pending`, `active`, `resolved`, `failed` |
| `campaign_state` | `scheduled`, `active`, `stage_complete`, `completed`, `failed`, `archived` |
| `liveops_event_state` | `draft`, `scheduled`, `active`, `ended`, `cancelled`, `archived` |
| `liveops_event_kind` | `scheduled`, `triggered`, `dynamic` |
| `season_state` | `upcoming`, `active`, `ended` |
| `release_stage` | `dev`, `ptr`, `staging`, `prod` (promotion order: dev → ptr → staging → prod) |
| `detection_severity` | `info`, `low`, `medium`, `high`, `critical` |
| `enforcement_case_state` | `open`, `investigating`, `actioned`, `dismissed`, `appealed`, `closed` |
| `sanction_type` | `warning`, `chat_mute`, `trade_restriction`, `market_restriction`, `temporary_suspension`, `permanent_ban`, `item_rollback`, `currency_rollback`, `progression_rollback` |
| `privacy_class` | `p0_public`, `p1_pseudonymous`, `p2_personal`, `p3_sensitive` |
| `environment` | same values as `release_stage` |

Merged 2026-10-17 from the expanded specs (owning spec in brackets):

| Enum | Values |
| --- | --- |
| `progression_stage` [Character] | `new_character`, `skill_discovery`, `specialization`, `mastery`, `legendary_progression` |
| `xp_source_kind` [Character] | `quest`, `kill`, `dungeon`, `raid`, `world_event`, `discovery`, `practice`, `achievement`, `liveops`, `compensation`, `gm` |
| `account_unlock_kind` [Character] | `title`, `cosmetic`, `mount`, `recipe`, `catch_up` |
| `attribute_lock` [Character] | reuses `skill_lock` values (`up`, `down`, `locked`) |
| `container_kind` [Itemization] | `equipment`, `backpack`, `bag`, `bank`, `account_bank`, `guild_bank`, `mailbox`, `escrow`, `buyback`, `construction_cache`, `construction_recovery` (Construction Integration extension; Itemization remains sole writer) |
| `legendary_progress_state` [Itemization] | `in_progress`, `stage_ready`, `forged`, `maxed`, `suspended` |
| `loot_drop_state` [Itemization] | `pending`, `rolling`, `assigned`, `expired` |
| `armor_material` [Itemization] | `cloth`, `leather`, `mail`, `plate` |
| `loot_assignment_method` [Itemization] | `personal`, `roll`, `master`, `council`, `fallback`, `expired_to_gold` (payload field `assignment_method`) |
| `npc_tier` [Combat; scaling in Dungeon] | `common` (1.0), `elite` (2.5), `mini_boss` (5.0), `world_boss` (20.0) — health × T, archetype damage × √T |
| `lockout_period` [Dungeon] | `none`, `daily`, `weekly`, `cooldown` |
| `lockout_granularity` [Dungeon] | `loot_only`, `instance_saved` |
| `lockout_kind` [Raid] | `loot`, `progress` |
| `party_state` [Dungeon] | `active`, `queued`, `in_instance`, `disbanded` |
| `queue_ticket_state` [Dungeon] | `queued`, `proposed`, `matched`, `cancelled`, `expired` |
| `run_formed_by` [Dungeon] | `premade`, `lfg`, `portal`, `gm` |
| `encounter_kind` [Dungeon] | `boss`, `mini_boss`, `event`, `objective` |
| `raid_rank` [Raid] | `leader`, `assistant`, `member` |
| `raid_group_state` [Raid] | `forming`, `active`, `disbanded` |
| `ready_check_response` [Raid, shared with Dungeon] | `ready`, `not_ready`, `no_response` |
| `raid_mechanic_kind` [Raid] | `soak`, `spread`, `stack`, `interrupt`, `add_priority`, `tank_swap`, `frontal`, `dispel`, `kite`, `intercept`, `assignment`, `debuff_pass`, `positional_split`, `dps_check` |
| `reject_reason` [Security] | `malformed_input`, `rate_limited`, `movement_speed`, `movement_teleport`, `movement_vertical`, `movement_airtime`, `movement_collision`, `combat_unknown_ability`, `combat_range`, `combat_los`, `combat_facing`, `combat_cooldown`, `combat_cast_time`, `combat_resource`, `combat_state`, `combat_target`, `interact_range`, `interact_state`, `interact_timing`, `loot_ineligible`, `craft_invalid`, `handoff_invalid`, `economy_invariant`, `inventory_invariant`, `sanction_restricted`, `replay_detected`, `session_binding` |
| `detector_category` [Security] | `movement`, `combat`, `integrity`, `network`, `economy`, `botting`, `rmt`, `market`, `exploit`, `conduct`, `account_security`, `staff` |
| `detection_status` [Security] | `shadow`, `active`, `confirmed`, `false_positive`, `expired` |
| `risk_band` [Security] | `normal`, `watch`, `elevated`, `high`, `severe` |
| `offense_category` [Security] | `cheat_software`, `botting`, `duplication`, `exploit_abuse`, `rmt_seller`, `rmt_buyer`, `market_abuse`, `harassment`, `griefing`, `account_sale_or_sharing` |
| `sanction_scope` [Security] | `account`, `character` |
| `appeal_state` [Security] | `submitted`, `in_review`, `upheld`, `partially_overturned`, `overturned` |
| `rollback_state` [Security] | `planned`, `approved`, `executing`, `completed`, `compensating`, `failed` |
| `staff_role` [Security; used by LiveOps admin-portal] | `support_agent`, `game_master`, `senior_game_master`, `enforcement_reviewer`, `enforcement_lead`, `security_analyst`, `economy_analyst`, `liveops_operator`, `liveops_lead`, `store_manager`, `release_manager`, `auditor`, `break_glass_admin` |
| `release_state` [LiveOps] | `draft`, `candidate`, `deploying`, `deployed`, `rolled_back`, `superseded` |
| `rollout_state` [LiveOps] | `pending`, `in_progress`, `paused`, `completed`, `rolled_back` |
| `compensation_state` [LiveOps] | `draft`, `pending_approval`, `approved`, `issuing`, `completed`, `partially_failed`, `cancelled` |
| `purchase_state` [LiveOps] | `pending`, `authorized`, `completed`, `failed`, `refunded`, `charged_back` |
| `service_status` [LiveOps] | `operational`, `degraded`, `partial_outage`, `major_outage`, `maintenance` |
| `announcement_channel` [LiveOps] | `motd`, `login_banner`, `in_game_toast`, `launcher_news`, `status_page` |
| `alert_severity` [Analytics] | `sev1`, `sev2`, `sev3`, `sev4` |
| `service_health_state` [Analytics] | `healthy`, `degraded`, `unhealthy`, `unknown` |
| `event_modifier_kind` [LiveOps] | `xp_multiplier`, `loot_multiplier`, `gold_reward_multiplier`, `marks_multiplier`, `season_points_multiplier`, `gathering_yield_multiplier`, `skill_gain_multiplier`, `feature_toggle`, `vendor_unlock`, `campaign_window`, `world_event_request`, `director_bias`, `seasonal_dungeon` |

Merged 2026-10-17 from the adjacent-spec contract patches (owning spec in brackets):

| Enum | Values |
| --- | --- |
| `login_failure_reason` [Backend; classified by Security] | `bad_credentials`, `mfa_failed`, `challenge_failed`, `account_locked`, `suspended`, `banned`, `rate_limited`, `client_outdated`, `session_binding`, `server_error`, `timeout` (payload field `auth.login.failed.reason`) |
| `privacy_request_kind` [Backend] | `export`, `delete` |
| `privacy_request_state` [Backend] | `received`, `verifying`, `rejected`, `cooling_off`, `cancelled`, `processing`, `completed_with_holds`, `completed` |
| `account_state` [Backend] | `active`, `pending_deletion`, `erasing`, `erased` |
| `content_pack_visibility` [Backend] | `client_visible`, `server_only` |
| `wallet_lien_state` [Economy] | `active`, `settled`, `released`, `written_off` |
| `transfer_hold_state` [Economy] | `held`, `released`, `cancelled` |
| `wallet_owner_kind` [Economy] | `character`, `account`, `guild` |
| `currency_flow_class` [Economy] | `faucet`, `sink`, `transfer`, `correction` |
| `market_scope` [Economy] | `global_exchange`, `regional` |
| `market_listing_state` [Economy] | `active`, `sold`, `expired`, `cancelled` |
| `trade_state` [Economy] | `open`, `locked`, `held`, `completed`, `cancelled`, `failed` |
| `transfer_kind` [Economy; used by Security] | `trade`, `market`, `mail`, `guild` (player ↔ guild wallet; behind `ff.economy.guild_wallet_transfers_enabled`) |
| `faction_standing_tier` [AI] | `hated`, `hostile`, `unfriendly`, `neutral`, `friendly`, `honored`, `revered`, `exalted` |
| `faction_war_state` [AI] | `peace`, `tension`, `war` |
| `faction_change_source` [AI] | `quest_reward`, `quest_choice`, `reward_grant`, `world_event`, `raid_world_effect`, `gm_adjust`, `enforcement_rollback`, `reversal`, `realm_transfer` (opening entries of a cross-region transfer; Backend spec, Realm and character transfer) |
| `influence_cap_reason` [AI] | `none`, `per_call`, `character_weekly`, `region_weekly`, `influence_max`, `floor_zero` |
| `workshop_kind` [Crafting] | `forge`, `sawmill`, `alchemy_lab`, `tailor_shop`, `engineering_workshop` |
| `crafting_outcome` [Crafting] | `success`, `failure` |
| `server_kind` [World] | `zone`, `instance` |
| `server_instance_state` [World] | `booting`, `warm`, `allocated`, `ready`, `draining`, `terminated`, `lost` |
| `server_release_reason` [World] | `run_closed`, `run_failed`, `lost_replaced`, `zone_rebalance`, `maintenance`, `allocation_cancelled` |
| `movement_mode` [World] | `walk`, `run`, `sprint`, `crouch`, `swim`, `mounted` |
| `displacement_cause` [World] | `teleport_spell`, `charge`, `knockback`, `pull`, `fast_travel`, `respawn`, `resurrect`, `unstuck`, `zone_handoff`, `instance_entry`, `instance_exit`, `gm_move`, `vehicle` |
| `biome` [World] | `greenlands`, `frozen_north`, `desert_wastes`, `corrupted_frontier` (the launch biomes of the glossary) |

Merged 2026-10-17 from the phase-2 expansions of Combat, Economy, World, AI, Crafting and Backend (owning spec in brackets):

| Enum | Values |
| --- | --- |
| `damage_school` [Combat] | `physical`, `fire`, `frost`, `nature`, `shadow`, `arcane`, `holy`, `poison`, `disease` |
| `hit_outcome` [Combat] | `hit`, `crit`, `miss`, `resist`, `dodge`, `parry`, `block`, `crit_block`, `absorb`, `immune` |
| `combat_state` [Combat] | `out_of_combat`, `in_combat`, `dead`, `evading` |
| `cast_state` [Combat] | `idle`, `casting`, `channeling` |
| `ability_use_outcome` [Combat] | `resolved`, `rejected`, `interrupted`, `cancelled` |
| `cc_category` [Combat] | `stun`, `root`, `silence`, `fear`, `charm`, `incapacitate`, `disarm`, `taunt`, `knockback`, `slow` |
| `effect_stack_rule` [Combat] | `refresh`, `stack`, `independent`, `replace_if_stronger`, `unique_global` |
| `npc_armor_class` [Combat] | `light`, `medium`, `heavy` |
| `killer_kind` [Combat] | `npc`, `player`, `environment` |
| `resurrect_method` [Combat] | `spirit_healer`, `player_ability`, `encounter_reset`, `gm` |
| `npc_archetype` [AI behavior; Character attribute weights; Combat damage coefficients] | `brute`, `skirmisher`, `caster`, `healer`, `guardian`, `beast`, `commander`, `shaman`, `champion`, `guard`, `civilian` |
| `npc_aggression` [AI] | `aggressive`, `defensive`, `passive` |
| `faction_stance` [AI] | `allied`, `neutral`, `hostile` |
| `world_event_category` [AI] | `invasion`, `world_boss`, `caravan_attack`, `faction_war`, `resource_shortage`, `discovery`, `seasonal`, `ambient` |
| `world_event_intensity` [AI] | `minor`, `major`, `epic` |
| `world_event_end_reason` [AI] | `objectives_completed`, `objectives_failed`, `timeout`, `cancelled`, `resolved_by_effect`, `server_lost`, `no_participants` |
| `world_event_trigger_kind` [AI] | `director`, `liveops_request`, `faction_ai`, `quest_outcome`, `raid_effect`, `world_schedule`, `gm` |
| `world_event_reward_tier` [AI] | `gold`, `silver`, `bronze` |
| `director_decline_reason` [AI] | `definition_not_found`, `definition_not_requestable`, `no_eligible_region`, `conflicts_faction_war`, `invasion_cooldown`, `exceeds_daily_cap`, `kill_switch`, `invalid_window` |
| `realm_state` [World] | `offline`, `locked`, `open`, `draining`, `drained` |
| `realm_role` [LiveOps; mirrored by World `SetRealmRole`] | `canary`, `standard` |
| `handoff_kind` [World] | `border`, `instance_entry`, `instance_exit`, `fast_travel`, `migration`, `layer_change`, `admin_move`, `login` |
| `handoff_failure_reason` [World] | `bad_signature`, `expired`, `wrong_server`, `nonce_reused`, `state_mismatch`, `not_owner`, `character_mismatch`, `target_full`, `target_unavailable`, `world_unavailable`, `ticket_expired_uncommitted`, `owner_conflict_resolved`, `client_timeout` |
| `day_phase` [World] | `dawn`, `day`, `dusk`, `night` |
| `weather_kind` [World] | `clear`, `overcast`, `rain`, `storm`, `fog`, `snow`, `blizzard`, `sandstorm`, `heatwave`, `ashfall` |
| `terrain_edit_kind` [World] | `dig`, `raise`, `flatten`, `smooth`, `paint`, `restore` |
| `terrain_edit_state` [World] | `committed`, `reverted`, `healed`, `rebase_dropped` |
| `spawn_slot_state` [World] | `active`, `respawning`, `dormant`, `disabled` |
| `poi_kind` [World] | `landmark`, `settlement`, `dungeon_entrance`, `vista`, `ruin`, `resource_site` |
| `world_action_kind` [World] | `weather_override`, `spawn_entities`, `despawn_entities`, `terrain_revert`, `edit_window` |
| `world_action_state` [World] | `pending`, `active`, `expired`, `cancelled`, `failed` |
| `character_state` [Backend] | `creating`, `active`, `transferring`, `deleted`, `purged` |
| `realm_transfer_state` [Backend] | `requested`, `validating`, `rejected`, `locked`, `staging`, `committing`, `completed`, `aborting`, `aborted` |
| `privacy_step_state` [Backend] | `pending`, `in_progress`, `done`, `held`, `skipped` |
| `legal_hold_scope` [Backend; holds owned by Security] | `enforcement_evidence`, `chat`, `economy_ledger`, `item_ledger`, `audit`, `telemetry`, `purchase_records` |
| `legal_hold_basis` [Backend; holds owned by Security] | `open_case`, `fraud_prevention`, `legal_obligation`, `payment_dispute` |
| `service_tier` [Backend] | `tier_0` (critical path; membership in Backend spec, Service tiers, critical path and SLOs), `tier_1`, `tier_2` |
| `recipe_kind` [Crafting] | `refine`, `component`, `gear`, `tool`, `consumable`, `structure_material`, `reforge`, `reroll`, `legendary_forge` |
| `recipe_learn_source` [Crafting] | `default`, `trainer`, `scroll`, `reward`, `account_unlock`, `discovery`, `gm` |
| `crafting_attempt_state` [Crafting] | `requested`, `rolled`, `succeeded`, `failed`, `error` |
| `gathering_node_state` [Crafting; regeneration clock: World] | `pending_spawn`, `available`, `depleted`, `retired` |

---

## 7. Shared constants, currencies, sources/sinks, resets

### 7.1 Constants

Values are defaults published as tuning keys in `config-service`; the key is authoritative at runtime.

| Constant | Value | Tuning key | Owning spec |
| --- | --- | --- | --- |
| `LEVEL_CAP` | 100 | `tune.progression.level_cap` | Character |
| `SKILL_MAX` | 1000 | `tune.progression.skill_max` | Character |
| `GLOBAL_SKILL_CAP` | 7200 | `tune.progression.global_skill_cap` | Character |
| `TRACK_SKILL_CAP` | 4000 per track | `tune.progression.track_skill_cap` | Character |
| `ATTRIBUTE_NATURAL_CAP` | 150 per attribute (before items/effects) | `tune.progression.attribute_natural_cap` | Character |
| `ATTRIBUTE_TOTAL_NATURAL_CAP` | 600 across all eight | `tune.progression.attribute_total_cap` | Character |
| `PARTY_MAX` | 5 | `tune.group.party_max` | Dungeon |
| `EXTENDED_PARTY_MAX` | 10 | `tune.group.extended_party_max` | Dungeon |
| `RAID_GROUP_MAX` | 25 | `tune.group.raid_group_max` | Raid |
| `MYTHIC_LEVEL_MAX` | 20 (launch) | `tune.dungeon.mythic_level_max` | Dungeon |
| `LUCK_MAX_INFLUENCE` | 10% | `tune.combat.luck_max_influence` | Combat |
| `FRONTIER_MARKS_WEEKLY_CAP` | 1,500 | `tune.economy.frontier_marks_weekly_cap` | Economy (values used by Quest/Dungeon) |
| `UNSPECIALIZED_SKILL_CAP` | 900 (skills above require a specialization) | `tune.progression.unspecialized_skill_cap` | Character |
| `MAX_SPECIALIZATIONS` | 3 | `tune.progression.max_specializations` | Character |
| `ATTRIBUTE_MIN` | 10 | `tune.progression.attribute_min` | Character |
| `PRESTIGE_RANK_MAX` | 999 | `tune.progression.prestige_rank_max` | Character |
| `LEGENDARY_EQUIPPED_MAX` | 1 | `tune.item.legendary_equipped_max` | Itemization |
| `LOOT_TRADE_WINDOW` | 120 minutes (group/raid `on_pickup` loot, eligible recipients only) | `tune.loot.trade_window_minutes` | Itemization |
| `LUCK_REFERENCE` | 150 (effective luck that reaches `LUCK_MAX_INFLUENCE`; confirmed by Combat spec, Luck; also used by crafting quality) | `tune.loot.luck_reference` | Itemization / Combat |
| `HASTE_MAX` / `CRIT_CAP` | 0.75 (time-reduction fraction) / 0.60 | `tune.combat.haste_max`, `tune.combat.crit_cap` | Combat |
| `GCD_BASE` / `GCD_MIN` | 1.5 s / 0.75 s (`gcd_eff = max(GCD_MIN, GCD_BASE × (1 − h))`) | `tune.combat.gcd_base_s`, `tune.combat.gcd_min_s` | Combat |
| Rating constants at level 100 | crit 1,400; haste 1,750; dodge/parry 1,400; block 700; armor/magic pen 385; crit damage 146; scaled by `B(4L)/B(400)` | `tune.combat.rating_k_<stat>` | Combat |
| `QUALITY_BONUS_MAX` | 50; fixed mapping `quality_bonus = floor(quality_score / 2)`, `quality_score` 0–100 | — | Crafting |
| `REPUTATION_MIN` / `REPUTATION_MAX` / `INFLUENCE_MAX` | −45,000 / 45,000 / 1,000 | `tune.faction.*` | AI |
| `CELL_SIZE_M` | 64 m | `tune.world.cell_size_m` | World |
| `TELEPORT_TOKEN_TTL` | 2,000 ms in-zone; 10,000 ms cross-server | `tune.world.*` | World |
| `PRIVACY_REQUEST_SLA` | 30 days from verification (delete grace 14 days inside it) | `tune.backend.privacy_delete_grace_days` | Backend |
| `RAID_SMALL_REF_SIZE` / `RAID_LARGE_REF_SIZE` / `RAID_MYTHIC_SIZE` | 10 / 20 / 20 | `tune.raid.small_ref_size`, `tune.raid.large_ref_size`, `tune.raid.mythic_size` | Raid |
| `SEASON_LENGTH_WEEKS` | 12 | `tune.liveops.season_length_weeks` | LiveOps |
| `MAX_COMBINED_XP_MULTIPLIER` | 2.5 (ceiling across all XP bonus sources incl. LiveOps overlays) | `tune.liveops.max_combined_xp_multiplier` | LiveOps (applied by Character's XP formula) |
| `TELEMETRY_P2_RETENTION` | 30 days for `p2_personal` fields | `tune.analytics.p2_retention_days` | Analytics |
| `IDEMPOTENCY_RETENTION` | 7 days | — | Backend |
| `EVENT_SCHEMA_RETENTION_RAW` | 25 months raw telemetry; p2/p3 fields per Analytics privacy rules | — | Analytics |
| `AVOIDANCE_CAP` / `BLOCK_CAP` / `BLOCK_REDUCTION` | 0.50 / 0.75 / 0.30 | `tune.combat.avoidance_cap`, `tune.combat.block_cap`, `tune.combat.block_reduction` | Combat |
| `ARMOR_DR_CAP` / `RESIST_CAP` / `PEN_CAP` / `CRIT_DAMAGE_MAX` | 0.85 / 0.75 / 0.60 / 3.0 | `tune.combat.*` | Combat |
| `THROUGHPUT_SCALE` σ(L) | per-level table, σ(100) = 0.3098 | `tune.combat.throughput_scale` | Combat |
| `DPS_ref(L, i)` | reference player DPS (Combat spec, Reference throughput); canonical TTK input for Dungeon, Raid and Security TTK floors | `tune.combat.*` | Combat |
| `POWER_SPEED_REF` / `ROTATION_MPS_REF` / `HPS_REF_FRACTION` | 2.4 s / 1.6 / 0.06 | `tune.combat.*` | Combat |
| `KILL_CREDIT_SHARE` / `KILL_CREDIT_RADIUS_M` | 0.10 / 60 m | `tune.combat.kill_credit_share`, `tune.combat.kill_credit_radius_m` | Combat |
| `DR_WINDOW_S` / `CC_REDUCTION_CAP` | 18 s / 0.40 | `tune.combat.*` | Combat |
| `COMBAT_EXIT_S` | 5 s | `tune.combat.combat_exit_s` | Combat |
| `TAUNT_THREAT_MULT` / `TAUNT_FORCE_S` | 1.10 / 3 s (taunt sets threat to `top × 1.10`) | `tune.combat.taunt_threat_mult`, `tune.combat.taunt_force_s` | Combat |
| `DAY_LENGTH_MIN` | 60 (one full in-game day, real minutes) | `tune.world.day_length_minutes` | World |
| `NIGHT_LENGTH_MIN` | 15 (real minutes of night within the day) | `tune.world.night_length_minutes` | World |
| `ZONE_CAPACITY_SOFT` / `ZONE_CAPACITY_HARD` | 200 / 250 players | `tune.world.zone_capacity_soft`, `tune.world.zone_capacity_hard` | World |
| `BASE_RUN_SPEED` / `V_CAP_FOOT` / `V_CAP_MOUNTED` / `V_CAP_ABSOLUTE` | 7.0 / 12.0 / 20.0 / 20.0 m/s | `tune.world.*` | World (read by Security movement validation) |
| `TERRAIN_MAX_VOLUME_M3` | 100 m³ per edit | `tune.world.terrain_max_volume_m3` | World |
| `CHARACTER_SLOTS_PER_REALM` | 8 | `tune.backend.character_slots_per_realm` | Backend |
| `CHARACTER_RESTORE_DAYS` | 30 | `tune.backend.character_restore_days` | Backend |
| `NAME_REUSE_QUARANTINE_DAYS` | 90 | `tune.backend.name_reuse_quarantine_days` | Backend |
| `REALM_TRANSFER_COOLDOWN_DAYS` | 30 | `tune.backend.realm_transfer_cooldown_days` | Backend |
| `BACKUP_RETENTION_DAYS` | 35 (bounds personal data in backups) | — (infrastructure) | Backend |
| `NPC_PRICE_MULT` range | `npc_price_mult = clamp((1 − d_cha) × (1 + f_tier), 0.75, 1.25)` from natural Charisma and faction tier | `tune.economy.npc_price_mult_min`, `tune.economy.npc_price_mult_max` | Economy (read by Itemization repair, World fast travel) |
| `QUALITY_SCORE_MAX` / `WORKSHOP_TIER_MAX` / `PUBLIC_WORKSHOP_TIER_MAX` | 100 / 5 / 3 (tiers 4–5 only in player or guild workshops) | `tune.crafting.workshop_tier_max`, `tune.crafting.public_workshop_tier_max` | Crafting |
| `CRAFTED_RARITY_CEILING` | `epic` (`legendary` only via legendary path; restates §8) | — | Crafting |

Tuning and flag domains registered with these specs: `tune.combat.*`, `tune.economy.*`, `tune.world.*`, `tune.ai.*`, `tune.director.*`, `tune.faction.*`, `tune.crafting.*`, `tune.backend.*`, `tune.building.*`, `tune.construction.*`; `ff.economy.*`, `ff.world.*`, `ff.director.*`, `ff.crafting.*`, `ff.backend.*`. Individual keys are listed in each owning spec's Tuning keys section; only keys read by another domain appear in the table above. The exact 16 construction/territory/weather keys and typed schemas in [Construction Integration §9](Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md#9-canonical-construction-configuration-declarations) are adopted as canonical. Historical `tune.*` identifiers do not authorize live editing of formulas or topology; Tuning and Configuration governs editability.

### 7.2 Currencies, sources and sinks

| Currency ID | Storage | Notes |
| --- | --- | --- |
| `currency/gold` | int64 copper; 1 gold = 100 silver = 10,000 copper | Primary player currency; tradeable. |
| `currency/frontier_marks` | int64 | Earned from dungeons/quests/world events; weekly cap; not tradeable. |
| `currency/season_tokens` | int64, wallet owner = character | Earned and spent within a season; converted (capped, source `season_conversion`) or expired (sink `season_expiry`) at season end (LiveOps). |
| `currency/crowns` | int64 | Premium currency (store-service). **Never converts into gold, items with stat budgets, XP, or skill gains.** Cosmetic and convenience only. |

Every currency or item creation/destruction carries one `source`/`sink` code. Canonical codes (extend via Registry additions):

- Sources: `quest_reward`, `dungeon_reward`, `raid_reward`, `world_event_reward`, `npc_bounty`, `loot_drop`, `crafting_output`, `gathering_output`, `salvage_output`, `vendor_sale`, `market_sale`, `trade_receive`, `achievement_reward`, `season_reward`, `liveops_compensation`, `store_purchase` (items/unlocks granted by a crowns purchase), `crown_topup` (crowns credited from a real-money purchase), `store_refund`, `gm_grant`, `legendary_upgrade`, `vendor_stock` (items bought from NPC vendors), `enforcement_restore`, `season_conversion`, `tier_conversion_output` (Raid weekly tier-set conversion), `settlement_tax_income` (credit to the settlement-owning guild wallet; flow class `transfer`), `starter_kit` (new-character starter bundle).
- Sinks: `repair_cost`, `housing_tax`, `territory_upkeep`, `fast_travel`, `market_listing_fee`, `market_transaction_fee`, `market_expiry_fee`, `guild_upkeep`, `siege_declaration`, `npc_service`, `vendor_purchase`, `crafting_consumed`, `crafting_failure`, `salvage_input`, `item_destroyed`, `trade_send`, `training_cost`, `legendary_upgrade_cost`, `enforcement_rollback`, `season_expiry`, `quest_turn_in`, `campaign_contribution`, `raid_bonus_roll`, `tier_conversion_input`, `store_spend`, `store_chargeback`, `account_erasure` (balances removed by account deletion; flow class `correction`, excluded from faucet/sink metrics), `market_purchase` (buyer-side debit; classified as a transfer, not a sink, in faucet/sink metrics), `settlement_tax_paid` (seller debit on a fill at a settlement hub; flow class `transfer`), `guild_dissolution` (assets of a disbanded guild after the retention window; flow class `correction`), `consumable_used` (consumables used up: repair kits, recipe scrolls, potions, food).

Construction adds source `construction_material_return` and sink `construction_material_consumed`, with causal allocation/decision receipts defined in Construction Integration. Returns are lineage-linked recovery of previously consumed material, classified as recovery for analytics and never as fresh gathering/crafting production. Existing stored-item movement on cache destruction/archive/capture is a transfer, not either code. Construction repair uses `repair_cost`, upkeep uses `territory_upkeep`, and siege declarations use `siege_declaration`.

Stack splits and merges inside inventory-service use the internal ledger value `split`; it is never published as a source/sink code because no value is created or destroyed.

### 7.3 Resets

Configured in `config-service` (`tune.liveops.reset_schedule`). Defaults (UTC): `na` daily 15:00, weekly Tuesday 15:00; `eu` daily 04:00, weekly Wednesday 04:00; `apac` daily 22:00, weekly Wednesday 22:00; `sa` daily 18:00, weekly Tuesday 18:00.

### 7.4 Shared reward scaling functions

Every reward-authoring spec (Quest, Dungeon, Raid, LiveOps, AI world events) expresses XP, gold, and marks with these functions and a content weight `w` (standard quest = 1.0). Character owns `xp_to_next(L)`; the rest are fixed here and tunable through the listed keys.

| Function | Definition | Tuning key |
| --- | --- | --- |
| `xp_to_next(L)` | XP needed to go from level L to L+1 (Character spec) | `tune.progression.xp_curve` |
| `xp_reward(L, w)` | `round(w × xp_to_next(L) / 12)` — a standard quest is 1/12 of a level | `tune.progression.xp_reward_divisor` (12) |
| `gold_reward(L, w)` | `round(w × 100 × (1 + L)^1.5)` copper (L=1 ≈ 2.8 silver, L=50 ≈ 3.6 gold, L=100 ≈ 10.2 gold) | `tune.economy.gold_reward_scale` (100) |
| `marks_reward(w)` | `round(w × 50)` frontier marks; only at Character Level ≥ 60 | `tune.economy.marks_reward_scale` (50) |
| `lp_reward(w)` | `round(w × 2)` learning points (in addition to LP from levels) | `tune.progression.lp_reward_scale` (2) |

L is the character's level, clamped to the content's level range. Weights at or above 5.0 require LiveOps economy review.

---

## 8. Power ladder (item level and progression alignment)

Authoritative ilvl targets shared by Itemization, Quest, Dungeon, Raid, Crafting, and LiveOps. Itemization owns stat-budget formulas built on these numbers.

| Source | ilvl | Rarity ceiling |
| --- | --- | --- |
| Levelling content at Character Level L (quests, open world, levelling dungeons) | `4 × L` (4–400) | `rare` (`epic` for dungeon final bosses) |
| Crafted gear | from crafter skill: `floor(skill / 1000 × 400) + quality bonus (0–50)` → 0–450 | `epic` (`legendary` via legendary path) |
| Dungeon `normal` at cap | 405 | `rare` |
| World events at cap | 405 | `rare` |
| World bosses at cap | 415 | `epic` |
| Dungeon `veteran` | 415 | `epic` |
| Raid `normal` | 420 | `epic` |
| Dungeon `mythic` level M (end-of-run reward) | `420 + 2 × min(M, 15)` (422–450) | `epic` |
| Raid `veteran` | 430 | `epic` |
| Raid `heroic` | 445 | `epic` |
| Raid `mythic` | 460 | `epic` |
| Legendary items (final stage) | 465, upgradeable to 475 | `legendary` |
| `mythic` rarity | reserved; not obtainable at launch | — |

**Seasonal offset.** The ladder above is raid tier 1 (launch). Each later raid tier adds `ilvl_tier_offset(t) = 15 × (t − 1)` (tuning key `tune.item.tier_ilvl_step`, default 15) to every row at or above "Dungeon `normal` at cap", including crafted and legendary ceilings; levelling rows do not change. Itemization's stat-budget curve must remain valid to ilvl 1000.

---

## 9. Concept ownership (who decides behavior)

| Concept | Authoritative spec | Consumers |
| --- | --- | --- |
| Attribute → secondary stat formulas, damage, mitigation, threat, abilities and effects, reference throughput `DPS_ref`, creature tiers | Combat | all |
| XP curve, levels, LP, skill gain, caps, mastery, specialization, prestige, achievements, anti-grind | Character | Quest, Dungeon, Raid, Itemization, LiveOps, Analytics |
| Item definitions, rarity, stat budget, affixes, sets, durability, binding, legendary paths, loot tables, pity, reward bundles and grants | Itemization | Quest, Dungeon, Raid, Crafting, Economy, LiveOps |
| Crafting quality, recipes and recipe learning, workshops, gathering nodes and yields, salvage profiles | Crafting | Itemization, Economy, Character |
| Currency balance, markets, trades, taxes, inflation index | Economy | all reward-granting specs, Analytics |
| Quest model, objectives, generation, branching, campaigns | Quest | AI, LiveOps, Dungeon, Raid |
| Instance runs, lockouts (framework), encounter framework, group/LFG, dungeon affixes, mythic scaling | Dungeon | Raid |
| Raid tiers, raid sizes, raid lockout policy, raid encounter design, raid loot modes, raid world integration | Raid | Itemization, AI, LiveOps |
| Faction AI, world director, world event selection/lifecycle/payloads, NPC behavior and memory | AI | Quest, Raid, Dungeon, World, LiveOps |
| World structure and cells, server fleet and realms, movement `v_eff`, teleport tokens, handoffs, world clock and weather, terrain edits, spawn execution, resource regeneration clock, world-boss placement and fixed windows | World | AI, Crafting, Dungeon, Raid, Security, LiveOps |
| Accounts and sessions (account core), character lifecycle, service communication conventions, persistence conventions, content delivery, privacy workflow, legal holds contract, realm transfer | Backend | all |
| NPC vendor pricing (`npc_price_mult`), transfer holds, wallet liens | Economy | Itemization, World, Security |
| Threat model, server validation, detections, risk score, enforcement, audit, rollbacks | Security | all |
| Event calendar, seasons, flags, tuning, releases, store, compensation, community tools | LiveOps | all |
| Event envelope implementation, pipeline, warehouse, metric definitions, dashboards, alerting, privacy/retention | Analytics | all |

---

## 10. Event taxonomy

### 10.1 Envelope (all domain and telemetry events)

```json
{
  "event_id": "evt_01J9ZK3Q8M4V6T2N5X7B1C0D9E",
  "event_name": "quest.instance.completed",
  "schema_version": 1,
  "occurred_at": "2026-10-17T15:00:00.000Z",
  "producer": { "service": "quest-service", "instance_id": "quest-service-7c9f", "build": "1.4.2+b8812" },
  "environment": "prod",
  "dc_region": "na",
  "realm_id": "na-1",
  "zone_id": "zone/greenlands_01",
  "account_id": "acc_…",
  "character_id": "chr_…",
  "session_id": "ses_…",
  "correlation_id": "uuid-or-ulid shared across one player action or flow",
  "causation_id": "event_id or request id that directly caused this event",
  "content_version": 1842,
  "privacy_class": "p1_pseudonymous",
  "payload": { }
}
```

Rules: unknown/absent context fields are omitted, not null. Client telemetry uses `producer.service = "game-client"`. `event_id` is globally unique and is the deduplication key. Payload schemas are versioned per event (`schema_version`), evolve additively; breaking changes require a new major event version and dual publishing for one season. Payload schemas are defined in the owning specification; Analytics registers them in the schema registry.

Topics on `event-bus`: `domain.<domain>` (keyed by the aggregate ID, e.g. `character_id`), `telemetry.client`, `telemetry.server`, `ops.alerts`. Retention: domain topics 14 days (warehouse holds history), telemetry 3 days.

### 10.2 Canonical event catalog

Owner = publishing service. Specs define payloads; Analytics maps them to warehouse tables.

**auth** (auth-service; payloads: Backend spec): `auth.account.created`, `auth.account.deletion_requested`, `auth.account.deletion_cancelled`, `auth.account.deletion_started`, `auth.account.deleted`, `auth.account.export_requested`, `auth.account.export_completed`, `auth.login.succeeded`, `auth.login.failed`, `auth.session.started`, `auth.session.ended`, `auth.mfa.enrolled`, `auth.mfa.removed`, `auth.credential.changed`.

**character** (character-service; payloads: Backend spec): `character.profile.created`, `character.profile.deleted`, `character.profile.restored`, `character.profile.renamed`, `character.world.entered`, `character.world.left` (`left_reason` includes `realm_transfer`), `character.zone.changed`, `character.transfer.requested`, `character.transfer.completed`, `character.transfer.aborted`.

**guild** (guild-service; payloads: Backend spec): `guild.profile.created`, `guild.profile.disbanded`, `guild.member.joined`, `guild.member.left`, `guild.leader.changed`.

**content** (content-catalog-service; payloads: Backend spec): `content.pack.published`, `content.version.activated`.

**progression** (progression-service): `progression.xp.granted`, `progression.level.reached`, `progression.lp.granted`, `progression.lp.spent`, `progression.skill.gained`, `progression.skill.band_reached`, `progression.skill.lock_changed`, `progression.attribute.changed`, `progression.mastery.node_unlocked`, `progression.specialization.selected`, `progression.prestige.rank_reached`, `progression.achievement.earned`, `progression.account_unlock.granted`, `progression.stage.changed`, `progression.mastery.tree_reset`, `progression.specialization.removed`.

**item / inventory** (inventory-service): `item.instance.created`, `item.instance.destroyed`, `item.instance.bound`, `item.instance.broken`, `item.instance.repaired`, `item.instance.salvaged`, `item.instance.modified`, `item.instance.quarantined`, `item.legendary.stage_advanced`, `inventory.item.moved`, `inventory.item.equipped`, `inventory.item.unequipped`.

**loot** (loot-service): `loot.table.rolled`, `loot.drop.created`, `loot.roll.resolved`, `loot.drop.assigned`, `loot.pity.triggered`.

**reward** (reward-service): `reward.grant.issued`, `reward.grant.delivered`, `reward.grant.failed`, `reward.grant.revoked`.

**crafting / gathering** (crafting-service; gathering emitted by zone-server): `crafting.item.crafted`, `crafting.attempt.failed`, `crafting.recipe.learned` (`domain.crafting`, key `character_id`; payload: Crafting spec), `gathering.node.harvested`, `gathering.node.depleted`.

**economy / market** (economy-service): `economy.currency.credited`, `economy.currency.debited`, `economy.trade.completed`, `economy.index.computed`, `economy.lien.placed`, `economy.lien.closed`, `economy.hold.placed`, `economy.hold.released`, `economy.snapshot.exported`, `market.listing.created`, `market.listing.sold`, `market.listing.expired`, `market.listing.cancelled`.

**quest** (quest-service): `quest.instance.offered`, `quest.instance.accepted`, `quest.objective.progressed`, `quest.objective.completed`, `quest.instance.completed`, `quest.instance.turned_in`, `quest.instance.failed`, `quest.instance.abandoned`, `quest.instance.expired`, `quest.choice.made`, `quest.template.generated`, `quest.campaign.contributed`, `quest.campaign.stage_reached`.

**group** (group-service): `group.party.formed`, `group.party.disbanded`, `group.member.joined`, `group.member.left`, `group.queue.joined`, `group.queue.matched`, `group.queue.left`, `group.ready_check.completed`, `group.raid_group.formed`, `group.raid_group.updated`, `group.raid_group.disbanded` (raid group events: Raid spec).

**instance / encounter / raid** (instance-service; encounter events originate in instance-server and are relayed by instance-service): `instance.run.created`, `instance.run.started`, `instance.run.completed`, `instance.run.failed`, `instance.run.abandoned`, `instance.run.closed`, `instance.lockout.created`, `instance.lockout.extended`, `instance.lockout.reset`, `instance.affix.rotated`, `encounter.attempt.started`, `encounter.phase.changed`, `encounter.attempt.defeated`, `encounter.attempt.wiped`, `encounter.attempt.abandoned`, `encounter.attempt.invalidated`, `encounter.mechanic.failed`, `raid.progress.updated`, `raid.progress.first_kill_recorded`, `raid.first_kill.revoked`, `raid.world_effect.applied`.

**combat** (zone-server / instance-server; `combat.npc.killed` is a durable domain event on `domain.combat` carrying `credited_character_ids`, the rest are telemetry; canonical payloads: Combat spec, Combat events): `combat.npc.killed`, `combat.character.died`, `combat.character.resurrected`, `combat.damage.summarized`, `combat.pvp_kill.recorded`, `combat.profile.sampled` (telemetry stat snapshot).

**world / faction** (ai-director-service, world-service, zone-server, building-service, faction-service): `world.event.started`, `world.event.stage_changed`, `world.event.ended`, `world.director.decided` (ai-director-service telemetry; payloads of `world.event.*` and `world.director.*`: AI spec), `world.structure.placed`, `world.structure.destroyed` (producer building-service, topic `domain.world`, payload: Construction Integration §8; zone-server supplies validated facts and replication), `world.poi.discovered` (published by world-service after the durable discovery insert), `world.server.allocated`, `world.server.released`, `world.server.lost` (`domain.world`, also forwarded to `ops.alerts`), `world.handoff.completed`, `world.handoff.failed`, `world.realm.state_changed`, `world.weather.changed`, `world.terrain.modified` (world-service; payloads: World spec; `world.server.*` other than `lost` and `world.handoff.*` travel on `telemetry.server`), `faction.reputation.changed`, `faction.control.changed` (payloads: AI spec).

**security** (anticheat-service): `security.detection.raised`, `security.score.changed`, `security.integrity.failed`, `security.action.rejected`, `security.trade.flagged`.

**enforcement** (enforcement-service): `enforcement.case.opened`, `enforcement.case.resolved`, `enforcement.sanction.applied`, `enforcement.sanction.lifted`, `enforcement.appeal.submitted`, `enforcement.appeal.resolved`, `enforcement.rollback.executed`.

**audit** (audit-service): `audit.record.appended`.

**liveops / config / store** (liveops-service, config-service, store-service): `liveops.event.scheduled`, `liveops.event.started`, `liveops.event.ended`, `liveops.event.cancelled`, `liveops.release.rolled_back`, `liveops.maintenance.scheduled`, `config.rollout.rolled_back`, `liveops.season.started`, `liveops.season.ended`, `liveops.release.deployed`, `liveops.announcement.published`, `liveops.survey.published`, `liveops.compensation.granted`, `config.flag.changed`, `config.tuning.published`, `store.offer.viewed`, `store.purchase.completed`, `store.purchase.refunded`.

**client telemetry** (game client via telemetry-ingest): `client.session.heartbeat`, `client.perf.sampled`, `client.crash.reported`, `client.load.completed`, `client.ui.interacted`, `client.survey.submitted`, `client.integrity.reported`.

**server/ops telemetry** (all servers via telemetry-ingest): `ops.server.tick_sampled`, `ops.zone.load_sampled`, `ops.service.health_changed`, `ops.alert.fired`.

---

## 11. Registry change log

Companion extension (2026-10-19): selected Progression as the durable writer; §12 tracks proposed protocol identifiers and adapter gates separately from merged runtime schemas. This is design, not deployed capability.

| Date | Change |
| --- | --- |
| 2026-10-17 | Initial registry created for the expanded Character, Itemization, Quest, Dungeon, Raid, Security, LiveOps, and Analytics specifications. |
| 2026-10-17 | Merged registry additions from the eight expanded specs (Analytics: `alr_`, `exp_`, `alert_severity`, `service_health_state`, `TELEMETRY_P2_RETENTION`, `market_purchase`, `game-client` producer, detectors `telemetry_tamper`/`headless_presence` for Security to define): entities/prefixes (`elv_`, `lcs_`, `iat_`, `evd_`, `rbk_`, `rpt_`, `mnt_`), content kinds, enums, constants, source/sink codes, power-ladder world rows and seasonal offset, events. Decisions: `staff_role` replaces LiveOps `gm_role`; one loot trade window key; NPC creature tiers scale health × T and archetype damage × √T only (Dungeon spec, Scaling); `combat.npc.killed` is durable; `quest.objective.completed` with `instance_ready = true` signals `ready_to_turn_in` (no extra event). |
| 2026-10-17 | Merged contract patches of the adjacent specs (Combat, Economy, World, AI, Crafting, Backend): prefixes `prq_`, `wln_`, `thd_`, `fsl_`, `crf_`, `hrv_`, `hof_`, `tpt_`; kinds `continent`, `gathering_node`; enums (adjacent-spec table in §6); constants; sink `account_erasure`; events `auth.account.*` deletion/export, `economy.lien.*`, `economy.hold.*`, `economy.snapshot.exported`, `world.server.*`, `world.handoff.*`. RPC table: faction-service ratified, world-service specified, economy/crafting/privacy RPCs added. `combat.*` payloads moved from Dungeon to Combat. `LUCK_REFERENCE` confirmed. `auth.login.failed` field is `reason` (`login_failure_reason`). |
| 2026-10-17 | Construction closure: registered building-service, clm_/strc_/sge_/cba_ entities, construction_cache/recovery container kinds, construction material ledger codes, structure event producer/payloads and the 16 typed control declarations in Construction Integration §9. These are design contracts, not deployed APIs. |
| 2026-10-17 | Merged the phase-2 expansions of Combat, Economy, World, AI, Crafting and Backend (entry files plus part folders `Combat/`, `Economy/`, `World-Simulation/`, `AI/`, `Crafting/`, `Backend/`): entities `wer_`, `dbw_`, `gqr_`, `ted_`, `wac_`, `rtx_`, `mal_`, `hld_` and keyed NpcMemory, Realm, SpawnSlotState, RegionResourceState, PoiDiscovery, WeatherState, WorldBossSchedule, KnownRecipe; content kinds `ability`, `effect`, `vendor`, `behavior_tree`, `utility_profile`, `schedule`, `spawnset`, `mount`, `poi`, `route`, `salvage`; world-service gains PostgreSQL `world`; RPC rows for ai-director, gateway, character, content-catalog, transfer and content-reference participants, plus economy vendor, inventory vendor/buyback, world realm/terrain/weather and crafting recipe RPCs; phase-2 enum table; constants; codes `settlement_tax_income`, `starter_kit`, `settlement_tax_paid`, `guild_dissolution`, `consumable_used`; `transfer_kind` += `guild`; `faction_change_source` += `realm_transfer`; events `character.profile.restored/renamed`, `character.transfer.*`, `guild.*`, `content.*`, `combat.profile.sampled`, `world.director.decided`, `world.realm.state_changed`, `world.weather.changed`, `world.terrain.modified`, `crafting.recipe.learned`. Decisions: `npc_archetype` is the union of Character's and AI's lists (11 values); AI's proposed kinds `bt`/`up` renamed `behavior_tree`/`utility_profile` (Dungeon examples updated); `buff/` uses kind `effect`; Backend's `guild.guild.*` renamed `guild.profile.*`; world events split (AI: selection, lifecycle, payloads; World: placement, hosting, fixed windows via `RequestWorldEvent` with `trigger_kind = world_schedule`; zone-server progress via `ZoneLink`); `realm_role` owned by LiveOps; World owns respawn timing under scarcity (regeneration slows, Crafting's faster-respawn draft withdrawn); Combat formulas win over Dungeon/Raid/Security copies (taunt × 1.10, `DPS_ref`, `gcd_eff`); `GetLegalHold` ratified. |

## 12. Companion extension proposals (2026-10-19)

[Mounts and Companions](Legends_Shattered_Frontier_Mounts_and_Companions_v0.1.md) selects `progression-service` / schema `progression` as sole durable writer for Stables, owned creatures, training/XP, capacity entitlements, operation receipts and deployment bindings. World retains wild spawn disposition/movement; Economy retains wallet writes; active simulation is a fenced projection. No new service, character skill, currency, rarity or item kind is introduced.

Protocol identifiers below are proposals until CMP-01 freezes exact schema/authorization/authority adapters. They are not silently added to existing RPC/event enums by this list. Ownership above is selected design authority; schema completion and runtime readiness are separate gates.

| Proposal | Owning contract / merge condition |
| --- | --- |
| Companion `cpn_`; CompanionOperation `cpo_`; Stable keyed by character; entitlements/training/deployment/ledger keyed by owning aggregate and cause | Companion Integration sections 1-2; validate durable uniqueness, realm/privacy participation and restore/retention before prefix/schema registration. |
| Lifecycle `owned`, `released`; separate training, Injury condition and fenced deployment binding | Integration section 2; do not collapse these into one mutually exclusive state field. |
| `mastery/hunting#pack_handling` (tree `mastery/hunting`, node `pack_handling`) | Skills and Mastery section 2.1; uses existing tree/local-node reference convention. Validate content integration in CMP-01; no additional tree slot. |
| Progression companion capture/training/deployment/conditions/XP, expansion quote/purchase, recovery/release/rename and operation-query RPCs | Exact proposed surface/callers/rejections in Integration section 3; freeze messages and permissions before shipping. World capture-disposition adapter and scoped Economy `npc_service` caller authorization are required cross-domain extensions. |
| `progression.companion.captured`, `.training_changed`, `.xp_granted`, `.condition_changed`, `.released`; `progression.stable.expanded` | Integration section 8 owns payloads and existing Progression outbox/topic usage. Register concrete names/envelope/privacy with schema fixtures in CMP-01. |
| `tune.companion.*`, `tune.world.combat_mount_cap_mps`, `tune.combat.companion_*` | Gameplay Details section 11 owns exact typed declarations/defaults/bounds/permissions/adoption. No wildcard runtime authorization implied. |
| Feature catalog: Trained creatures | Gameplay Details section 11 defines dependencies, new-admission stop, engaged drain, emergency suppression and condition-preserving recovery. Exact flag key pending compatible LiveOps registration. |

Creature rarity uses existing `rarity` subset common/uncommon/rare/epic/legendary independently of `npc_tier`. Stable expansion uses existing `currency/gold` and `npc_service` sink, with supported refund semantics required before payment acceptance. Dungeon story NPC helpers remain distinct from owned trained creatures for group scaling. Integration section 9 lists CMP-01-06 delivery gates; these are proposed contracts, not deployed APIs or acceptance evidence.
