# Mounts and Companions — Integration and Delivery

Version 0.1 — Implementation design baseline (2026-10-19)

Status: Design contract, not runtime evidence. [Entry](../Legends_Shattered_Frontier_Mounts_and_Companions_v0.1.md) · [Gameplay/defaults](Legends_Shattered_Frontier_Companion_Gameplay_Details_v0.1.md) · [Backend conventions](../Backend/Legends_Shattered_Frontier_Backend_Service_Communication_Persistence_and_Content_v1.0.md) · [World population](../World-Simulation/Legends_Shattered_Frontier_World_Terrain_Resources_and_Population_v1.0.md). Protocol identifiers below are proposals tracked in the Registry companion extension; freeze their exact schemas before persistence implementation. Gameplay decisions and writer ownership are selected here, not left to an implementer.

## 1. Selected owners and consistency boundaries

| Concern | Sole durable writer / authority | Contract |
| --- | --- | --- |
| Stable, owned creature identity/traits/training/XP, entitlements, operation receipts and deployment binding | `progression-service`, schema `progression` | Character-scoped aggregate; shares owner-level/Hunting consistency boundary; no new service. |
| Wild spawn identity, generation/seed, capture reservation/tombstone and respawn eligibility | `world-service`, schema `world` | Zone simulation validates the actual encounter; World durably fences spawn disposition. |
| Wallet charge/refund | `economy-service` | Gold `Debit`, `allow_partial=false`, sink `npc_service`; supported compensation credit only, never direct SQL from Progression. |
| Active actor AI, hits, conditions and commands | Current `zone-server` / `instance-server` | Projection under a fenced deployment binding; cannot mint persistent ownership or XP. |
| Pet damage/threat/owner credit and shared interrupt gate | Combat runtime | Character-sourced math, own threat entry, owner-attributed rewards; persisted shared cooldown belongs to the Progression deployment snapshot. |
| Movement and live simulation ownership | World | Existing movement/token/handoff rules plus trained-creature binding. |
| Character lifecycle/privacy/realm transfer orchestration | Existing Backend services | Extend Progression's existing participants; no second privacy/transfer writer. |

The Stable is not an inventory container and creatures are not ItemInstances. No Inventory dependency is needed for the first capture loop. A later bait/equipment system must add explicit Inventory contracts. Creature-owned progression never grants player LP/attribute points/prestige. Account mount unlocks remain existing account entitlements, with compatible appearance use only.

## 2. Proposed records and constraints

Apply Backend realm scoping, account encryption/privacy classification, expand/contract migrations, optimistic revisions, idempotency and outbox conventions. All ownership-changing operations lock the owning character/Stable aggregate before creature rows, ordered by ID. No DB lock is held across an RPC. No cross-schema reads/writes substitute for owning APIs.

| Record | Minimum fields / constraints |
| --- | --- |
| Stable (key `character_id`) | Account/realm, creation-base entitlement, revision, transfer/privacy gate; capacity derived from unique entitlements. Hard max 12, used + reserved ≤ capacity. |
| Companion (proposed `cpn_`) | Owner/account/realm; species `npc/*`, family key and content version; capture provenance (world/zone/layer/slot/generation/source definition/level/trait seed); current level, curve version, within-level XP/carry; rarity and one trait definition; training lessons/techniques/passive selection; Injury; health/resource snapshot; condition revision; lifecycle and pending operation; row revision. |
| Stable entitlement (key character/source key) | Source `base`, `hunting_milestone`, `gold_purchase`; slot delta; cause/quote/payment receipt; content/config version and created time. Unique milestone or purchase ordinal. |
| Deployment (key character/active slot) | Companion ID; slot 1/2; world actor binding; simulation owner/epoch and deployment generation; profile version; active-capacity snapshot; latest acknowledged conditions/sequence; owner-shared interrupt deadline. At most one admitted binding per creature and one creature per slot. |
| Companion operation (proposed `cpo_`) | Kind, actor/character, idempotency/request hash, immutable payload/content/config/revision digest, fixed participant set, state, decision/completion evidence, receipts, worker/fence, timestamps and result. Capture/purchase identity cannot disappear when ordinary RPC dedup expires. |
| Companion ledger (key operation/source + recipient + entry kind) | Ownership/training/XP/entitlement/condition transitions; before/after revision, base/granted/discarded XP, multiplier segments, cause/source ID, version and durable result. Append-only with existing retention/privacy rules. |

Separate lifecycle from condition: lifecycle `owned`, `released`; training lessons are monotonic bits; Injury is a condition; deployment is a binding; pending capture is an operation, not a usable owned creature. Storage/deployment cannot clear Injury. Untrained training projections use a distinct non-combat binding kind with no Combat capability. Released records cannot deploy or earn XP and never become wild reward sources.

Suggested normalized tables: `progression.stables`, `companions`, `stable_entitlements`, `companion_training`, `companion_deployments`, `companion_operations`, `companion_ledger`. Exact SQL/indices and prefix registration are the first persistence packet, not a claim that migrations exist. Validate unique live creature per source spawn generation, unique companion/slot binding, unique milestone/purchase ordinal and unique XP source/recipient, including after restore/realm transfer.

## 3. Operation surface and rejection contract

Client sends intent via authenticated existing gateway/simulation route. Account/owner authorization is derived from trusted context; requests cannot supply authoritative stats, rarity, price, skill, receipts or XP. All mutations require idempotency key and expected aggregate revision; same key/different payload returns `IDEMPOTENCY_MISMATCH`. Backend retry/status/deadline semantics apply. Reads return current revision, pending operation and authoritative result; never infer success from an animation.

Proposed Progression RPCs:

| Operation | Caller and authoritative input/output |
| --- | --- |
| `GetStable` | Gateway/simulation: roster, entitlements/capacity, training/conditions, active bindings, pending operations, revision. Owner-private fields only. |
| `BeginCompanionCapture` / `FinishCompanionCapture` / `CancelCompanionCapture` | Current simulation: exact spawn generation and server encounter/channel evidence; returns reservation/operation or committed companion. Finish cannot supply a client-authored trait/level. |
| `RecordCompanionTraining` | Current simulation: lesson/technique, objective proof, binding/content and input context; commits one monotonic completion and optional Hunting practice linkage. |
| `ChangeCompanionDeployment` | Current simulation: deploy/store/slot-selection with completed channel evidence and owner combat/authority snapshot; returns fenced binding/condition state, not a duplicate actor. |
| `CheckpointCompanionConditions` | Bound simulation only: monotonically sequenced health/resources/Injury/cooldowns; old generation/sequence rejected. Injury finalization is a critical durable transition. |
| `GrantCompanionXp` | Simulation or reward-service with trusted source authorization: source ID, owner base-source XP context and participant snapshot. Progression computes eligibility, split and multiplier; client never calls it. |
| `QuoteStableExpansion` / `PurchaseStableExpansion` | Gateway: trusted Stablekeeper proximity through current simulation; quote binds next ordinal, exact copper price, content/revision; purchase returns pending or durable entitlement/result. |
| `RecoverCompanion` / `ReleaseCompanion` / `RenameCompanion` | Current simulation/gateway: verified rest/channel or confirmation/moderation context; owned-revision checks and result. No direct admin DB editing. |
| `GetCompanionOperation` | Authenticated owner/support: definitive pending/committed/aborted result and retry guidance; hides participant security material. |

Internal World calls must expose prepare/query/commit/abort capture disposition on an exact stable slot/generation and current simulation epoch. Freeze concrete method names and proofs in the protocol packet; do not overload a periodic `ReportZoneState` write into capture commitment. Economy adapter calls only existing authorized `Debit`/supported refund flow; add Progression's scoped `npc_service` caller permission without granting arbitrary wallet authority.

Use Backend status codes with proposed reasons `CREATURE_NOT_TRAINABLE`, `LEVEL_TOO_LOW`, `HUNTING_TOO_LOW`, `STABLE_FULL`, `CAPTURE_CONTESTED`, `CHANNEL_INTERRUPTED`, `TRAINING_REQUIRED`, `ACTIVE_LIMIT`, `OWNER_IN_COMBAT`, `CREATURE_INJURED`, `PATH_UNAVAILABLE`, `MODE_FORBIDS_CREATURE`, `QUOTE_STALE`, `OPERATION_PENDING`, `STALE_BINDING`, `CONTENT_INCOMPATIBLE`, `CHARACTER_TRANSFERRING`. Pending is not an abort. UI can retry/query the same operation; an expired quote never reprices silently.

## 4. Capture commit and World persistence extension

Current World persists gathering/rare/elite slots periodically and keeps common NPC slots in memory. That is insufficient for captures. **Every capture-enabled slot must durably record its spawn generation, selected definition/level/rarity/trait seed and disposition before exposing a capturable actor.** Generations never reuse an old spawn identity. This is a bounded new write on spawn creation/disposition, not per-tick NPC-position persistence. Measure the extra population write load before realm rollout.

Progression coordinates capture; fixed participants are Progression and World, with the current simulation as a fenced evidence/projection participant, not a durable database substitute:

1. Create operation and reserve exactly one Stable slot locally. World reserves the exact active spawn generation after authoritative simulation channel admission; its durable reservation serializes competing capture, death disposition and respawn advancement for that generation. Normal damage/death still interrupts the channel; commit the death disposition only after known capture abort, rather than making the target immune. An admission failure triggers a known abort and release of only this operation's holds.
2. Channel runs with target AI/normal damage. At completion simulation serializes finish versus damage/death/interrupt on its game authority, then freezes capture disposition while the commit is resolved. Freeze cannot make a creature invulnerable early: it starts only after channel requirements were satisfied. Stop new loot/death/capture dispositions while a prepared decision is unknown.
3. After channel finish, World final-prepares the exact still-reserved disposition under current channel/actor evidence. Progression records immutable **commit** only with valid owner level/Hunting, free reserved capacity, exact content/traits, current channel-completion attestation and World final-prepared receipt. A stale simulation epoch, foreign engagement or pre-finish death aborts before this pivot.
4. World applies that commit by marking the generation captured and scheduling its ordinary replacement once; Progression creates the owned individual and consumes the reservation. Projection gates remain until all fixed participants have applied. Completion then makes the Stable creature usable and removes the wild projection coherently. A lost reply queries/replays the same decision.
5. Before commit a durable abort releases holds; after commit only forward completion/reconciliation is allowed. No timeout releases an uncertain hold or restores a wild actor. Recovery reads authoritative decision/receipts, never just a stale cache or absence from a restored snapshot.

This is a semantic protocol; it does **not** claim existing construction B-03/B-04 automatically admits capture. The persistence packet must bind current actor admission, immutable decisions, worker takeover, participant barrier/completion, retention and restore enrollment through a companion adapter with the same no-stale-writer guarantees. See [B-03](../../decisions/B-03-actor-admission-contract.md) and [B-04](../../decisions/B-04-delivery-fencing-and-restore.md). A broad lease, random epoch or in-memory lock alone is insufficient. Do not mark production capture ready until this gate is exercised.

Capture-channel admission reservations are not construction commit prepares: construction's fixed five-second decision deadline cannot be reused for an eight-second capture. The adapter separates the long channel/reservation phase from the final durable commit-prepare phase after validated channel finish. Record distinct phase evidence and supported deadlines in CMP-01, with no DB lock held while channeling. Losing the channel before final prepare can produce a known abort; any unknown final prepare/decision stays fenced until reconciliation. A reservation timestamp alone never proves an uncertain operation aborted.

## 5. Purchase and entitlement recovery

Quote binds character, next ordinal, slot delta, copper price, content/config version and Stable revision; default validity 60 seconds under the declared quote-lifetime control, checked at acceptance. Expiry never authorizes rollback of an accepted payment. Milestone reconciliation and purchases serialize on the Stable. Requote on intervening mutation or price/content publication; never accept over hard capacity.

1. Progression persists a purchase operation reserving the next ordinal; request exact full Economy debit with deterministic key derived from that operation. Backend authorization includes only this `npc_service` scope. No caller-chosen sink, wallet or refund amount.
2. Definitive insufficient-funds/rejected debit aborts with no entitlement. Unknown debit outcome stays pending and queries/retries the same key. A successful debit is the forward-recovery pivot: grant its reserved permanent entitlement exactly once, even if content prices change afterward.
3. Reconnect displays pending operation and resolves receipts; it cannot submit another purchase for the same ordinal. If an irreparable entitlement failure requires compensation, use an audited supported Economy credit linked to that exact original debit, at most once. Refund and entitlement cannot both become usable; a separate durable correction action resolves it. Do not invent a refund with an unrelated currency source code.

Existing Economy permission/source allow-lists and semantic receipts must be bound in the adapter packet before paid expansion can ship. No raw balance updates. Reject partial debits. Price/entitlement history survives the generic seven-day RPC dedup window; aged replay cannot buy/grant twice.

## 6. Deployment, conditions and handoff

Progression atomically checks ownership, training, owner level/Hunting/node, character gates, current condition revision, active slot capacity and aggregate revision. It allocates a new deployment generation under World's current live simulation owner. The server projects exactly that binding; repeated acknowledgments cannot spawn another actor. Commands validate the local admitted generation and monotonically sequenced intent; network messages from an old binding never execute.

Runtime may simulate ordinary damage without a DB call per hit, but it must mark a deployment **unsealed** before activation. Periodic acknowledged snapshots and critical Injury/cooldown transitions advance condition sequence. A graceful store/handoff writes a final sealed condition snapshot before making the source binding inactive. Health/resource snapshots never accept an older sequence or invalidate a recorded Injury/shared interrupt deadline.

If the server dies with an unsealed binding, fence source simulation before recovery. Conservatively mark that creature Injured and retain the latest durable training/XP/cooldown evidence; do not restore a stale healthy snapshot or reward a crash with healing. Where shared interrupt state could be newer than the last acknowledged checkpoint, extend its deadline to at least recovery-enrollment time plus the supported maximum interrupt recovery (30 seconds). Thus an uncertain crash cannot reset an interrupt even with a short Injury recovery profile. UI explains interrupted-journey recovery. This fail-safe applies only to uncontrolled loss, not ordinary sealed travel. XP/training/entitlement grants acknowledge only after durable commit. Periodic condition checkpoints use the declared default 5-second control; load evidence must validate its supported 1–10-second range without per-hit DB calls. Critical/final transitions are saved separately, and an interval never proves survival/health.

World zone handoff transfers owner and creature binding set together. Source stops new commands, seals conditions, and yields authority; destination validates content/mode, revisions and owner/slot admission, then activates only after source fencing/ownership transition. A partially migrated set remains non-offensive pending reconciliation, never active on both servers. An incompatible destination can store a creature with conditions after a coherent binding transition. No automatic heal, free resurrection or loss of training on handoff.

Emergency withdrawal/skill correction must preserve threat/ongoing effects until Combat's safe disengagement; storage is not a threat-clearing operation. World forced moves/recalls use existing displacement token semantics. Companion capability expiration/revocation, crash takeover and restore enrollment must be integrated with current actor-admission machinery before network acceptance. Do not design a parallel authentication/lease system inside this feature.

## 7. XP and reward contracts

Progression reads trusted owner source XP and current owner level, not client amounts. Source identity includes encounter/death generation, award kind and owner; each creature recipient gets a dedup key. A kill and an encounter bundle are separate legitimate rewards only if the authoring encounter intentionally grants both to players. A replay or delayed duplicate cannot produce another creature pool. Store eligibility snapshot and base pool before attempting recipient writes, then commit split/levels/ledger atomically for that owner.

Add optional companion-participation evidence to simulation/reward delivery, without changing `credited_character_ids`, party size or existing player reward ownership. Base source amount precedes player-only modifiers; publish that amount explicitly when rewards are authored. A reward producer lacking the field cannot silently derive it from already-modified XP. The first milestone can support kills only; completion-bundle XP remains disabled until its producer adapter passes evidence. This is an explicit delivery boundary, not permission to fabricate participation on claim.

Exact calculation contract: base pool/share is a nonnegative int64 XP amount; within-level progress and remaining base are reduced arbitrary-precision rationals. Parse `bonus_max` as integer tenths `b10`, and gap control as integer `F`. Multiplier is exactly `(10*F + b10*min(gap,F)) / (10*F)`. While below the owner's ceiling: let `need = xp_to_next(level) - progress`; consume `take = min(remaining_base, need/multiplier)`; add `take*multiplier`, subtract `take`, and on exact threshold increment level/reset progress before recomputing. Display `floor(progress)` but persist normalized numerator/denominator. At ceiling discard remaining base and set progress to zero. Persist segment/result ledger together with the level update. No floating point or per-XP rounding.

Use checked arbitrary-precision integers with a 4096-bit numerator/denominator bound on persisted rational state; reject unsupported input/range before any partial mutation and flag a domain invariant issue. Ordinary profile bounds and at most 100 level transitions per recipient keep work bounded; the arithmetic oracle must verify that bound under all supported profile/curve transitions. Splitting an identical recipient's base award into sequential segments under unchanged owner/profile must produce exactly the same level/rational progress as one batch, including several boundaries and the ceiling. Two-pet pool splitting is performed once per source, not independently per transport chunk. Tests cover cap, zero multiplier bonus, rational carry, very large grants, dedup and rejected overflow. This algorithm is selected; CMP-01 freezes its language-independent fixtures and schema representation, rather than leaving arithmetic to runtime float defaults.

## 8. Events, replication and lifecycle participation

Proposed events, under Backend/Registry envelope and existing Progression outbox/topic:

| Name | Minimum payload beyond envelope |
| --- | --- |
| `progression.companion.captured` | Owner character/companion/operation IDs, source generation, capture level, species/rarity/trait content IDs, content/config version, Stable revision. |
| `progression.companion.training_changed` | Owner/companion, lesson/technique, training revision, operation. |
| `progression.companion.xp_granted` | Owner/companion/source, base share, granted/discarded XP, old/new level/progress, curve/config version, ledger revision. |
| `progression.stable.expanded` | Owner, source/ordinal, slot delta/resulting capacity, operation, receipt reference, Stable revision. |
| `progression.companion.condition_changed` | Owner/companion, Injury/recovery transition, condition revision, cause/operation. |
| `progression.companion.released` | Owner/companion/operation, tombstone revision. |

All events have owner routing/privacy class; do not publish payment receipts or security attestations to ordinary clients. Owner receives private roster/training/XP/pending-operation snapshots; nearby observers receive actor species/appearance/level/name/owner, health/action and permitted combat effects through normal relevance. Secret trait seeds, wallet data, aggregate receipts and proof material remain server-only. Emit events in the same local transaction as their state change; event consumers dedup and cannot mint ownership through projection replay.

Extend Progression `PrivacyParticipant` export/erase with the complete roster, entitlements, training, operation/ledger linkage and current bindings under existing retention/legal-hold policy; released creatures remain attributable until the supported erasure/tombstone process. Soft character deletion suspends projections and preserves the roster; restore revalidates; purge erases/pseudonymizes under owning policy. Content reference scans include species, trait, technique, mastery, appearance and curve/profile versions. Retiring content needs compatible replacement/quarantine that preserves the individual, never delete-on-missing-ID.

Extend Progression `TransferParticipant` row set/digest to all Stable data. Prepare blocks new capture/deploy/purchase/XP/training, drains/seals active bindings and refuses unresolved cross-domain operations. Stage/commit carry roster/entitlements/conditions with old source bindings fenced; destination starts stored. Abort releases only transfer holds and never refreshes health. Respect existing ledger carry/opening rules and privacy gates. Restores freeze affected new admission, reconcile capture/payment/source frontiers and tombstones, and enroll/fence old writers before reopening. No surviving event queue may recreate a purged or released creature.

## 9. Delivery packets and observable exits

| Packet | Smallest result / required checks |
| --- | --- |
| CMP-01: schema/content contract | Freeze proposed names, SQL/constraints, family content schema, arithmetic oracle and authority adapter bindings. Validate content references, slots, trait compatibility, curve preservation and fixed participant evidence. No generic infrastructure program. |
| CMP-02: first playable canine | One real wild canine can be weakened/captured, individually trained, deployed, commanded to attack, stored and recovered after defeat. Two-slot roster persists across restart; source creature cannot reappear from old generation. Targeted authority tests plus rendered server/two-client observation. |
| CMP-03: progression and Pack Handling | Hunting practice, 300/700 permanent slots, owner-capped catch-up and two-creature control/profile work. Boundary tests at 699/700, reset/reconnect, source dedup and combined DPS/CC/survival comparisons. Kill XP first; encounter-bundle adapter only when ready. |
| CMP-04: Stablekeeper expansion | All four sequential gold purchases; exact quote/charge/entitlement/pending/recovery paths, price change and aged replay. Does not sell active slots. |
| CMP-05: mounted equine | Riding lesson, rider/pet target selection, allowed rider abilities, combat speed history, forced dismount geometry/water/no-mount, two-actor control. Rendered movement and network checks; no flight. |
| CMP-06: instance/realm travel and broader ecology | Walking pet versus riding admission, owner-build scaling versus story-helper scaling, sealed zone handoff, realm transfer/privacy/restore, ursine then regional family coverage. Profile/rarity population evidence before whole-world claims. |

Supporting work is bounded by the packet's concrete blocker/exit. Do not substitute an in-memory fixture for durable gameplay or turn a test harness into capture commands available to players. No full repository suite, deployment or live economy writes are part of this design change.

Targeted failure matrix for the relevant packet:

- Capture: two players/one spawn; two operations/last Stable slot; DoT/channel/foreign engagement/death race; restart before/after each participant prepare/commit/apply/completion; no rewards or duplicate respawn; common and rare generation survive restart without reroll.
- Training: ordered objectives, untrained combat/riding rejection, duplicate lesson, high-level technique coaching, no idle practice/XP, training projection cannot interact with combat/navigation.
- Stable/purchase: base 2, earned milestone once, all purchase ordinals/prices, skill reset preserving storage, exact hard-cap rejection, insufficient/unknown debit, stale quote, lost reply, refund-versus-grant and replay after dedup expiry.
- XP: grey/ineligible source, capped owner, two shares, capped pet consuming share, level boundary segmentation, large award/overflow, sync and owner correction, duplicate kill/bundle and delayed claim, no player LP or reward multiplication.
- Combat: single versus dual profile, shared interrupt deadline/target DR, own threat and owner credit, no idle/unattended actions, command range/LOS/path, defeated second retaining dual allocation, PvP legality and group auto-taunt default.
- Conditions/authority: no storage/reconnect heal, Injury and cooldown continuity, dirty-crash conservative recovery, stale binding/generation rejection, source fencing before destination activation, partial handoff/restore pending instead of duplication.
- Riding/content: distinct rider/mount targets, AoE once each, geometry-safe dismount, `v_eff` history, deep-water/no-mount, instance policy, observer relevance/private data, staged feature disable and restart with last-good profile.

Before declaring implementation-ready, close CMP-01's exact protocol/arithmetic bindings in reviewed files. Before declaring a later packet playable/accepted, record its matching targeted runtime and rendered/network evidence. Numerical balance defaults here are hypotheses; passing schema/authority checks alone does not validate their gameplay quality.
