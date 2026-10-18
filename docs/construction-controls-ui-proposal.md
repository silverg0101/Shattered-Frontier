# Construction controls and UI proposal

**Status: PROPOSAL FOR REVIEW — the proposed catalogue, remapping, and final control scheme are not implemented or approved.** The sandbox already has a basic palette/HUD and temporary B, 1–4, R, mouse placement/removal, E door/probe and F5/F9 development save/restore controls. Door walking and actual process restart are tested; final catalogue/remapping and human comfort remain open. SBX-05E has tested atomic straight-line placement APIs; line input is the next gate.

This proposal takes interaction patterns from the [Enshrouded controls research](research/enshrouded-construction-controls.md), not a claim of exact parity. It aims to make the existing construction rules legible through explicit action contexts, a catalogue for deliberate choices, and a compact in-world palette for frequent edits.

## Contract boundaries

- Structural placement remains on the 0.5 m address lattice. Shapes and their allowed subset of the 24 proper cube orientations remain authored content; rotate in 90° steps around a selected axis. The UI cannot enable arbitrary structural rotation, reflection, negative scale, or non-unit scale. See [BLD-01](decisions/BLD-01-voxel-geometry-interface.md).
- Material, shape, and cosmetic appearance are separate choices. Cosmetic appearance cannot change occupied cells, support, collision, permission, or functional access.
- Catalogue and preview show server-relevant cost/source, claim/permission, occupied footprint, support consequences, and the first actionable rejection. Preview is advisory; accepted edits and refunds are authoritative server results.
- Keep structural batch size at no more than 256 changed cells. Terrain brushes stay 0.5–5 m radius and no more than 100 m³ changed per action, within claim and protected-area rules. Line/rectangle repeats are previews of bounded batches; exceeding a limit splits into separately previewed and committed batches. No unlimited stock is added to saved production.
- Eligible reclaim lasts 60 seconds only while the piece is undamaged, unoccupied, and outside combat or siege; it returns attributed materials. Later demolition returns 50% of intact structural materials. A UI undo affordance can only submit a fresh, validated compensating action; it cannot restore snapshots, duplicate refunds, or bypass contribution, permission, or revision checks. See [Construction §3.1](Game-Design/Legends_Shattered_Frontier_Construction_System_Architecture_v0.1.md#31-placement-and-shaping) for reclaim and demolition limits.
- Demolition that causes collapse shows affected pieces and requires explicit confirmation. Construction §3.4 specifies a 10-second instability warning/recovery period and then collapse waves. The standalone sandbox checkpoint is separate from production damage, terrain and salvage integration. The server owns the deadline, revisions, recovery, salvage, and final result. See [Construction §3.4](Game-Design/Legends_Shattered_Frontier_Construction_System_Architecture_v0.1.md#34-structural-integrity). A stale preview must be refreshed and confirmed again; UI state never authorizes collapse.

## Action and context matrix

| Context | Primary actions | Inputs and feedback |
| --- | --- | --- |
| Exploration | Enter/exit construction; inspect without editing | Proposed entry shortcut/tool selection opens construction; Tab opens the catalogue only while the construction context/tool is active. Escape closes unsubmitted UI or exits that context. Construction controls are inactive outside it. |
| Structural | Place, Paint material, Repair, Demolish; select shape/material/style | Primary action applies the selected mode's preview. 90° rotation uses selected axis and authored allowed orientation. Secondary action opens demolition preview when enabled in that context. Paint/repair retain their separate transactions and later implementation gates. Show grid anchor, footprint, cost/source, support and rejection. |
| Decoration / functional object | Select item, preview, place, pack/move or remove | Decoration and objects use their own authored transform/access rules; decor transforms cannot leak into structural placement. Preserve object identity and packing costs/rules. Show interaction clearance and occupancy. |
| Terrain | Select bounded brush and operation; preview changed volume; apply | Distinct terrain context. Show brush radius, claim band, protected blockers, estimated/actual changed volume; reject over-cap edits with a smaller valid option. |
| Catalogue / palette | Search, category, material, shape, appearance, favourites | Selection changes the ghost only; no transaction or inventory grant. Quote displays Cost [server quote] · Source [authorized source]. Keep appearance choices visibly cosmetic. |
| Confirmation / authority | Confirm, cancel, wait, refresh | Confirmation names the requested action, costs, collapse set and revision. Cancel dismisses an unsubmitted preview; a submitted request stays pending until the authority reports its outcome. Show pending, accepted, rejected/stale, and refreshed states in text/icon as well as colour. |

No context leaks a destructive action into exploration or catalogue navigation. Modal text entry consumes movement/build shortcuts. Remap conflicts are surfaced; controller navigation and prompts map to the same contexts and authority outcomes.

## Compact layout sketch

```text
┌ Catalogue (Tab) ────────────────────────┐
│ Search…     Structural | Decor | Terrain│
│ Category       [shape tiles + favourites]│
│ Material       [tier/material list]      │
│ Appearance     [cosmetic swatches]       │
│ Selected: Rough Timber / Existing Half Slab       │
│ Cost [server quote] · Source [authorized] · allowed turns │
└──────────────────────────────────────────┘

       [world ghost / footprint / target]
  BLOCKED · unsupported · anchor one cell below

┌ In-world palette / HUD ─────────────────┐
│ Mode: Place | Rough Timber | Half Slab   │
│ Material · orientation · grid anchor     │
│ Cost/source · permission · support       │
│ Primary Place   Secondary Remove Preview│
│ Rotate axis · shape wheel · Cancel       │
└──────────────────────────────────────────┘
```

Catalogue selection and in-world palette selection address the same item state. The HUD collapses to the selected item, mode, orientation, cost/source, current reason, and visible action prompts; it never hides a blocker behind colour alone.

## Common placement and removal flow

1. **Enter:** Select/enter the construction tool or context to enable its controls; Tab opens the catalogue within that context. Whether a global shortcut selects/enters this context remains an explicit prototype choice. Escape closes an unsubmitted catalogue/preview or exits the context.
2. **Select:** choose action context, category, shape, material and optional appearance. Search/favourites are catalogue conveniences; they do not unlock content.
3. **Orient:** R rotates 90° about the currently selected axis. Show axis and resulting orientation. Wheel cycles shape while in construction context. Optional visible modifier controls precision (axis, plane lock, grid-step offset); do not require chords for basic shelter building.
4. **Preview:** primary-place ghost snaps to the structural lattice, or uses the selected category’s bounded rules. Show footprint, cost/source, permission, support, and first block reason. A line/rectangle preview displays every member and batch boundary, with whole-candidate support validation for every member before one accepted batch.
5. **Place:** primary action commits one validated batch. Display pending status; on stale/rejected response refresh the preview and explain the reason. Accepted result displays authoritative debit and new revision.
6. **Block reason:** if invalid, retain the preview and name the first actionable blocker (for example, occupied cell, unsupported, out of reach, no permission, or over terrain cap). Offer a valid adjustment where one exists; never auto-move or silently overwrite.
7. **Removal preview:** secondary action targets the member and outlines it. Show return eligibility/source and any affected dependent pieces. If more pieces collapse, state the count/area and 10-second recovery rule before the explicit confirmation step.
8. **Confirm / cancel:** confirm submits the preview’s revision-bound intent; cancel/Escape discards only an unsubmitted preview. Once submitted, show pending and await the authoritative result; do not promise rollback. Recheck target, permissions, support, combat/siege status, refund source, and revision on the server. If state changed, refresh and require a fresh intent; correcting an accepted change requires a new validated compensating action.

## Proposed defaults and review gate

The Enshrouded reference suggests Tab for catalogue, primary click for placement, secondary click for demolition preview, R for quarter-turn, and wheel for shape selection. Treat these as ergonomic prototype defaults only. Numbered favourites and direct previous/next selection can shorten common choices. Keep axis, plane, depth/offset, and bounded repeats as visible optional precision tools. Add full keyboard/mouse remapping, conflict indication, and controller navigation/prompts in the UI design; these are proposals, not current sandbox support. Final defaults should be checked for conflicts with movement, combat, accessibility, and platform conventions.

At the prototype comfort checkpoint, ask the user to review catalogue versus palette layout, clarity of blocked/removal previews, comfort of common placement/removal actions, and the entry shortcut/tool-selection model before settling bindings. No decision on every key is needed for this document.

## Next implementation tasks

1. Prototype catalogue and in-world palette with explicit structural, decor/object, terrain, and confirmation contexts; test text-entry focus, modal conflicts, non-colour feedback, keyboard/mouse and controller paths.
2. Exercise the common flow with search/favourites, material/shape selection, selected-axis rotation, single placement, blocked preview, demolition preview, collapse confirmation and cancellation.
3. Add bounded line/rectangle preview and visible batch limits after whole-candidate support validation and bounded atomic publication are available.
4. Connect the full UI to authoritative refund/source and revision results; preserve Construction §3.4 confirmation/recovery/collapse policy when extending the standalone checkpoint to real paid/networked construction. Do not present UI confirmation as trusted authority.
5. Return the prototype at the comfort checkpoint for user review, then settle final remappable defaults and accessibility prompts.

See [current construction sandbox milestone](construction-sandbox-milestone.md) for the implementation boundary: its controls are development-only; the proposed catalogue/remapping scheme, actual player-input exercise, line brushes, door/save, packaged and network acceptance remain open.
