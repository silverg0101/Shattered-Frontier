# Legends of the Shattered Frontier — Tuning and Configuration Architecture

Version 0.1 — Project design contract

## 1. Policy and ownership

Adjustable balance and operating values use declared configuration instead of scattered code literals. Formulas, algorithms, validation, authorization, and storage topology remain in versioned code and content. Operators choose supported values and presets; they never write expressions, scripts, SQL, or formula text in a console.

This policy applies to every gameplay domain. Domain documents own gameplay behavior and default values. This document owns the classification and presentation of adjustable controls. [LiveOps Config and Releases](LiveOps/Legends_Shattered_Frontier_LiveOps_Config_and_Releases_v1.0.md) remains authoritative for configuration publication, targeting, approvals, propagation, kill switches, and rollback. Its `config-service` is the single source of runtime tuning. [Shared Registry](Legends_Shattered_Frontier_Shared_Registry_v1.0.md) remains authoritative for canonical keys, constants, roles, and events. Canonical construction service and typed control identifiers are registered through [Construction Integration](Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md); this policy introduces no separate configuration service.

## 2. What can change

| Class | Examples | Delivery and operator control |
| --- | --- | --- |
| Runtime balance | Upkeep rate, weather frequency, weather wear rate, repair cost scale | Typed, bounded inputs and approved presets through config-service; validation logic stays shipped. |
| Scheduled policy | Upkeep grace, siege notice/window, settlement unlock costs | Versioned configuration applied prospectively at the next eligible cycle; show affected schedules before publication. |
| Authored content | Recipe ingredients, material profiles, weather presets, event definitions | Validated content versions; public consoles select approved profiles. Creating a new recipe/profile follows content release and economy review. |
| Release-bound rules | Support/load algorithm, comfort formula, damage pipeline, recipe quality formula, voxel resolution, topology, transaction semantics | Versioned client/server release and migration where necessary. Consoles cannot change these. |
| Non-negotiable invariants | Permission checks, transaction atomicity, resource conservation, security limits, protected ownership, minimum wear protection | Code-enforced; no live override, including GM tools. |

A numeric value is not automatically live editable. For example, voxel cell size is a storage and collision contract, while storm frequency is a balance control. Domain declarations must explicitly opt a value into live tuning. Undeclared values are release-bound.

Compatibility with existing specifications: a value's historical `tune.*` name does not grant live edit permission. The LiveOps statement that domain constants have tuning keys is a naming/distribution convention; formula coefficients, formula tables, and invariant constants keep their canonical names but are release-controlled and absent from editable console forms. Existing runtime declarations that permit such edits must be restricted before the corresponding console control ships. LiveOps remains authoritative for transport and publication; this policy determines which values qualify for operational editing.

Player-facing formula or topology changes require a compatible client and server update, minimum-build gating, and migration for affected saved data. A server-only implementation fix may ship as a server release when it preserves the client contract. A console publication cannot substitute for either release.

## 3. Required control declaration

Every exposed control has: owning domain; canonical key or a clearly marked registry proposal; plain-language label; help text describing player effect; type; units; launch default; permitted range or enum; step size; safe fallback; scope; apply time; operator permission; approval tier; incompatible combinations; client visibility; and observability/rollback criteria. Declarations and bounds ship with reviewed code/content. Values are immutable configuration versions.

Bounds cannot be widened by a console user. A release must introduce broader bounds and compatible validation. Cross-field checks prevent combinations that are individually legal but unsafe, such as a grace period shorter than required player notice. Defaults documented in Construction and Building are launch profile values, not duplicated code constants. Consumers read one effective snapshot per operation.

Profiles supply named presets such as Standard, Gentle weather, and Harsh weather. Each preset expands to explicit values displayed before publication. Presets do not stack multipliers secretly. The operator sees the final effective value and the source of each override. Existing LiveOps overlay resolution remains authoritative; domain event modifiers compose once under shipped domain rules.

## 4. Console responsibilities

| Surface | Controls and intended use |
| --- | --- |
| In-game GM console | Inspect effective rules for the selected object/claim/region; choose approved weather or event presets; issue scoped temporary actions through GM capabilities. No raw configuration tables or formula editors. |
| Web admin console (`admin-portal`) | Author configuration drafts within declared bounds, compare profiles, preview affected populations/economy/schedules, obtain approvals, schedule canary rollout, inspect acknowledgments, roll back. |
| Player/guild management | Configure only domain-granted local choices, such as cache access, upkeep payment source, and eligible siege availability. These choices cannot change realm tuning or hard limits. |

Both operator consoles use the same authorized configuration and domain workflows. An in-game GM with configuration rights submits the same draft as the web console; it cannot bypass approvals. Regional weather/event actions use the owning domain's approved event scope; they do not invent regional cohort selectors absent from the LiveOps targeting contract. Realm-wide persistent tuning uses config-service.

## 5. Plain-language interaction

Show controls by task: Settlement upkeep, Weather, Building wear, Repairs, and Siege scheduling. Display real units such as days, percent of standard cost, storms per day, and minutes. Hide internal key names behind an optional technical-details view. Explain the effect next to the control, for example: “Building wear: 50% — exposed buildings lose condition half as quickly; storms still occur.”

The default screen offers presets and a small set of bounded controls. Advanced fields retain the same labels, units, validation, and permissions. Never expose a text field for a formula. “Restore Standard” creates a previewed draft, not an immediate destructive reset. Invalid entries show the allowed range and reason.

Before publication show current and proposed effective values, affected realm/region, start/expiry, expected cost or schedule examples, approval requirement, and reset behavior. Claims at risk of entering a different upkeep state must be counted and listed for authorized operators. A change affecting player obligations generates player notice through the owning domain.

## 6. Initial tuning controls

These player-facing declarations map to the ratified keys and complete type/approval/application contracts in [Construction Integration §9](Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md#9-canonical-construction-configuration-declarations). Domain launch profiles supply the default base rates and durations. Standard multipliers are 100%; the release defines the formula that applies them.

| Label | Permitted values | Scope and application |
| --- | --- | --- |
| Upkeep cost | 0–200% of standard, steps of 5% | Realm profile; next billing cycle, never retroactive debt. Zero explicitly displays “Upkeep charges disabled.” |
| Weather frequency | 0–200% of standard, steps of 10% | Realm climate profile; next weather selection, no abrupt replacement of an active storm. |
| Weather severity | Mild / Standard / Severe | Approved weather profiles only; next weather selection, bounded by biome's shipped profile limits. |
| Environmental wear | 0–200% of standard, steps of 5% | Next simulation interval; accumulated condition is preserved and the non-destructive wear floor remains enforced. |
| Neglect wear | 0–200% of standard 1 condition per real day, steps of 5% | Delinquent claims only, next interval; grace deadlines, archive notices and support protection are unchanged. |
| Repair material cost | 50–150% of standard, steps of 5% | New repair quote; committed receipts retain the previous quoted cost. |
| Upkeep grace | 7–30 real days, whole days | New delinquency cycles only; existing claims retain their granted deadline. |
| Siege notice/window | Approved schedule profiles only | New declarations only; existing published windows remain fixed. Profiles must preserve defender notice and regional accessibility rules. |

An emergency pause is a domain kill switch under LiveOps policy. Setting wear to zero does not heal buildings. Raising upkeep does not charge old periods. Reducing grace does not shorten an already granted deadline. Changing a recipe never consumes materials from an existing plan automatically.

## 7. Activation, consistency, and recovery

Use LiveOps draft, validation, review, canary, publish, acknowledgment, and rollback semantics. No console writes directly to a running server's private setting. Every mutation records actor, approvers, reason, scope, prior/new versions, effective time, expiry, and result.

The existing LiveOps `apply_on` values remain unchanged (`immediate`, `next_instance`, `next_reset`, `restart`). Billing-cycle, weather-selection, and new-declaration boundaries in this document are domain adoption rules after configuration delivery, not additional transport enum values. For example, an `immediate` snapshot can carry a rate that the billing domain adopts only for the next cycle.

Construction and crafting capture the effective configuration version at quote/preview and revalidate it at commit. If a cost-affecting version changes, refresh the preview and require a new commit; do not silently charge more. Active sieges, paid billing cycles, and committed operations retain their original version. Weather transitions happen at preset boundaries. The same server-side snapshot determines both preview and commit rules.

Rollback publishes a new version restoring previous values; it does not rewrite billing receipts, heal damage, resurrect destroyed objects, or undo completed transactions. Those outcomes require supported domain compensation. Temporary overrides return to the current underlying approved profile on expiry. If the control plane is unavailable, consumers use LiveOps last-good-snapshot/fail-safe rules and refuse new publications.

## 8. Review and acceptance

Before exposing a control, verify that its default reproduces the approved domain design, bounds preserve invariants, units are understandable, the effective value is inspectable in both consoles, expiry restores the underlying profile, and rejected or replayed requests cannot mutate state. Domain verification must cover incompatible values, stale previews, concurrent operations, restart/reconnect, and rollback where those paths are implemented.

This is a design contract; these checks describe required implementation evidence, not tests run for this documentation change. Adding a control without its declaration, domain validation, or audit path is incomplete work.

## 9. Feature Control catalog and management

User requirement: provide one Feature Control area for major systems and activities that have a meaningful enable/disable boundary, together with their approved settings and testing controls. This is a design requirement, not an implemented interface. It uses the existing admin-portal/config-service and authorized GM workflow; LiveOps remains authoritative for flag schemas, targeting, publication, approvals and emergency switches. No parallel flag store or new canonical keys are introduced here.

### 9.1 Eligible features and declarations

Every new major feature must be assessed for this catalog. Register independently switchable behavior, or record why it requires a compatible release instead. Broad foundational systems need not have a master on/off switch. Existing domain safety switches remain in force even when they are not presented as a top-level Feature Control card.

Each eligible entry links its domain-owned availability flag and declared tuning/profile controls. In addition to section3, declare: feature owner and description; dependencies and incompatible combinations; supported test scope; minimum compatible builds; behavior when enabled or disabled; treatment of active participants, instances, published schedules and committed outcomes; any player notice; and whether an emergency stop differs from a normal scheduled change. Unsupported properties remain read-only or release-bound. Feature dependencies and transition algorithms ship in reviewed code/content; operators cannot edit them as expressions.

| Candidate card | Management purpose | Required boundary before implementation |
| --- | --- | --- |
| New feature rollout | Keep availability off, expose to supported QA/test targets, inspect approved settings, then promote through the existing rollout workflow. | Separate temporary rollout gates from permanent safety switches; retire temporary gates through the existing review policy. |
| Weather | Control availability of weather transitions, select approved presets and expose declared bounded weather properties. | World owns transition/fallback behavior; normal changes respect preset boundaries and existing obligations/effects. |
| Combat (owner request 2026-10-20) | One card with four switches. (1) Master combat availability per realm: whether new engagements may start. (2) Hostile NPC spawning per zone: World `spawn_groups` of `spawn_kind` `npc` with hostile definitions go `disabled`. (3) PvP per mode: the PvP modes card below. (4) Read-only display of each zone's `protected` areas (Zone Definition Standard Z5): combat is never allowed there, from the zone record and not from a live switch. | Combat owns the in-progress rule. Turning combat off stops new engagements; engagements already running end through a declared transition that neither heals, resets threat nor deletes anything. Offensive actions are suppressed. Players get notice, and the switch never offers an instant combat escape. World owns slot `disabled`/`respawning` behaviour for hostile spawns. The master switch is a safety control, separate from rollout gates. |
| PvP modes | Control access to each supported PvP mode and expose that mode's approved controls. | Combat/territory owners define mode scope, existing-combat behavior, notice and exploit prevention; no new PvP mode or instant combat escape is implied. |
| Siege events | Control new siege scheduling/eligibility and select approved scheduling profiles. | Existing published windows and committed outcomes remain governed by territory rules; active-event cancellation needs its own supported domain transition. |
| Trained creatures | Control new creature capture/training/deployment admission and select compatible companion profiles. | [Companion Gameplay §11](Companions/Legends_Shattered_Frontier_Companion_Gameplay_Details_v0.1.md#11-declared-controls-and-publication) owns typed controls/dependencies, pending-commit completion, engaged drain, safe storage and emergency offensive suppression without deletion/heal/threat reset. Flag/schema registration remains CMP-01 work. |
| Other optional events or activities | Add catalog entries when the owning domain provides an independent admission/start boundary. | Explicitly define dependencies, in-progress behavior and recovery before exposing the switch. |

These are catalog candidates, not registered flag names or approved new property bounds. Domain declarations and any Shared Registry proposals must be completed before controls become editable.

### 9.2 Management view and test access

Present searchable feature cards with a plain-language purpose, owner, effective availability, target environment/realm/instance, effective configuration version and approved settings. Detail views show proposed versus applied state, pending activation, propagation acknowledgments, dependencies, active work affected, history, rollback/expiry behavior and the precise meaning of Disable. A disabled feature exposes the appropriate unavailable state to players; enforcement remains at the server authority boundary.

The UI may summarize availability as Off, Test access or Live. These are presentation labels over existing flag values and targeting, not a new stored state machine. Emergency kill state and pending changes must be displayed separately. Test access uses only supported LiveOps QA targeting and compatible builds. Region and instance selection must map to an existing authorized domain or realm target; this section does not add cohort selectors.

Shared-world rules such as weather, PvP and sieges are evaluated consistently for everyone in the affected realm or instance. Test them in a designated test/canary realm or instance, never by giving arbitrary player percentages different authoritative rules in the same shared world. Account targeting is suitable only where the domain proves isolation, compatibility and fairness; client visibility alone never grants access.

### 9.3 Activation and safe disable

Availability and properties are separate controls. A coordinated change publishes through the existing validated, versioned workflow, with compatible snapshots and declared apply times. Turning a feature on cannot bypass its dependencies or required approvals. Normal disable commonly stops new admission or starts, while active work follows the owning domain's explicitly declared continuation/drain/cancellation behavior. The console must not invent that behavior. Emergency switches use the owning domain's reviewed safe-stop path and existing LiveOps precedence; they do not erase durable state, retract committed rewards, rewrite published obligations or abort committed cross-domain actions.

Before shipping a card, verify server enforcement, consistent shared-world evaluation, unauthorized/stale requests, dependent feature combinations, on/off transitions during active use, restart/reconnect with the last valid snapshot, propagation failure, expiry and rollback. A feature is incomplete if its switch only hides UI, if active-state handling is undefined, or if its tuning fields lack the declarations required by section3. This adds design acceptance work; it does not claim runtime tests or owner acceptance of candidate domain transitions.
