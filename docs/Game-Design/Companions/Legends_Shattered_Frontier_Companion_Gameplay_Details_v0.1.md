# Mounts and Companions — Gameplay Details

Version 0.1 — Implementation design baseline (2026-10-19)

Status: Design only; numerical defaults require playtesting. [Entry](../Legends_Shattered_Frontier_Mounts_and_Companions_v0.1.md) · [Integration and delivery](Legends_Shattered_Frontier_Companion_Integration_and_Delivery_v0.1.md). This part resolves the entry's initial open gameplay choices. Names/controls marked proposed require schema integration before implementation.

## 1. Scope and player progression

Initial scope: regional capture, individual training, persistent growth, character Stable, one baseline active creature or two with Hunting mastery, mounted combat, injury recovery and instance travel. Breeding, creature trading/auctions/mail, creature equipment, hunger/upkeep, permanent death, flight/gliding and multi-seat riding are deferred. No real-money or crowns purchase is included.

`skill/hunting` retains gathering track, Dexterity .6 / Wisdom .4, caps, tracking and skinning. Creature handling adds meaningful practice, not a new character skill. Capture/training difficulty is an authored integer 0–1000. Required effective Hunting is `max(0, handling_difficulty - 100)`; starter wildlife uses difficulty 100. Creature level must not exceed owner progression level. Rarity alone does not add a hidden skill surcharge. Difficulty and requirements are inspectable before an attempt.

| Effective Hunting | Unlock |
| --- | --- |
| 0+ | Novice captures/training where individual requirements allow; one active trained creature. |
| 300 (30.0) first reached | +1 permanent Stable slot. |
| 700 (70.0) first reached | Another +1 permanent Stable slot. |
| 700+ and active Pack Handling node | Two active combat companions; standard master node, 25 LP, at least two expert Hunting nodes. |

Earned Stable slots survive ordinary skill loss and mastery reset. Pack Handling's active slot requires current effective skill and an active node. Its proposed reference is `mastery/hunting#pack_handling` (existing tree `mastery/hunting`, local node `pack_handling`); it consumes an existing standard master-tier position, not an extra node or specialization. The companion-output benefit must fit the whole mastery tree's combat budget even though Hunting belongs to gathering.

## 2. Capture interaction and ecology

Inspect shows trainability/exclusion reason, family/species, level, rarity, individual trait, rideability and minimum Hunting. Build-defining traits are visible before capture. The spawn's level, rarity and trait seed are generated once; inspection, combat, failed capture and leash/reset never reroll them.

**Calm and Bind** is a deterministic capture interaction, not a kill reward or hidden chance roll:

1. Target eligible, unowned wildlife from a capture-enabled population. Protected/story actors, ordinary instance actors and encounter-critical bosses are excluded unless content explicitly provides a capture encounter. Require a free, unreserved Stable slot and current level/Hunting admission.
2. Weaken the creature to at most 30% max health. Dismount and order existing creatures to Passive/Recall beside the owner before channeling. They may remain deployed, occupying their existing slots/conditions; this does not require prohibited in-combat storage. Suppress their new offensive intent for the channel. No foreign character or foreign pet may have participated in this engagement; capture cannot claim another player's ongoing fight.
3. Exclusively reserve that spawn and one roster slot. Channel for 8 seconds, stationary, within 5 m, with line of sight. Target AI continues to defend itself: capture is not free CC. Movement, range/LOS loss, owner defeat, hostile damage received by the owner or their deployed creatures, new damage to the target, target death, foreign engagement or loss of authority interrupts it. Damage-over-time still interrupts; the player must create a real safe opening, not have a second pet tank indefinitely during the channel.
4. Revalidate and commit. Only coherent durable completion removes the wild actor and enables its owned record. The UI shows **Securing creature** while the result is pending; that record cannot deploy. Scheduling the wild replacement occurs through normal World population/respawn rules, once.

Predator content must provide an exhaustion/flee opening that supports the channel; passive species can offer calm windows. A trainable creature that attacks indefinitely with no capture opening fails authoring acceptance. More difficult creatures require encounter tactics and higher handling skill, not hidden dice. The canine pilot must demonstrate an actual hostile capture opening.

Failure releases only the known-aborted operation's reservations; the wild creature keeps its current health and normal AI. Success preserves capture level and compatible individual traits, carries its health/resource fractions into the companion profile (not raw boss HP), starts XP at zero toward the next level, and grants no kill XP, loot, corpse, skinning or encounter-completion reward. Training does not heal it; after basic training, normal deployed recovery applies. A capture-only encounter cannot also pay a kill bundle. Captures do not bypass population caps, make rare respawns immediate or create rewards on release. No bait/trap consumable is required initially; adding one later adds an explicit Inventory transaction participant.

Initial rarity weights for compatible ordinary spawnsets are 6500 / 2500 / 800 / 180 / 20 out of 10,000 for Common / Uncommon / Rare / Epic / Legendary. Remove unsupported rarities and normalize the remainder; this is not a global legendary probability across all wildlife. No owner Luck multiplier. Special regional rare creatures may use authored fixed-rarity schedules. Spawnsets and trait pools are release content; bounded weight profiles adopt only on new spawn creation (§11), never rerolling existing individuals.

Whole-world content coverage requires at least 75% of ordinary wildlife species definitions to be trainable, excluding explicitly tagged protected/story/encounter actors, with a trainable population in each inhabited gameplay region. Report exclusions and actual population availability so tagging cannot hide missing coverage. The tutorial area supplies difficulty-100 wildlife. This is an authoring floor, not a claim about every visible crowd or boss room.

## 3. Training and family techniques

Capture yields **Untrained**. Start personal training in a rest area: the creature appears as a training-only projection that cannot attack, be damaged, scout, block navigation or grant abilities. This projection does not occupy an active combat slot but still occupies its Stable slot. Entering combat ends it and preserves completed lessons.

Three ordered lessons: approach/calm; follow/recall through a marked route; hold/release at a marked station. Each requires an authored objective and a 10-second uninterrupted active interaction. Repeating completed lessons grants nothing. On completion unlock basic commands/attack. Rideable species require a fourth saddle/mount/dismount lesson using the same interaction duration. No saddle item is required initially. Lesson definitions validate actual route/station completion, not just elapsed time or repeated UI clicks.

Lessons can submit Hunting practice under normal gain/anti-macro rules but grant no creature XP or player reward bundle. Creature techniques require one distinct coached use after their level gate; use a safe authored training scenario for first practice. Credit is keyed by creature and technique. Training one creature does not train another.

| Creature level gate | Unlock after individual coached use |
| --- | --- |
| Basic training complete | Basic attack and follow/stay/recall/engage/disengage. |
| 10 | Family signature A. |
| 30 | Family signature B. |
| 50 | Choose one of two family passives; one active. |
| 70 | Upgrade replacing an earlier family technique. |

High-level captures may train all already-eligible techniques in sequence without replaying low-level XP. Swap the family passive at a rest area out of combat, retaining unlocks. During level sync, disable techniques above effective level while preserving training. These gates and the family proficiency-to-combat-skill mapping are release content, not new player skills.

| Family | Shared kit direction | Riding |
| --- | --- | --- |
| Canine: first delivery | Pursuit attack; interrupt using shared owner interrupt recovery. | Authored large wolves only. |
| Equine: second delivery | Short charge; brief owner movement aid. Charge uses World displacement validation. | Compatible horses/regional variants. |
| Ursine: third delivery | Threat-generating swipe; PvE taunt using canonical taunt/DR rules. | Only species with viable rider geometry. |
| Feline: follow-up | Positional strike; short slow. | Large authored species. |
| Reptile: follow-up | Bite/DoT; short defensive technique. | Species-specific, within supported movement modes. |

One active individual trait at every rarity. Common supplies useful family-compatible utility; uncommon/rare offer specialized tradeoffs; epic/legendary can replace a technique with a distinctive variant. Rarity does not stack extra stat slots, add active pets, confer encounter immunity or increase the combat budget. Publish exact coefficients/costs, animation support and trait compatibility before releasing a family. The pilot requires a complete canine kit through its admitted level range, not placeholder ability IDs.

## 4. Stable entitlements and purchases

Default capacity is 2 base + 2 earned + 4 purchased = **8 reachable slots**. Hard release maximum is **12**, accommodating bounded base-slot profiles. Stored, training, injured, deployed and capture-reserved creatures all consume capacity. Stable is character-owned; account UI may display it but cannot share creatures across characters. A physical stable building is optional later.

Stablekeeper purchases grant one permanent slot each, sequentially: 5 / 15 / 40 / 100 gold (50,000 / 150,000 / 400,000 / 1,000,000 copper), using `currency/gold` and existing sink `npc_service`. Prices are initial typed configuration defaults (§11). The quote names the next entitlement and resulting capacity; payment and grant have recoverable receipts. No partial debit, crowns, combat-slot sale or real-money product. Price publication invalidates unaccepted old quotes; completed purchases retain accepted terms.

Base slots become a creation entitlement; changing defaults affects only new Stables. Reconcile the 300/700 milestones from historical Hunting peak evidence when the feature enables, without forcing a veteran to lose/regain skill. Entitlement keys are unique per character/milestone or purchase ordinal; replay cannot award another slot. Fraud/recovery corrections preserve owned creatures and use an explicit repair plan rather than silently shrinking capacity.

Roster operations: inspect, rename under existing text moderation, choose active slots 1/2, train, deploy/store, recover and release. Permanent release is out of combat, unavailable for active/reserved/pending creatures, and requires typed creature-name confirmation. It archives ownership; no reward-bearing wild/tame actor respawns. Realm transfer carries the full roster and entitlements. Character-to-character trading, gifting, mail and auctions are disabled.

## 5. Deployment, commands and withdrawal

Deploy/store is a 6-second stationary out-of-combat channel with ground/nav clearance and no pending transfer. It works in the world; Stablekeeper proximity is needed only for purchases. No swapping while owner or any active creature is engaged. Persistent health/resources, Injury and absolute cooldown deadlines survive storage, login and travel; deployment never heals/refills. Pet combat immediately engages its owner for admission checks. A trained ridden creature counts toward the same active limit; account-unlocked appearances require a compatible trained species and confer no stats, rarity, training or slots.

Controls target slot 1, slot 2 or All: Follow, Stay, Engage, Disengage/Recall and trained signatures. **Passive/Defend** are combat stances, separate from movement orders; default Defend reacts only to a legal attack on owner/creatures and never chain-pulls neutrals. Manual Engage obeys owner target legality. Group auto-taunt is off by default, requiring owner opt-in. Commands replace pending intent rather than queue unbounded casts; each actor separately validates cooldown, resource, LOS and state.

Owner must be alive/connected on the same authoritative simulation. Creature must be within 40 m of owner; ordered target within 40 m of creature and legal ability range. Owner-anchored leash stops offense and navigates home beyond range. Never use wild-NPC evade immunity, full healing or reset speed. If unreachable for 10 seconds, out-of-combat recall may reposition beside the owner only with World displacement validation and free-ground clearance; in combat it remains non-offensive and attempts path recovery without teleport escape/healing.

Loss of Pack Handling retains slot 1 and removes slot 2's new offensive-cast permission immediately; effects already applied resolve normally. Block voluntary node/skill/spec changes removing active permission while engaged. Forced withdrawal cannot erase an attacker/threat consequence: damageability and encounter state continue until safe storage, with condition checkpoint. Player can reorder slots out of combat before a voluntary change. Stored creatures, XP and capacity are never deleted.

Owner death/disconnect stops new offensive behavior and triggers passive return. Existing hostile effects/damage can still defeat a creature. Store only after Combat reports disengagement and the final condition checkpoint is durable; no unattended pet fighting. Reconnect cannot clear conditions or spawn a second actor. Source authority must be fenced before a server-loss recovery projects a replacement.

## 6. Combat output and group rules

Trained creatures use Combat's **character-sourced** formula, own derived stats and trained proficiency plus a virtual natural weapon/implement. Discard wild `DMG_npc`, `npc_tier` multipliers, boss-only techniques and immunities. Baseline reference item level is `4 × creature_effective_level`, independent of owner gear. Authoring calibrates natural-weapon coefficients against the reference build through normal damage/crit/mitigation; never replace final damage after mitigation. No equipment UI in this release.

Let `R(L) = dps_ref(L, 4L)` from Combat/Dungeon reference math. Single-creature sustained output targets **20% × R(L)**. With two active slots, each targets **11.2% × R(its level)**: a 0.56 magnitude factor on its single-creature profile, 112% combined output at equal level. An owner deploying only one uses the normal single profile even with Pack Handling learned. A defeated/unreachable/disengaged second creature retains its combat allocation until legal out-of-combat storage, preventing stance toggles from restoring single-pet damage. Profile changes apply on out-of-combat deployment and are versioned.

The extra 12% companion output consumes the master-tier Hunting tree's combat-output allowance; other nodes/traits cannot add another unreviewed damage multiplier. Gathering-yield budgets are separate. Initial single-profile guardian health target is 50% of the same-level baseline reference character health, other families 35%. Dual profiles apply the same 0.56 factor to reference max health as to output, preserving the creature's health fraction when profiles change out of combat. Thus two same-role creatures have 112% combined reference health, not twice the single-role health. Resources/mitigation are authored under normal level math; no extra dual-mode armor bonus. Two pets still add targetable bodies, so combined survivability, interception, pressure and CC must pass PvE/PvP comparisons; satisfying DPS alone is insufficient acceptance.

The dual factor applies to pet damage/heal/absorb magnitudes before normal mitigation/target modifiers; persistent effects retain application profile. It does not multiply hard-control duration, taunt, percentage defenses or movement. Initial kits have no ally heals, shields, resurrection or group damage buffs. Family utility needs its own combination review. Hard interrupts share a **12-second owner-wide creature cooldown**, across actors/replacements/reconnects. CC retains canonical target DR/immunity and taunts retain canonical threat semantics; two creatures cannot reset DR.

Each creature generates its own threat; commands do not manufacture threat. Damage/healing aggregates to the owner once for kill credit. Creatures are not party members, loot recipients or additional reward grants. Killing a trained creature grants no XP, loot or skinning. PvP attacks inherit owner legality/flags and attribute assists/kills to the owner; creatures never create an exception to safe zones or territory permissions.

Dungeon/raid trained creatures do not add to real-player scaling `n`, damage-equivalent `n_eq`, roster counts, voting or rewards: they are part of the owner's build. Story NPC helpers remain separate and retain their existing 0.3 damage-equivalent contribution. Instance policy can prohibit riding independently of walking pets; incompatible deployment is safely stored on admission out of combat with a visible reason. No normal instance capture. Competitive restrictions must be published mode profiles selected on admission, not ad hoc mid-fight removal. Initial ordinary PvP uses current owner-attributed rules without an extra pet-only multiplier; evidence can justify a later declared profile.

## 7. Mounted combat

Initial rider abilities allowed while mounted: supported basic melee/archery and instant non-ground-target techniques explicitly tagged mounted-compatible in content. Stationary casts/channels, stealth and deployable placement require dismount. Mount anatomy/animation, attack reach and LOS are validated; a content tag alone cannot establish rendering or collision acceptance. On-foot item movement bonuses remain on foot.

While engaged, mounted movement caps at `min(authored_mount_speed, 7 m/s)` and feeds World's authoritative `v_eff` history. Restore travel speed only after normal combat exit. Ordinary damage does not dismount a trained combat mount; explicit dismount effects, defeat, prohibited volumes and deep water do. Mounting itself stays out of combat. Rider/mount are distinct targetable actors; AoE may hit each once without health-sharing or duplicate hits. Dismount uses a validated adjacent ground/capsule point and cannot grant free charge/teleport; if normal placement is obstructed, hold the authoritative pair stationary until a validated placement or existing World recovery route succeeds, rather than ejecting through geometry.

Non-combat account travel unlocks keep the old damage-dismount baseline. Appearance unlocks never create an owned trained individual. Flight/gliding, swimming exceptions and multiple riders require separate compatible movement/security work.

## 8. Creature XP and level ceiling

Use a creature-owned snapshot of the shipped `xp_to_next(L)` curve, captured at introduction and changed only through a compatible content migration preserving level/fractional progress. Capture begins at its original level, zero progress toward next. No LP, owner attribute points, prestige or account catch-up unlocks. Temporary sync uses `min(creature_level, owner_effective_level, instance_sync_cap if present)`; durable owner progression level remains the XP ceiling.

Initial eligible sources: XP-eligible PvE kills and PvE encounter completion bundles. Pool equals the owner's eligible **base** source XP before rested/diversity/account catch-up/event multipliers; capped owners use the pre-prestige amount. Grey/ineligible sources yield zero, even with a low-level pet. Generic quest turn-ins, discovery, PvP, lessons, farming and compensation are excluded. Encounter bundle support must carry the deployment/participation snapshot; swapping before claim cannot redirect XP.

To qualify, a creature must be deployed, alive at encounter completion, within the existing 60 m kill-credit radius, and have resolved damage, meaningful defense or a validated family utility action. Idle proximity is insufficient. Owner must qualify under existing reward rules. Split one base pool evenly between qualifying owned creatures; integer remainder goes by ascending companion ID. A capped participating pet consumes its share; do not redistribute it. One participant gets the full pool. A capture target or training projection never qualifies.

Apply the entry's diminishing multiplier (default up to 3×, full bonus at gap 10), recomputing after every level crossed as remaining base XP is consumed. Use exact reduced rational arithmetic with persisted fractional progress (Integration §7); never round each XP unit or accept client-computed amounts. At equality to owner progression level, discard residual share/progress and permit no progress toward the next level. Source/recipient dedup returns the original outcome. Owner level and all creature XP updates serialize in Progression's consistency boundary; a delayed award uses current owner level at commit.

Example: owner level 30, base pool 100, qualifying creatures at 20/25 → shares 50/50 → 150/100 XP respectively if neither crosses a level. At level 30 a creature discards its share; the other still receives only its own. Record base, multiplier segments, curve/config version, granted and discarded amounts. Owner-level correction below a creature's durable level suspends its use/XP without erasing progression; explicit audited recovery resolves the mismatch.

## 9. Injury, recovery and conditions

Defeat marks **Injured**, ends actions and dismounts the rider. No permanent loss, XP debt, durability wear, revival item or gold fee initially. Corpse/presentation cleanup follows instance/world safety rules without blocking traffic. The individual retains capture history, rarity, level, traits and techniques.

For a stored creature with no unresolved binding/operation, in a rest area out of combat a **30-second** owner-initiated recovery channel clears Injury and restores **50%** health and family resources. Stablekeepers and ordinary rest areas support it; no mandatory physical stable. Interruption costs nothing, retains Injury and grants no practice/XP. Cooldown deadlines remain. No in-combat resurrection and no passive revival. Surviving creatures regenerate only while deployed/alive/out of combat: **1% of maximum per second after 10 seconds without hostile activity**. Stored creatures do not regenerate in the baseline. Owner death does not refresh surviving pets.

Dismiss/redeploy, reconnect/crash and instance entry preserve health/resources, Injury and absolute cooldown deadlines. Encounter reset does not revive or fully heal pets absent a separately published mode profile. Runtime checkpoints need explicit acknowledged-condition recovery; the persistence plan forbids treating stale saves as full health or proof of survival.

## 10. UI and content acceptance

Stable shows used/reserved/total capacity, expansion sources/quotes, active slots, current/capture/effective level, XP ceiling/bonus, family techniques, individual trait, training and recovery. Inspect explains capture exclusion/requirements. Capture distinguishes Pending from Secured. Combat UI provides per-slot health/resource/stance/target, signatures and owner-shared interrupt deadline; All-orders reports which actors accepted. Dual-mode tooltip explains per-pet output reduction. Skill loss explains slot 2 deactivation. Bindings use shared remapping/input contexts; no final keyboard bindings are imposed here.

First content packet: trainable canine, fixed common/rare variants with compatible traits, real capture opening, training route/station/objectives, command and injury feedback, virtual natural weapon, level-10 pursuit technique, and admitted-level coefficients. Fixture spawns can force variants for tests but never replace real spawn distribution. Whole-world coverage is a later acceptance gate.

## 11. Declared controls and publication

All following keys are **proposals** under existing config-service/LiveOps workflow. Progression owns them except World mounted speed and Combat output/health/interrupt controls. Realm scope; existing authorized `PublishTuning`/`UpdateFlag` workflow applies identically in GM/web consoles, not raw GM state editing. Production gameplay changes use LiveOps tier 2 (owning lead + `liveops_operator`); Stable price/capacity and rarity distribution are economy-class tier 3 (also `economy_analyst`). Any numeric change exceeding 25% also needs tier 3. Dev/PTR and staging retain existing tier 0/1 rules. See [LiveOps approval policy](../LiveOps/Legends_Shattered_Frontier_LiveOps_Config_and_Releases_v1.0.md#25-approval-tiers-for-config-changes); no novel permission bypass. No player entitlement grants through configuration. Labels/help describe the effects below. Show effective values to players and version/source to staff. Validate complete compatible snapshots, publish/acknowledge through normal workflow, and audit actor/reason/old/new/version/result. Fallback is last-good configuration with these shipped defaults; missing compatible configuration refuses new feature admission.

| Proposed key suffix (`tune.companion.` unless full key) / label | Type; units; default | Safe bounds; step | Adoption/help |
| --- | --- | --- | --- |
| `stable_base_slots` / Starting Stable size | integer slots; 2 | 2–6; 1 | New Stable only; total entitlements must fit hard max 12. |
| `stable_expansion_prices_copper` / Stable expansion prices | ordered 4-element int64 copper array; [50000,150000,400000,1000000] | Each 10000–2000000; 10000; nondecreasing | New quotes; invalidates unaccepted older-version quotes, never accepted payments. |
| `stable_quote_lifetime_ms` / Expansion quote lifetime | integer ms; 60000 | 30000–120000; 1000 | New quote; expiration does not abort an accepted purchase. |
| `rarity_weights_bp` / Wildlife rarity distribution | fixed 5-element integer weight array; [6500,2500,800,180,20] | Common 5000–9000, uncommon 500–3500, rare 100–1500, epic 0–400, legendary 0–100; step 1; sum exactly 10000 | Next ordinary spawn; filter unsupported rarities and require positive remaining total. Fixed-rarity authored spawns unaffected. |
| `catch_up_bonus_max` / Maximum catch-up bonus | fixed decimal additional XP multiplier; 2.0 | 0–2; .1 | Next source award; zero disables bonus, not XP. |
| `full_bonus_gap_levels` / Gap for full catch-up | integer levels; 10 | 1–100; 1 | Next source award; zero rejected. |
| `capture_health_bp` / Capture health threshold | integer basis points max HP; 3000 | 1000–4000; 100 | New capture; lower means more weakening. |
| `capture_channel_ms` / Capture time | integer ms; 8000 | 5000–15000; 1000 | New channel, never mid-channel. |
| `capture_range_m` / Capture distance | fixed decimal metres; 5 | 3–5; .5 | New channel; LOS still required. |
| `training_step_ms` / Lesson interaction time | integer ms; 10000 | 5000–30000; 1000 | New lesson step; objectives still required. |
| `deploy_channel_ms` / Deploy or store time | integer ms; 6000 | 3000–10000; 1000 | New channel; no combat override. |
| `command_range_m` / Command leash | integer metres; 40 | 20–40; 1 | Next deployment; no wider than replication/ability admission. |
| `unreachable_recall_ms` / Recall path timeout | integer ms; 10000 | 5000–30000; 1000 | New path-recovery episode; no combat teleport. |
| `recovery_channel_ms` / Injury recovery time | integer ms; 30000 | 10000–60000; 1000 | New recovery channel. |
| `recovery_fraction_bp` / Recovered health/resources | integer basis points; 5000 | 2500–5000; 100 | New recovery channel. |
| `regen_delay_ms` / Wait before passive recovery | integer ms; 10000 | 10000–30000; 1000 | New out-of-combat episode. |
| `regen_per_second_bp` / Passive recovery | integer basis points max resource/second; 100 | 0–100; 10 | Next deployment; zero pauses regeneration. |
| `condition_checkpoint_ms` / Active creature save interval | integer ms; 5000 | 1000–10000; 1000 | Next deployment; critical/final transitions still save separately; dirty-crash recovery never trusts this interval as proof of health. |
| `tune.world.combat_mount_cap_mps` / Mounted combat speed | fixed decimal m/s; 7 | 5–7; .5 | Next combat entry; update authoritative movement history. |
| `tune.combat.companion_interrupt_cooldown_ms` / Shared creature interrupt recovery | integer ms; 12000 | 12000–30000; 1000 | Next interrupt; cannot shorten existing deadline. |
| `tune.combat.companion_single_output_bp` / Single creature reference output | integer basis points reference DPS; 2000 | 1000–2000; 100 | Next deployment; family profiles still require calibration/validation. |
| `tune.combat.companion_dual_total_bp` / Two-creature combined output | integer basis points of single-creature budget; 11200 | 10000–11200; 100 | Next deployment; each actor uses total/2 of the single profile; whole-tree combat budget still enforced. |
| `tune.combat.companion_health_bp` / Creature reference health | typed role map, integer basis points reference health; guardian 5000, others 3500 | Each 2500–5000; 100; complete supported role set | Next deployment; preserve current health fraction, never revive Injury. |

One snapshot covers a whole channel/source award. Cross-field/content validation requires a supported capture opening at least as long as the configured channel, compatible family profile/reference output, a complete health-role map, normalized positive rarity distribution and capacity within hard max. Active-profile adoption of health/output tuning waits for legal out-of-combat reprojection and preserves condition fractions/cooldown deadlines; it is not an in-combat stat refresh. Rollback changes future adoption, never rewrites committed XP, captures, entitlements, payments or active deadlines. Existing base/curve/control snapshots survive restore. Mastery prerequisites, active limit, dual-profile algorithm, purchase entitlement catalogue/milestones, supported rarity/trait topology, family kits, XP eligibility and hard cap are typed release-bound content/rules. No independent combat-slot slider, editable formulas or per-player hidden rarity modifiers.

Feature catalog candidate **Trained creatures** uses existing LiveOps availability semantics, dependent on persistence/authority, compatible content, Progression, Combat, World, AI and UI. Normal disable stops new captures/purchases/training/deployment, lets existing engaged creatures drain, then stores them with conditions. Inspection, recovery and coherent pending-operation completion remain available. Emergency stop blocks new offensive actions/admission but preserves damageability, receipts, threat consequences and safe withdrawal; it is not a heal/deletion/refund. Re-enable revalidates revisions. Test in an isolated realm before compatible realm activation, never different combat rules for random players in the same encounter.
