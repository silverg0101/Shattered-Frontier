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
