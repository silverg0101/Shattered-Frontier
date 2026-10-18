# Playable construction sandbox — next milestone

User selected this milestone on 2026-10-18. It takes priority over expanding unrelated backend fixtures or the final asset library. It is a development sandbox on the existing Unreal foundation, not completion of the persistent-world MVP or the later vertical slice. Keep those wider scopes intact.

## Playable outcome and isolation

Launch a reproducible packaged Windows client into a small dressed Greenlands test area. Walk/jump, select rough-timber geometry, preview a snapped placement, rotate, build and remove pieces, form a roofed shelter with a working door, see useful rejection/support feedback, then save/reload the same development construction.

Use a dedicated development sandbox map/mode and explicit launch selection. Preserve the current E1 graybox, default configuration and running LAN service. First prove the loop in an isolated local authoritative session; subsequently verify two matching clients against an owned test server. Do not deploy an unfinished construction tool onto the existing service. The environment uses static ground; editable terrain is not required for this milestone.

Development materials are explicitly unlimited test stock, visibly labelled, with no Inventory service, real item grants, crafting progression, salvage rewards, claims or shared-world economy attached. The development action journal/save is a separate test format/path, never schema `building` or `world`, and makes no production durability claim. Only an explicit sandbox mode can accept sandbox edits; packaging or hosting the normal E1 mode cannot accidentally enable them. Exporting a test building cannot import its unlimited-material provenance into the persistent world.

The server owns placement/removal and revisioned geometry. A client sends bounded intent, not arbitrary trusted mesh vertices, material quantities, filesystem paths or authority. The server recalculates reach, snap/rotation allowlists, occupied solid intersection, actor/functional clearance and support from approved content. Stale edits reject with a refreshed preview; retries cannot duplicate members. A successful local prototype does not close B-03/B-04 authentication, transaction or recovery gates.

## Finite playable scope

- One small Greenlands map with clear spawn and build plot, sky/light, readable ground and selected reusable scenery. Dress only the area needed to judge the loop.
- One rough-timber kit first. Initial tool pilot: full cell, half slab and 1:1 slope; then quarter slab/post, corners and stair needed for the shelter. Arches/curves stay tracked in GEO-01, but do not block the first simple shelter if the pilot subset has its own approved evidence. No final mesh for an unvalidated shape.
- Build palette, ghost with allowed/blocked state and plain reason, 0.5 m snapping, permitted rotation, place/remove and bounded line placement. Display active shape, material, controls and development-stock status. Brush/shape changes do not overwrite occupied cells silently.
- One ordinary door and its clearance/motion/support interface; rectangular opening assembled from validated cells. Use Construction §16 dimensions; the multi-cell art arch is not the ordinary door definition.
- Support validation and readable rejection of unsupported placements; occupied space cannot be built through. Runtime support-removal/collapse behavior must follow Construction §3.4 before it is represented as implemented. Earlier placement-only checkpoints are labelled incomplete.
- Explicit save/load test construction, including content/version checks, restart comparison and door state. Reject corrupt/incompatible saves without replacing the valid in-memory world. Writes are atomic for this local format; restart protection remains a bounded sandbox test, not production recovery evidence.

Advanced weather/fire/comfort, combat destruction, siege, automation, live configuration, crafting/claims, networked persistence services and public release are later integration. Do not invent substitute gameplay rules or erase their backlog requirements to complete this sandbox.

## Assignments and dependency gates

| ID | Goal and fixed write boundary | Acceptance / next dependency |
| --- | --- | --- |
| SBX-01 / GEO-01 pilot | `tools/construction_geometry.py`, `tests/test_construction_geometry.py`: stdlib canonical content generation/validation for full cell, half slab, slope; exact rational volume/contact and affine-rotation fixtures | Focused numerical tests and independent review. Explicitly a subset, no ten-shape or general compiler completion claim. Parent owns acceptance and routing. |
| SBX-02 | Read-only client/source/build inventory, then exact Unreal implementation packet in this document; no environment changes | Existing build/input/authority paths, isolated map/mode boundary and bounded checks identified before Unreal changes. Can run alongside SBX-01. |
| SBX-03 / GEO-02 pilot | Numerical compatibility for approved pilot solids: transformed positive-area contacts, exposed face clipping, partial neighbor culling and negative/bucket boundaries | Expected geometry assertions and independent review; demonstrate absence of false contacts/blocked empty portions. Broader GEO-02 remains open. |
| SBX-04 / GEO-03 pilot | New sandbox-only Unreal geometry actor/adapter, explicit mode and opt-in launch; preserved E1 controller behavior; pilot solid display/collision and revision-bound rebuild | Focused C++ build/tests plus isolated editor/client hit/empty-space and stale-result probes. No unmeasured cook/replication claims. Rendering API/module selection reviewed before adding dependency. |
| SBX-05 | Client ghost/palette/input and server intent validation, place/remove/rotate, bounded repeats; dedicated development controls/settings | Actual player-input and authoritative validation checks, then two matching test clients, stale/retry/occupied-space cases. No normal-mode sandbox mutation route. |
| SBX-06 | Support solver/feedback, remaining shelter shapes, separate working door, test-only versioned save/load | Reproducible shelter, support-loss behavior, doorway traversal, reload equivalence and incompatible-save rejection. No Inventory or real claim integration. |
| SBX-ART | Parallel other-session model work via [art handoff](art-templates/construction-sandbox-model-handoff.md); ready environment assets and rough-timber appearance studies | Reviewed `.blend`, exchange formats, textures, scale/provenance and import notes. Final structural assets require approved geometry hashes; do not block on them to make nonstructural scenery. |
| SBX-07 | Dedicated map dressing, approved kit integration, packaging and acceptance run | User-launchable matching build, controls, exact launch instructions, known limitations and saved-building exercise. Parent owns acceptance report and work-plan update. |

At most two authors and one independent reviewer; one writer per file. Parent retains architecture and shared plan/map/handoff. Luna low for inventory, medium for bounded mathematical tooling; architecture/Unreal authority and independent cross-domain review use stronger judgment where needed. Do not duplicate audits or fill slots without useful work.

First cycle: SBX-01 pilot and SBX-02 packet complete within their stated bounds, independently reviewed. [Numerical evidence](evidence/construction-sandbox-geometry-pilot.md): six focused tests plus CLI generation; no playable/engine acceptance. Next SBX-03 and SBX-04, with the latter's geometry/engine assertions gated by the compatibility evidence.

## SBX-02 client packet — source inspection complete

Existing Windows manual controls (WASD/mouse/Space/E) were confirmed by the user in [E1 evidence](evidence/e1-foundation.md). The working module has only Core/CoreUObject/Engine/InputCore/DeveloperSettings dependencies; no procedural construction mesh adapter is present. Keep the existing legacy input setup and probe interaction unchanged. The probe RPC is an example of intent validation, not a construction mutation API.

Next Unreal author write set: new sandbox classes under `GameClient/Source/ShatteredFrontier/{Public,Private}/Construction/`, focused `Private/Tests/SFConstruction*Tests.cpp`, and a new `tools/Create-ConstructionSandboxMap.py`. Proposed dedicated package: `/Game/Frontier/Maps/Greenlands_ConstructionSandbox`. Its explicit world-settings GameMode uses a sandbox-only controller derived from `ASFE1PlayerController` and a mode derived from `ASFE1GameMode`; the existing character continues movement and E interaction. Bind build controls only on that controller, retain base behavior, and route edit requests through the owning controller to authority-owned member state. No construction RPC or unlimited stock in the normal E1 controller/mode.

The author must inspect the installed engine's candidate render/collision APIs and verify intended target support before selecting an adapter. Any required dependency changes to `ShatteredFrontier.Build.cs`/`.uproject` get independent review and dependency-map updates; dependency availability does not prove collision cooking. New map generation refuses an existing package. Do not change default Game/Server maps or the current cook list to publish the sandbox implicitly. Define an explicit opt-in sandbox packaging route and matching client/server build before the packaged acceptance gate.

Existing editor compile route (run from repository root):

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' ShatteredFrontierEditor Win64 Development "-Project=$PWD\GameClient\ShatteredFrontier.uproject" -WaitMutex -NoHotReloadFromIDE
```

This command is a known route, not evidence of a sandbox build. Run only construction-named automation for new logic, followed by the isolated sandbox collision/input and two-client exercises. Do not rerun the full suite or repurpose the live server. Linux/packaged routes remain in the dependency map and must use matched artifacts.

## Acceptance exercise

1. Launch the sandbox build with explicit mode/map and see the small dressed environment and development-stock label.
2. Move/jump and build a grounded timber shelter using approved partial shapes and roof slopes; confirm rotation and repeated placement align across cell/bucket boundaries.
3. Reject occupied-character/door-access overlap, invalid rotation, out-of-reach, stale and unsupported placements with an understandable reason. Neither client visuals nor double submission can create extra members.
4. Remove a piece and see correct surviving exposed surfaces and the designed support response. Place/open/close a door and traverse its unobstructed opening.
5. Save, terminate the owned sandbox process, reload and compare members, versions and functional state. Invalid saves/content fail safely. Show that normal E1 launch does not enable unlimited building.
6. Verify the same authoritative edits on a second matching isolated test client. Record asset import/visual review, targeted tests, build provenance and actual manual observations separately.

The milestone is Done only after this exercise passes with independent evidence review. Earlier geometry/tool/demo checkpoints are useful delivered increments, not the completed sandbox.
