# Legends of the Shattered Frontier — Mounts and Companions

Version 0.1 — Implementation design baseline (2026-10-19)

Status: Design only; not implemented or balance-validated. User direction is adopted; numerical values are proposed shipped defaults for implementation/playtesting. Shared Registry remains authoritative; new protocol identifiers and controls are tracked as proposals pending schema integration.

| Part | Owns |
| --- | --- |
| [Gameplay details](Companions/Legends_Shattered_Frontier_Companion_Gameplay_Details_v0.1.md) | Concrete capture/training loop, Hunting milestones, Stable gold prices, family/rarity content, two-pet combat budget, mounted combat, XP eligibility/curve, injury/recovery, UI and complete typed controls. |
| [Integration and delivery](Companions/Legends_Shattered_Frontier_Companion_Integration_and_Delivery_v0.1.md) | Selected durable writer, records/constraints, proposed RPCs/events, capture/payment commits, condition and handoff fencing, lifecycle participation, playable packets CMP-01–06 and targeted verification. |
| [Abilities and animation](Companions/Legends_Shattered_Frontier_Companion_Abilities_and_Animation_v0.1.md) | Ability sources by family/species/rarity/trait, ability-to-animation contract, required pet/mount/lifecycle/signature clip sets, Voidreign legendary Rift Recall and animation completeness gates. |

This entry summarizes the feature; the parts own the detailed rules and resolve the initial open choices. Breeding, creature trade/equipment, hunger, permanent death and flight are outside initial delivery. No crowns/real-money purchase is included.

Links: [World movement](World-Simulation/Legends_Shattered_Frontier_World_Cells_Movement_and_Handoffs_v1.0.md) · [Character progression](Legends_Shattered_Frontier_Character_Progression_Architecture_v1.0.md) · [AI](Legends_Shattered_Frontier_AI_Architecture_v1.0.md) · [Combat](Legends_Shattered_Frontier_Combat_System_v0.1.md) · [Configuration policy](Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md)

## 1. Player experience

Mounts are part of character development: players discover wildlife, capture eligible creatures, train them for use, and develop them as travel and combat partners. Each region offers native wildlife and trainable creatures. A large majority of ordinary wildlife should be trainable across the world, with deliberate exceptions. Being attackable makes a creature a candidate for this system, not automatically eligible: named story actors, protected creatures and encounter-critical bosses can remain excluded through authored content.

Players seek a creature for its family abilities, suitability to their build and individual traits. A familiar common creature remains a viable long-term partner; rarer discoveries offer desirable variation. Not every trained creature must be large enough or anatomically suitable to ride. Rideability and combat eligibility are explicit content capabilities, separate from trainability.

## 2. Capture and training

- Inspecting a creature shows its family, level, rarity, trainability and known requirements or exclusion reason. Capture and training are player activities, not an automatic reward for killing it.
- Capture preserves the creature's current level and authored individual traits. It does not reset the creature to level one or reroll its rarity.
- To satisfy both preserved capture level and the player-level ceiling, a creature above the capturing character's progression level cannot be captured. The UI explains the level requirement before an attempt.
- Capture establishes ownership; training establishes permission to use that individual. A newly captured creature cannot immediately be ridden or commanded in combat. Family-specific handling requirements and demonstrated training unlock the supported uses.
- Training continues through meaningful activity with the player and unlocks family skills and command competence. Existing `skill/hunting` covers capture, training and combat handling, alongside tracking/skinning; see [Skills and Mastery §2.1](Character-Progression/Legends_Shattered_Frontier_Character_Skills_Mastery_v1.0.md#21-hunting-creature-handling-and-combat-companions). Repeated trivial commands and idle deployment do not provide progression. Gameplay Details §§2–3 specify Calm and Bind, three ordered handling lessons and a separate riding lesson; no new character skill is added.
- Capture requires a free Stable slot. A successful capture consumes that slot atomically with removing the wild creature and creating the owned individual. Concurrent attempts cannot duplicate a creature or overfill the Stable. Failed attempts do not consume a slot.

## 3. Stable and capacity

Each character has a **Stable**, a roster of owned creatures, with a shipped default capacity of **2**. Both stored and deployed creatures occupy a slot; deployment does not free a slot for another capture. This is a gameplay roster; a physical stable building is a separate design decision.

Capacity increases through permanent Hunting milestones at 300/700 (+1 slot each) or four sequential Stablekeeper purchases (+1 each, 5/15/40/100 gold using `currency/gold`). Default reachable capacity is 8; hard release maximum 12 accommodates approved base-slot profiles. Both routes use explicit persistent entitlements. Gameplay Details §4 owns exact terms and recoverable purchase behavior. Purchases do not grant combat slots, skill points or creatures.

An expansion offer displays its slot increase and resulting capacity before commitment. Purchase payment and capacity grant must be retry-safe and recoverable without duplicate charges or duplicate slots. A skill-earned expansion remains earned if the character later retrains that skill; capacity does not shrink and strand creatures. Configuration reductions likewise never delete creatures or revoke earned/purchased slots; a migration must preserve existing capacity rights. Removing an owned creature requires an explicit player action and confirmation.

Stable capacity is separate from the active companion limit. Increasing storage does not grant additional simultaneous combat creatures. Baseline: one active companion. Initial Hunting mastery proposal: **Pack Handling** at effective Hunting 700 (**70.0**), purchased as a standard master-tier node for 25 LP with the existing tree prerequisites, raises the limit to **two active combat companions**. Both require completed individual training and occupy Stable slots. Stable purchases cannot grant this combat permission. A ridden trained creature counts toward the active limit, allowing a qualified player to ride one and fight alongside the other without creating an extra actor. More than two active companions is outside the initial proposal.

Deployment and command validation use the current active Hunting node, effective skill and available Stable roster, including after reconnect/handoff. Resets or voluntary skill/spec changes that remove active permission are blocked during combat. Forced corrections remove slot 2's new offensive-action permission while retaining slot 1, damageability and encounter consequences until safe storage. Players may reorder slots out of combat before a voluntary change. Withdrawal cannot heal, revive, delete or duplicate creatures, revoke owned Stable slots or erase progression.

## 4. Creature classification and rarity

Keep these independent dimensions on authored creature content:

| Dimension | Purpose |
| --- | --- |
| Family/type | Shared skills, ability progression, animation and handling rules; illustrative families include equines, canines, felines and reptiles. Species inherit the appropriate family kit. |
| Capabilities | Trainable, rideable and combat-capable are explicit permissions; terrain and movement restrictions still apply. |
| Level | Starting strength and progression requirements; a higher starting level is not automatically a higher rarity. |
| Rarity | **Common → Uncommon → Rare → Epic → Legendary**; the five supported creature rarities, a subset of existing Registry `rarity`. No mythic creature rarity is introduced. |
| Individual traits | Authored variations within the family; harder or higher-level eligible creatures may carry unique traits worth seeking. |
| Encounter tier | Existing Combat `npc_tier`; independent of rarity. Encounter health/damage multipliers and boss immunities are not companion rewards. |

Common creatures supply the complete family foundation. Each rarity has one active individual trait; rarer variants offer distinctive tradeoffs or technique replacements within the same reviewed combat budget. Rarity is not a blanket stat multiplier and does not bypass training or the owner-level ceiling. Gameplay Details §§2–3 own initial spawn weights, coverage floor, family kits and trait rules. Family content, compatibility and encounter-to-companion conversion ship in compatible releases.

## 5. Level and skill progression

The creature starts at its capture level and earns its own XP from validated participation alongside its owning player. Creature XP does not consume the player's XP grant or create a second player kill reward. Stored creatures do not earn activity XP. Use a creature snapshot of the existing character XP curve without LP/prestige, and split one eligible owner-base XP pool among qualifying creatures. Gameplay Details §8 owns source eligibility, split/dedup, sync and curve migration. Creature skills use its family kit/training rather than copying the player's chosen skills.

Proposed catch-up rule, delivered in versioned code:

```text
gap = max(0, owner_progression_level - creature_level)
catch_up_multiplier = 1 + bonus_max * min(gap / full_bonus_gap_levels, 1)
creature_xp = eligible_base_creature_xp * catch_up_multiplier
```

Initial proposal: `bonus_max = 2.0` (up to 3× total XP), `full_bonus_gap_levels = 10`. The bonus diminishes as the creature approaches the player and reaches zero bonus at equal level. For a level-30 player, levels 10/25/29/30 receive 3×/2×/1.2×/1× respectively, before the level ceiling is applied. These are example results, not measured balance.

The owning character's progression level is a hard ceiling. At equality, XP cannot advance the creature toward the next level; excess XP is discarded rather than banked. A large award processes level boundaries, recalculating the bonus on remaining base XP and stopping at the ceiling. Temporary encounter scaling does not change the durable ceiling. Owner-level correction below a creature suspends its use until resolved without silently erasing progression. Integration §7 specifies deterministic arithmetic and source receipts.

## 6. Combat and riding

Training unlocks commands such as follow, stay, engage and disengage, subject to server validation, pathing, range and combat target legality. Creature abilities use Combat's damage, threat, control and PvP rules; ownership does not allow attacks on otherwise protected targets. Mounted abilities and fighting beside an unmounted creature are supported design directions, with each family's permitted actions authored explicitly. A ridden creature is the same individual, never a duplicate deployed actor.

Pack Handling supports coordinated commands and individual target assignments. Gameplay Details §6 proposes single-pet output of 20% reference DPS and dual profiles at 11.2% each (112% combined single-pet output), with shared owner interrupt recovery and canonical target DR. Validate combined survivability/utility as well as damage. Profiles are release content, not adjustable console formulas. Creatures have their own threat, owner-attributed reward credit, and add no player/group scaling slot. Story NPC helpers remain a separate scaling category. Pet count cannot duplicate player rewards, Hunting practice or the creature XP pool.

Travel and mounted combat use World's authoritative movement limits. The old travel-only assumption that any hostile damage always dismounts the rider is superseded for trained combat mounts: ordinary damage alone does not dismount them. Explicit dismount effects, defeat and prohibited movement volumes still force a validated dismount. Non-combat travel unlocks retain their existing damage-dismount behavior. Combat mounting remains out of combat initially; fighting while already mounted does not imply permission to mount during combat.

Gameplay Details §§5–9 specify command/leash behavior, mounted ability admission and 7 m/s combat cap, separate rider/mount targeting, instance policy and Injury/recovery. Defeat is recoverable in a rest area (30-second channel, 50% health/resources); no permanent loss or gold revival fee. Storage/reconnect cannot reset health, conditions or cooldowns.

## 7. Ownership and integration boundaries

Captured creatures are persistent, character-owned individuals with a Stable slot, provenance, family/species, rarity, trait, level/XP and training. Existing account `mount` unlocks remain entitlements; compatible appearances can apply to trained rideable species without creating an individual, transferring character progression or bypassing training/ownership.

World owns wild populations/movement; AI owns decisions/commands; Combat owns ability legality, threat, combined power budgets and outcomes. **progression-service is the sole durable writer for the Stable, owned creatures, training/XP, entitlements, operation receipts and deployment bindings**, alongside owner progression/Hunting. Economy alone writes wallet charges. Integration §§1–8 define records, proposed APIs/events, durable capture against World spawn generations, payment recovery and handoff/lifecycle contracts; no new service is introduced.

Server authority must persist each individual once and fence capture, deployment, storage and zone handoff. Reconnect/restart must preserve training and progression and never leave the same creature active on two servers. Purchase and progression sources cannot write each other's records directly.

## 8. Declared configuration proposals

Gameplay Details §11 is the single control declaration table: proposed keys, domain owners, typed defaults/bounds/units, permissions, review, adoption and rollback. Use shared config-service and plain-language GM/web controls, never console expressions. It also defines the feature catalog dependency/admission/drain behavior. One snapshot covers an operation; rollback never erases earned progression, committed captures or entitlements. Rules, formulas, family/rarity content, mastery gates, purchase catalogue and hard capacity ship through compatible releases.

## 9. Acceptance before gameplay readiness

- A region exposes trainable wildlife and clear authored exclusions; capture preserves the selected individual's level/traits and requires training before use.
- A fresh character has two Stable slots; a third capture fails without losing the wild creature. Skill and purchase expansion each grant the advertised capacity exactly once, including concurrent/retried operations.
- Family abilities remain consistent across species/rarity variants, and a rare trait is inspectable without importing wild encounter multipliers.
- Validated joint activity advances the creature, demonstrates diminishing catch-up, and never crosses or banks XP beyond the owner's level, including a large award.
- A trained creature fights alongside its owner and an eligible combat mount can fight while ridden. Ordinary damage does not defeat that capability; forced dismount and prohibited volumes remain enforced.
- Without active Pack Handling, deploying a second creature is rejected. Effective Hunting 700 plus the active mastery node permits two; 699, an unpurchased/inactive node or a Stable purchase alone does not. A ridden creature counts toward the same limit. Reset/skill loss, reconnect and concurrent deployment cannot bypass it; multi-pet actions do not duplicate practice or player rewards.
- Storage/deployment, restart, handoff, configuration rollback and skill retraining preserve the roster and cannot duplicate individuals or erase capacity rights.

These are future targeted verification requirements. This documentation change supplies no runtime implementation or gameplay acceptance evidence.

Integration §9 adds CMP-01–06 delivery packets and the failure matrix. The next bounded prerequisite is CMP-01's reviewed schema/protocol/arithmetic/authority binding; then CMP-02 delivers the canine capture/train/fight/save loop. Numeric defaults remain playtest hypotheses, not proof of balance or runtime readiness.
