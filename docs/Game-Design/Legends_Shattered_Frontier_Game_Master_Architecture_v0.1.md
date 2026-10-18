# Legends of the Shattered Frontier - Game Master Architecture
## Concept v0.1

## 1. Purpose

The Game Master (GM) is a player-like actor with explicit tools for interacting with the live game world. It can observe the world and take authorized actions across gameplay areas: for example, spawning a monster, constructing or altering a structure, changing weather, adjusting an NPC, or applying an approved change to a player's stats.

The GM also has tools to govern what actions are available in a world, region, event, or other defined scope. It may enable a construction action that normal policy currently blocks, or apply a temporary rule that changes how an existing system behaves. This makes the GM a cross-domain game capability, not just a rules evaluator or a construction feature.

This contract defines authority boundaries, operators, capabilities, and design intent. Exact Unreal classes and wire schemas belong to implementation specifications.

## 2. Actor model

The GM has a distinct, visible, non-combatant in-world actor identity and a dedicated tool interface. It can move and interact with the world as an actor; privileged world actions are issued through explicit tools and attributed to that identity. The GM cannot participate in player combat, earn rewards, affect progression by normal play, or be targeted by ordinary combat. Where an action materially affects players, the interface identifies it as a Game Master action.

The GM is not an unrestricted database editor. Its tools invoke the owning gameplay domains, which validate and commit world changes. A GM capability authorizes a class of requested actions; it does not transfer ownership of the affected domain's state.

The GM supports human and AI operators. Both use the same capability definitions, domain validations, and audit requirements. AI operation requires a human-authorized, scoped capability grant; the AI cannot grant itself new capabilities or approve its own high-impact action. Human operators use an authenticated staff role and receive the same scope checks.

## 3. Capability model

GM tools are grouped by capability and granted with explicit scope. Capabilities are versioned, data-driven definitions rather than arbitrary executable scripts.

### 3.1 Capability families

- **Observe:** inspect relevant world, region, actor, entity, structure, claim, event, and rule state.
- **World interaction:** alter weather or other permitted world conditions; initiate or modify approved world events; request region or terrain operations.
- **Entity interaction:** spawn, remove, reposition, or configure monsters and other world entities within approved archetypes, counts, locations, and lifetimes.
- **NPC interaction:** inspect NPC state and apply approved adjustments to stats, behavior parameters, faction context, or encounter role.
- **Player interaction:** inspect player state and request approved stat or state adjustments through progression, combat, inventory, or other owning domains.
- **Construction interaction:** sculpt terrain, place, repair, alter, or remove structures and functional objects through Construction, World Simulation, and Building and Territory operations.
- **Rule governance:** activate, modify, or expire scoped rules that allow, deny, condition, or modify gameplay actions.
- **Review and recovery:** preview effects, inspect action history, deactivate temporary changes, and request supported rollback or compensation.

Each concrete tool defines its targets, accepted values, validation, scope, risk, failure behavior, and player-visible effects before it is made available. A tool is unavailable if any of those contract fields are missing.

### 3.2 Action limits

The numbers below are the Standard launch profile. Operational changes require explicitly declared controls under [Tuning and Configuration](Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md); an undeclared limit is release-bound. No capability can edit a formula, algorithm, security invariant, or voxel/storage topology. In-game tools select shipped presets and bounded values, with the same publication and approval path as the web admin console.

- **Entity spawning:** only registry-approved entity definitions can be spawned. Each spawn request is limited to 10 entities; a region may have at most 20 GM-spawned active entities per square kilometer. Public-world GM-spawned entities grant no loot, XP, quest credit, or faction influence unless an approved event definition explicitly grants them.
- **NPC adjustment:** direct raw-stat writes are prohibited. The GM selects an approved encounter profile or a temporary multiplier between 0.5× and 2.0× the canonical profile for at most 60 minutes. Any wider adjustment requires a content-authored event profile and high-impact review.
- **Player adjustment:** temporary combat/stat modifiers are limited to ±10% of derived combat output for at most 60 minutes. Durable progression changes must use existing progression grant or correction operations, have a reason, be visible in the character history, and be reviewed by a second authorized human in public persistent realms. Direct writes to base or derived stats are prohibited.
- **Construction and terrain:** GM actions use the construction volumes, brush limits, claim rules, and persistence limits in Construction System Architecture. A GM capability can enable a normally restricted action only in its declared volume and time window.
- **Weather and world conditions:** a direct GM override lasts at most 60 minutes and selects an approved weather preset. Longer changes must be part of an authored world event.
- **World events:** the GM can start, stop, or stage only approved event definitions. Each event definition declares its eligible regions, schedule, entity budgets, reward policy, and cancellation behavior.

### 3.3 Capability scope

Every grant specifies the narrowest practical scope, such as a target entity, claim, region, world instance, event, or realm. Grants may be limited by action count, value range, duration, affected-player count, or allowed content definitions. A capability outside its scope is denied, even when the operator otherwise has GM access.

Temporary grants and rule changes have explicit expiry or removal conditions. Durable changes require a domain-supported reason and audit record. Capabilities cannot grant themselves broader scope. The default grant is limited to one world instance and one tool family; realm-wide grants require a separate staff authorization.

## 4. Cross-domain authority

Each gameplay system remains authoritative for its own state and invariants. GM tools request actions from those systems and receive success, denial, or a validation result.

| Action | Owning authority | GM boundary |
| --- | --- | --- |
| Spawn or remove a monster/NPC | World Simulation and AI | GM selects an allowed entity definition and bounded placement/configuration; the owning system validates population, location, and simulation rules. |
| Adjust NPC stats or behavior | Combat, Character Progression, AI | GM requests supported overrides or templates; canonical formulas and NPC behavior ownership remain with their domains. |
| Adjust player stats or progression | Character Progression and Combat | GM uses explicit, bounded domain operations with a reason; it cannot directly rewrite derived stats or bypass progression invariants. Temporary modifiers and durable corrections must be distinguishable. |
| Build or modify structures | Construction and Building and Territory; World Simulation for terrain | GM obeys tool scope and persistence limits; owning domains validate claims, materials or authorized exceptions, structural state, and terrain bounds. |
| Change weather or trigger world events | World Simulation, with AI/LiveOps as applicable | GM selects approved conditions/events and duration; simulation and event rules remain authoritative. |
| Change action permissions | Owning domain plus rule governance policy | GM can change only explicitly granted scopes and may not override security, persistence, or protected-world invariants. |

The Shared Registry remains authoritative for canonical service, entity, enum, event, and field names. This concept proposes no registry additions.

## 5. Tool action lifecycle

1. **Identify:** establish the GM actor, operator/process, session, and active capability grant.
2. **Resolve scope:** identify target entities and affected players, structures, regions, and systems.
3. **Preview:** for consequential actions, show expected changes, rule conflicts, costs, duration, and affected scope before execution.
4. **Authorize:** validate the capability, its scope and limits, and any required review or approval.
5. **Execute:** call the owning gameplay domain, which validates current state and commits the action.
6. **Record:** retain the GM identity, operator/process, capability, reason, target, request, result, affected state/version, and time.
7. **Notify or recover:** communicate material player-facing changes and support expiry, rollback, or compensation when the owning domain allows it.

No partial mutation occurs when authorization or domain validation fails. Retrying a tool action must not duplicate a spawn, material grant, stat change, or construction edit.

## 6. Governance and rule changes

Rules compose from global defaults, world or region policy, claim permissions, actor permissions, and event or phase context. Composition must be deterministic and versioned. A specific regional or event rule changes only the declared action types and scope.

The GM may expand what players can do, not only restrict them. Examples include temporarily allowing terrain shaping in a protected restoration zone, enabling a new build material in one event, or opening a normally inaccessible area for a supervised construction activity. Expansion cannot alter security invariants, persisted data ownership, or protected narrative landmarks. It cannot enable an action for which the owning domain has no validation and persistence support.

Rule changes must declare:

- Rule identity and version.
- Allowed action or policy effect.
- World, region, event, claim, or actor scope.
- Preconditions and conflicts.
- Start time and expiry or removal condition.
- Player-facing explanation where gameplay is materially affected.
- Rollback target and expected recovery behavior.

Precedence and invariants:

1. Security, authentication, exploit-prevention, and data-integrity invariants are never overridden by GM policy.
2. Owning domains retain validation authority over their state.
3. Protected-world and persistence constraints are never overridden by GM tools. The only exception is a content-authored event that explicitly marks an editable region and supported action set before publication.
4. GM changes apply only to declared scopes; unspecified interactions are denied and produce operator diagnostics.
5. Conflicting rules do not resolve through undocumented last-write-wins behavior.

## 7. Risk tiers and review

Tool actions should be classified by impact rather than by operator identity alone:

- **Low impact:** read-only inspection, preview, and temporary non-persistent presentation changes.
- **Moderate impact:** bounded entity spawning, local weather changes, reversible construction, or temporary rule changes.
- **High impact:** player stat/progression changes, persistent structure or terrain edits, claim/ownership changes, large-scale spawns, or changes affecting many players.

Required controls are fixed as follows:

- **Low impact:** read-only inspection and previews. No approval is required; access is still logged.
- **Moderate impact:** bounded entity spawning, weather changes, reversible construction, and temporary rule changes. The tool shows a preview, validates scope and quotas, and records execution. A public event notice is required whenever players can observe or be affected by the change.
- **High impact:** durable player progression/stat changes, claim or ownership changes, structure destruction outside a declared siege, large-scale spawns, and rules affecting more than one region. A second authorized human must approve the exact preview before execution in public persistent realms. These actions are disabled in public realms unless a domain defines a safe rollback or compensation path.
- **Development and private test worlds:** an owner may grant broader ranges and skip second-person approval, but actions remain scoped, attributed, domain-validated, and recorded. Test grants cannot be transferred into public realms.

The preview is bound to the target state version and expires after 60 seconds or any relevant state change. If the target changes, the action must be previewed again.

## 8. Player experience and trust

When GM actions affect players, the game should make meaningful effects understandable. Players should be able to distinguish a world event from an ordinary simulation result where that distinction affects trust, competition, rewards, or persistent character state.

Player stat or progression changes need a clear source and reason. GM-created monsters and encounter changes must follow the world event and reward rules so they cannot silently become an unfair source of loot, progression, or faction influence. Construction exceptions must communicate their location, scope, and duration.

The audit trail is available to authorized support and operations roles. Player disclosure and access to detailed records are governed by the relevant privacy and security policies.

## 9. Unreal and multiplayer considerations

- The server is authoritative for GM identity, capability checks, action validation, and world mutations.
- Clients may present GM tools and previews but cannot commit privileged actions directly.
- GM tool execution must target the correct world/zone authority and handle handoffs or stale target state safely.
- High-volume edits, entity spawns, and terrain operations need explicit batching and limits.
- Tool results should distinguish authorization failure, rule denial, stale state, and domain validation failure.
- Persistent mutations use the owning domain's normal persistence and recovery path; GM actions do not create a shadow state store.
- The GM is a cross-domain capability layer, not a new gameplay state owner. The server-side world session authenticates the GM actor and dispatches its tool requests to owning domain APIs. Security owns capability issuance and revocation; each gameplay domain owns validation, mutation, persistence, and replication of its state.

## 10. Related system documents

- [Tuning and Configuration](Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md) defines live controls, release-bound rules, plain-language GM/web console presentation, and config-service publication. Persistent realm tuning is a configuration operation; a temporary local GM event is a domain action with an expiry. Neither surface bypasses the other domain's authority.

- [Construction System Architecture](Legends_Shattered_Frontier_Construction_System_Architecture_v0.1.md) defines terrain shaping, structures, comfort, environmental damage, automation, claims, and raids.
- [Building and Territory](Legends_Shattered_Frontier_Building_and_Territory_System_v1.0.md) owns claims, permissions, settlement progression, upkeep, and sieges.
- [World Simulation](Legends_Shattered_Frontier_World_Simulation_Architecture_v1.0.md) owns terrain, active world simulation, NPC placement, weather, and world events.
- [AI](Legends_Shattered_Frontier_AI_Architecture_v1.0.md) owns NPC/faction behavior and world-director decisions.
- [Combat](Legends_Shattered_Frontier_Combat_System_v0.1.md) owns canonical attributes, derived stats, NPC stat generation, combat outcomes, and player/NPC parity rules.
- [Character Progression](Legends_Shattered_Frontier_Character_Progression_Architecture_v1.0.md) owns levels, skills, attributes, progression state, and progression rewards.
- [Security and Anti-Cheat](Legends_Shattered_Frontier_Security_and_Anti_Cheat_Architecture_v1.0.md) owns operator identity, staff authorization, exploit prevention, audit and rollback policy.
- [Shared Registry](Legends_Shattered_Frontier_Shared_Registry_v1.0.md) owns canonical names and event contracts.

Each system document should describe its own domain's validation and any specific GM action effects. This document is authoritative for the cross-domain GM actor, capability model, and tool lifecycle.

## 11. Capability administration and recovery

Capability grants are issued by an authenticated staff administrator, bound to an operator identity, world scope, tool family, and expiry. Default expiry is one hour. Grants may be revoked immediately; revocation cancels uncommitted previews and pending approvals. No GM action can modify its own grant.

Rollback follows the owning domain's supported semantics. Temporary modifiers, weather overrides, and event flags are removed by expiry or explicit cancellation. Persistent terrain, structure, NPC, and progression changes use a compensating domain action that records the original and replacement state. When safe rollback is impossible, a second approved compensating action is required and affected players are notified.

Every committed mutation and rejected high-impact request is recorded with operator, GM actor, capability grant, reason, target, preview version, result, domain receipt, timestamp, and rollback/cancellation link. Security and privacy retention rules govern storage and access.

This document defines the Game Master design target. Unreal implementation details belong in the project architecture and implementation specifications.

## 12. Scoped player and instance recovery

Status: user-requested design target (2026-10-18), not an implemented tool or approved domain contract. The user explicitly confirmed that GM revival should restore the gear damage caused by that death. This requires durable evidence before persistent gameplay schemas are finalized.

### Player incident recovery

In the open world, recovery targets one player's specific death incident. It does not rewind the world clock or replace a character with a historical snapshot. Combat performs its supported GM resurrection, retaining the existing dead-state and player-acceptance requirements (currently 60 seconds); World owns safe placement through its supported resurrection path. The preview and execution bind to the current authoritative death generation, authority and relevant item versions. A later resurrection, new death or authority change invalidates stale approval.

Equipment already owned by the player keeps its identity. Inventory corrects only the actual, verified durability loss still attributable to that death, including the appropriate broken/active state and equipment-cache refresh. It must not simply restore an old item snapshot or add the nominal death percentage. If death damage was already repaired, a second correction must not erase later combat wear. Partial repairs, upgrades, changed maximum durability and clamped/zero wear require causal domain evidence; ambiguous corrections are unavailable pending review. Refunding repair costs is a separate approved action.

Preserve later legitimate repairs, trades, other players' ownership and unrelated combat/PvP records, sanctions, progression, cooldowns, effects and consumable use. Transferred, escrowed, destroyed, consumed or salvaged items require their owning domain's supported resolution; this tool cannot clone or claw them back. Existing item-restoration identity/provenance rules remain authoritative. Revival with death-wear correction does not imply a general lost-item or currency grant.

### Instance checkpoint recovery

An instance operation targets an owning-domain-supported checkpoint or encounter boundary and previews every affected participant. It is not an arbitrary timestamp slider. Fence admission, transfers and the old simulation/allocation authority, and drain or reconcile outstanding outcome/reward messages before recovery. Revalidate membership and authority before applying the plan; refuse a boundary that cannot be reconciled safely.

Recovery preserves exported loot, grants, lockouts, trades, quest/progression and wallet history. Existing per-attempt reward idempotency alone is insufficient: a new attempt can create a new reward key. The Dungeon and Rewards owners must define a prior-defeat/recovery-lineage eligibility fence, including modes without lockouts, before this tool is available. Player cooldowns remain unchanged under current boss-reset rules. Mythic elapsed time, death penalties and scoring/record eligibility require explicit domain policy; no silent penalty erasure. Cross-boundary economic corrections require separately approved compensating actions.

### Durable prerequisites and management workflow

Combat death/resurrection telemetry is currently best effort, so it cannot authorize recovery. Required owner contracts must establish durable incident references, equipped-item versions, actual penalty receipts and their causal links. Seal and acknowledge the original wear batch before correction, and link correction/deduplication so delayed or redelivered wear cannot apply the death penalty again. Retention eligibility must be declared by the owners; evidence outside that boundary cannot be guessed.

Use a frozen approved plan with scoped expected revisions, incident/item idempotency, narrow mutation fences and durable per-step receipts. Concurrent GMs and retries cannot recover the same incident twice. Partial completion resumes remaining steps: a crash after revival must not resurrect again while retrying wear correction. Report partial status until every intended effect is verified; this is not distributed atomic rollback or direct database editing.

The management interface shows the incident/checkpoint, affected players/items, proposed corrections, preserved effects, conflicts, prior recovery and safe placement. Require a reason/support case, current capability and the existing section7 approval policy for high-impact public persistent changes. Recovery follows owning-domain compensating entries with operator, approval and receipt audit links. Security/LiveOps support routing must be agreed rather than repurposing enforcement cases or QA grants. Any configurable operating value needs the shared typed declaration, bounds, units, defaults, permissions and effective-time rules before exposure; no new recovery defaults are implied here.

### Owner gates and acceptance

Combat/World, Character, Inventory/Itemization, Security, Dungeon/Rewards and applicable LiveOps owners must approve their contract extensions; protected documents remain unchanged. Before implementation acceptance, cover duplicate/concurrent requests, stale death/authority, consent and safe placement, delayed wear, partial/full repair followed by later wear, upgrades/max-durability changes, ownership/lifecycle conflicts, partial crashes/retry, old-authority late outcomes and repeat instance rewards after a new attempt. Verify preserved economic/scoring history and denied unauthorized requests. These are required fixtures, not executed tests.
