# DEC-03 — MVP terrain adoption and Unreal integration

Status: proposed, 2026-10-17. Adopt the existing World representation for the bounded Greenlands H2 test area. No algorithm/format exception is proposed. Documentation review does not accept engine capability, throughput, schema installation or deployment readiness. World, Construction and Unreal owners must accept the adapter details and evidence below before TER-01 implementation is ready.

## Decision boundary and authority

This resolves the framing of [roadmap] DEC-03: adopt/integrate the existing baseline rather than reopen terrain representation without a demonstrated constraint. [MVP], “Included experience,” limits terrain to a bounded editable test area. [Backlog], “Epic BLD/TER,” assigns TER-01/02 to H2 and TER-03 to C4. The wider promise of fully destructible mountains remains deferred.

Binding sources:

- [World terrain], “Durable world state (schema `world`),” “Terrain and voxel modification,” and “Structure persistence hooks”: geometry, encoding, T1–T10 validation, chunk storage, replay, regeneration provenance and live hooks.
- [Construction], §§3.4, 8 and 13: deterministic structural support, active-server authority, terrain adoption/healing and protected anchors.
- [Construction Integration], §§3–5: staged World participant, immutable Building decision, completion certificate and recovery. [DEC-02] is the reviewed, still-proposed MVP deployment mapping.
- [UE layout], “Unreal framework ownership,” “Spatial and persistence ownership,” and “Replication contract”: server simulation versus durable owner versus client projection.
- [Tuning], §§2–3: historical `tune.*` names do not make geometry/invariants live editable; undeclared values remain release-bound.

## Proposed adoption

| Area | Adopted contract / MVP mapping |
| --- | --- |
| Density and coordinates | 0.5 m signed-density voxels; 16 m cubic chunks (32³). Q15 `int16` density, round-half-away-from-zero. Map-local millimetres for op center/extent; palette material `uint16`; strength `uint8`. Use World's operation encodings, not client float edits. |
| Spatial identity | World's `uint64 chunk_key` packs zone index and signed 16-bit chunk coordinates with floor division/sign extension. A 64 m cell column spans four by four chunk columns. Do not truncate negative coordinates toward zero or key chunks by rendered actor identity. |
| World owner | Only `world-service` writes schema `world`; only the current layer-0 zone owner submits with `zone_owner_epoch`. Claimable zones are not layered. Building owns claim/permission/structure decisions; terrain edits have no stock or yield participant (TER-02 §13). |
| Durable version | Preserve `base_content_version`, `base_hash`, `head_seq`, `snapshot_seq`, edit identity and construction-action association. Expected heads cover every touched chunk. Prepared/applied/activated participant state must map to DEC-02's explicit staging schema; the illustrative committed-op tables alone do not implement it. |
| Replay and snapshots | Ordered gap-free per-chunk ops over the published base. World compacts after more than 256 ops or 32 KiB to a zstd sparse diff `{voxel_index uint16, density int16, material uint16}`. Load validated snapshot plus tail; runtime mesh/collision are derived state, not save authority. |
| Content release | Rebase committed ops against the changed base during release validation; report `rebase_dropped` protected intersections and finish before activation. Never silently replace a base under saved edits. Compatible build/schema/content handling remains DOC-10/DEC-08 work. |
| Scope | Ship one authorized test area's declared materials/tools and protections. World lists dig/raise/flatten/smooth/paint/restore; exact MVP tool subset requires owner sign-off and fixtures. Do not implement an unspecified tool as a permissive substitute. Instance geometry remains immutable. |

The release codec still needs an executable definition/fixtures: byte order and field framing, voxel index axis/order, operation kernel and boundary sampling, saturation/rounding cases, palette/base version checks, malformed input handling and snapshot integrity validation. The inspected World section supplies field types and determinism requirements, not a complete interoperable codec. World owns this implementation contract; do not invent a second encoding in the Unreal adapter. Changing these semantics is a compatible release/migration decision.

## Unreal support, collision and visibility

Use a server-world terrain runtime behind the project subsystem/adapters described in [UE layout]; class names remain implementation details. It consumes the canonical density/revision state, invokes domain APIs, and projects relevant geometry to clients. It cannot write World tables. Client tools show local previews/pending ghosts; server validation and domain completion determine success.

Proposed adapter sequence:

1. Load the published base, validated snapshot/tail and relevant structures; reconcile pending DEC-02 actions before admitting edits. Build the authoritative spatial/support/collision view from a consistent activated revision set.
2. Compute every affected chunk, boundary neighbor needed for surface/support checks, and claim/protected-volume intersection. Validate World T1–T10, Building rights and expected revisions. Compute candidate geometry, density-derived anchor results and collision work without activating it. Recheck versions at prepare; no preview reserves rights.
3. Prepare the World deltas under one Building `cba_` action, with no stock or yield participant for terrain edits (TER-02 §13). Record the immutable decision, apply participants, then activate only with the completion certificate, following DEC-02. The 32-op/two-second transport flush ceiling never permits volatile success.
4. Before making the changed area playable, install its matching authoritative collision/support projection and deliver revision-consistent geometry. A committed decision cannot be refunded because collision installation failed. Hold the affected area recovery-pending and retry/rebuild; do not allow edits, placement or movement to depend on a half-installed surface. The Unreal owner must define the safe occupancy/readiness transition and demonstrate it in fixtures.
5. Re-evaluate affected Building support in the activation tick for accepted density removal, using the same shipped solver used by preview. World `IsSupported` supplies density eligibility (solid at `ρ ≥ 0.5 ρ_max`); Construction additionally requires compact, ground-connected material, valid face contacts and anchor reachability. Density eligibility alone does not authorize loose fill as an anchor.
6. Replicate bounded relevant revisions/snapshots. Missing revisions trigger resync; late join reconstructs the current base plus accepted revision, not historical effects. Client visual or collision readiness cannot authorize an action. Reject a stale preview and present a refreshed quote.

Construction §3.4's face-connected support solver, unstable grace/collapse and conservation rules remain authoritative; this packet does not substitute a physics solver. The World density hook and Building compact/ground-connected eligibility must be combined explicitly.

Surface extraction, server collision generation/cooking and client mesh representation are **pending Unreal implementation choices**. Prefer one adapter driven by canonical density with deterministic boundary inputs; select a concrete implementation only after DEC-01 engine capability and representative chunk fixtures establish that it can meet project requirements. A plugin or engine mesh path is not selected by this document. Record its version, compatible targets, asset provenance and failure behavior in the implementation ticket. Nanite/rendering choices do not establish edit/collision capability.

## Edges, permissions and lifecycle

- **Chunk/cell edges:** same edit identity across all touched chunks; prepare the complete affected set with expected heads and one decision. Shared boundary samples and support neighbors must be consistent; no one-chunk visible partial edit. Cell streaming changes residency, not durable ownership. Near a zone border, preserve World's eight-metre exclusion; the adapter must demonstrate the entire affected footprint stays outside it rather than checking only the brush icon.
- **Claims:** re-evaluate every intersecting parcel, vertical band, effective terrain/support-edit right and protected volume. Owning one piece or parcel gives no authority over another. Pending chunks/claim actions fence conflicting placement, adoption and healing. Support loss beneath protected foreign structures rejects under T4/T5.
- **Concurrency:** one current zone writer serializes overlapping candidates. Expected-head conflicts reload/revalidate; new proposals need fresh previews. Retry the same key/hash returns the original action; changed payload rejects. Zone-epoch fencing rejects the former owner after recovery. Non-overlap does not waive rights or capacity checks.
- **Restart/failure:** reconstruct snapshots/tails and DEC-02 participant state before interaction. Undecided work recovers via the coordinator; committed work completes/activates, never timeout-refunds. Corrupt/missing snapshots or inconsistent heads isolate affected state and reject mutation until validated recovery. Preserve the existing fractional yield provenance fields as zero through replay (TER-02 §13).
- **Healing/adoption:** public digging exists only in expressly enabled zones. Ordinary public edits heal at the first nightly run after seven real days; each edit retains its own deadline. Legal claim creation atomically adopts committed intersecting edits without changing the zero-yield provenance fields. Recheck occupancy, claims, supports and routes before healing; paid-fill and renewed-yield checks are no-ops (TER-02 §13). Unsafe restoration stays pending. Archive claimed edits before safe restoration/release; event edits use their explicit policy. MVP may author a no-public-digging test region, but must declare that content scope rather than bypass the lifecycle.

## Bounds and configuration

Keep World defaults and T1–T10 order. The following are source defaults, not measured operating limits or newly registered controls.

| Limits / units | Existing default / key | Treatment before shipping |
| --- | --- | --- |
| Geometry | 0.5 m voxel; 16 m chunk (`tune.world.terrain_chunk_m`) | Release-bound storage/topology. No live edit permission. |
| Brush and changed volume | 0.5–5 m radius; 100 m³ (`tune.world.terrain_max_volume_m3`); nominal pre-check for dig/raise/flatten, actual changed-volume check for every kind | Preserve both checks. Shipped validation/content rule; no radius override bypass. |
| Construction terrain budgets | At most eight 16 m chunks and 8,192 touched cells per action; 20,000 outstanding public changed cells per character/realm (Construction §15) | Inherited release-bound budgets. Enforce alongside changed-volume checks, including smooth/paint; fail before charge. |
| Original-base bounds | 8 m below / 12 m above; `zone.min_dig_z`; 8 m zone-border exclusion | Release-controlled as World explicitly states; no cumulative-baseline drift, land-owner or GM override. |
| Reach/protections | 8 m reach; protected nodes 3 m, respawns 5 m, unauthorized structures 1 m clearance | Enforce server rights/protection rules. Any future editable reach needs a complete declaration; protections remain invariants. |
| Rate/load | 1 commit/1.5 s per character; 2,000 m³ per character/hour; 200 ops/s per zone | Existing keys: `terrain_commit_interval_ms`, `terrain_volume_per_hour_m3`, `terrain_zone_ops_per_s` under `tune.world`. Undeclared bounds/permissions mean release-bound until DOC-05 closes declarations. |
| Public healing | Seven elapsed real days, nightly safe-restoration job | Release-bound lifecycle/algorithm. Changes need compatible migration/player-impact review. |

Where owners deliberately expose an adjustable operating value, DOC-05 must supply its canonical key, typed units/default, safe permitted bounds, step/fallback, scope, operator permission/approval, effective timing, cross-field checks, client visibility and rollback/observability through shared config-service controls. Do not invent safe bounds from these defaults. Capture one effective config/content version per action; publication cannot reinterpret an already prepared action. Unavailable/invalid policy fails the edit closed rather than silently widening it.

## Evidence gates and selectable fixtures

TER work remains gated by owner acceptance of DEC-02, the codec/support/collision mappings above, DEC-01 toolchain capability and an implementation packet. [MVP] owns provisional performance budgets; World’s ≤8 s added boot for 20,000 edited chunks is also an unmeasured target. Select representative work, report hardware/build/scene and p95/max rebuild/activation latency, tick/frame time, memory, replication bytes, snapshot/tail recovery time and queue behavior. Do not treat a chunk-size calculation or design budget as a benchmark.

| Fixture | Required result / evidence |
| --- | --- |
| TER-D03-01 deterministic codec | Golden positive/negative-coordinate chunks, signed-key round trips, half-rounding/saturation and malformed payloads; server/tool/replay produce identical canonical densities/materials and heads. |
| TER-D03-02 chunk/cell edges | A brush crosses face/edge/corner and a 64 m cell boundary; all affected chunks share one edit outcome; no cracks or one-sided collision; assert zero yield (TER-02 §13). Add zone-border rejection and base-depth/raise limits. Test exact/over eight chunks, 8,192 touched cells and 20,000 outstanding public changed cells, including smooth/paint; excessive actions reject with zero charge. |
| TER-D03-03 competing rights | Owner, authorized collaborator and stranger contend across parcel/vertical boundaries; protected supports/nodes/routes reject. Permission revision changes between preview/prepare invalidate the quote without charge. |
| TER-D03-04 support/Compact | Loose versus compacted fill, floating fill, ground-disconnected anchors, exact density threshold and bridge-support removal; preview and authority agree, foreign supports reject, legal instability/collapse uses Construction receipts; fill charges zero (TER-02 §13). |
| TER-D03-05 concurrency/fencing | Same-key retry, changed payload, competing expected heads, stale zone epoch and non-overlap; one conserved outcome with fresh previews where required. |
| TER-D03-06 staged failure | Crash after prepare/decision/apply/completion and fail collision installation/publication; recovery exposes no usable partial geometry and produces zero yield (TER-02 §13), returns original receipt and repairs activation. Observe protected occupancy/readiness. |
| TER-D03-07 replay/late join | Snapshot compaction plus tail, missing revision, late client, corrupt/unavailable snapshot; same canonical state or isolated unavailable region, never guessed terrain. |
| TER-D03-08 lifecycle | Public edit age boundary/unrelated edit, claim adoption race, archived release, occupied/protected healing, event expiry and repeated excavation/fill/revert; no entombment or erased surviving structure; renewed-yield and paid-fill checks are no-ops and yield/charge stay zero (TER-02 §13). |
| TER-D03-09 compatible content | Base hash/version change with protected intersection and interruption; rebase report and complete validation before activation, or keep old compatible candidate closed to writes. |

These are future focused verification cases, not tests executed by this document or a full-suite instruction.

## Owner requests and revisit triggers

1. **World + Unreal:** close codec/kernel/boundary fixtures and select the density-to-collision implementation with measurements. Define safe readiness/occupancy transition for delayed activation; a crash after durable commit must converge.
2. **World + Construction:** specify how Construction's Compact action, compacted-fill provenance and anchor eligibility persist/replay through World's current six-kind terrain interface. The inspected edit enum does not demonstrate that mapping; do not silently add a seventh kind or infer compaction from density. Confirm support-neighbor coverage and lifecycle atomicity.
3. **World persistence + Building:** implement DEC-02's staging/certificate/epoch mapping and DOC-08 recovery policy before claiming H2 durability. **Configuration owners:** close only deliberately adjustable declarations via DOC-05. **Delivery owners:** handle codec/base/schema compatibility in DOC-10.

Revisit representation only if measured capability or compatibility evidence shows the baseline cannot satisfy the bounded scope. Compare a scoped content reduction, a different derived mesh/collision adapter preserving World storage, and an explicit representation migration in that order. Each alternative needs affected contracts, player/data impact and new fixtures; no live console exception or unreviewed algorithm replacement.

Derived-adapter evidence: [TER-01](TER-01-mesh-terrain-fit-check.md) (2026-10-22) found that UE 5.8 Mesh Terrain is authoring or backdrop only. A density-driven mesh and collision adapter met replication, collision and chunk-seam needs. **TER-01b passed the same day:** a manifold face-loop mesher with 2:1-balanced distance LOD and transition cells, meshing off the game thread, 0 tears in every check, and the whole 2.5 km zone within the GFX-02 ground allocation. Dig-anywhere is adopted on this adapter, with the limits in [TER-02](TER-02-digging-limits.md). It keeps only edited chunks, so the base is regenerated from `base_content_version`. This representation is unchanged.

Validation: source link targets and named headings checked; documentation-only work, no runtime tests. Independent review and correction evidence are recorded in the work plan. Coordinator owns planning/index integration.

[roadmap]: ../Game-Design/Legends_Shattered_Frontier_Production_Roadmap_v1.0.md
[MVP]: ../Game-Design/Legends_Shattered_Frontier_First_MVP_Milestone_v1.0.md
[Backlog]: ../Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md
[World terrain]: ../Game-Design/World-Simulation/Legends_Shattered_Frontier_World_Terrain_Resources_and_Population_v1.0.md
[Construction]: ../Game-Design/Legends_Shattered_Frontier_Construction_System_Architecture_v0.1.md
[Construction Integration]: ../Game-Design/Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md
[DEC-02]: DEC-02-durable-actions.md
[UE layout]: ../Game-Design/Legends_Shattered_Frontier_UE58_Project_Layout_v1.0.md
[Tuning]: ../Game-Design/Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md

