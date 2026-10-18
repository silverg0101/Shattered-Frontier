# UI-01A: Cursor construction prototype

**Status: bounded implementation assignment.** Implement one standalone construction-controls prototype for the existing opt-in `Greenlands_ConstructionSandbox`. This is a graybox comfort iteration, not production construction UI. The user confirmed the current graybox opens and supports placement; the next prototype must make cursor placement, camera control, demolition, and depth selection legible and comfortable.

## Goal and scope

In the construction sandbox, keep a free, visible OS cursor available while the player aims a placement ray from the cursor into the world. Holding RMB temporarily captures camera look; releasing it restores the free cursor. Placement, door interaction, and demolition preview use the cursor ray. Demolition is a separate deliberate mode: LMB deletes only in that mode; RMB controls the camera in either mode. Add a compact palette and visible stepped plane/depth controls so a player can select a plane and move the placement depth without guessing. Keep existing sandbox-only structural pieces, door, single placement, line mode, support-loss confirmation, and development save/load behavior working where the new cursor model permits.

Build only the catalogue/palette shell needed to demonstrate structural shape selection and the current door selection plus placement feedback. Reuse the existing semantic state and messages. Do not add decor, terrain transactions, search/favorites, remapping, controller navigation, economy, refunds, or production gameplay. Present the prototype for user comfort review before choosing final bindings.

## Contracts to preserve

- Keep all edits behind the opt-in standalone sandbox GameMode/controller and its existing mode checks. Preserve normal E1 settings, maps, art, live preview PID 40800, and the base E1 input behavior outside the sandbox.
- `ASFConstructionSandboxState` remains the authority for reach, sight, clearance, support, permissions, and revision checks. UI and preview are advisory. Commit the captured intent/revision through current State entry points; never silently refresh and retry a stale action.
- Preserve the complete line seed/revision fence, bounded line enumeration, whole-candidate support validation, and support-loss demolition preview/confirmation/rollback behavior. A change in State revision invalidates the corresponding pending preview/seed as it does today.
- Keep cursor targeting inside the existing configured reach. The placement ray must be visibility-tested from the pawn view origin to the cursor-selected world point and then run through existing evaluation; do not accept an unoccluded cursor hit as permission to edit through walls. Keep existing server/State checks intact.
- Do not add adjustable gameplay balance values. Plane/depth controls are local preview selection on the existing 0.5 m construction lattice, with a small fixed prototype step and explicit bounds from the sandbox plot. They do not change canonical geometry or authority rules.

## Interaction behavior

- In construction mode, show a free cursor and a small palette with current shape/door, mode, rotation/orientation, selected placement plane, depth step, current preview reason, and plain-text prompts. Show allowed/blocked status with text or icon as well as color.
- LMB places/advances the current placement flow. In line mode it preserves the existing first-click seed and second-click commit behavior. Clicking UI controls must never also place in the world.
- X/Delete or the palette toggles Demolition mode without editing the world. Its orange target and mode label precede LMB deletion through the existing support-loss preview/confirmation flow. RMB is exclusively camera control in either mode; neither click nor release deletes.
- During RMB hold, hide or confine the cursor only as required by the chosen engine input mode, route mouse delta to the existing look axes, and restore the cursor and UI focus on release. Never warp the OS cursor to synthesize camera motion.
- Escape closes an unsubmitted catalogue/palette subview or cancels an unsubmitted demolition preview using current cancellation semantics. Losing window focus while RMB is held must release look capture, restore the free-cursor mode when focus returns, and clear any pressed-button state so the next click cannot commit an old preview.
- Text entry or UI focus consumes construction shortcuts and mouse clicks. UI bounds must not click through to the world. When the pointer is over a UI control, world placement and demolition do not execute. Closing the UI returns focus to gameplay without invoking an action.
- Plane/depth controls must be visible in world context: identify the active plane/axis, show the selected grid depth, and provide bounded one-step increment/decrement controls. The ghost/footprint must move to the selected plane and continue to show the actual hit and rejection reason. The depth control must not make an otherwise out-of-reach or sight-blocked target valid.

## Implementation constraints and open risk

Current bindings live in `ASFConstructionSandboxController::SetupInputComponent`; controller-level `Interact` consumes the pawn's inherited E callback. `ASFE1Character` owns movement/look/jump/E bindings, and its look handlers apply the configured sensitivity. The controller currently traces from `GetPlayerViewPoint` along view forward each tick and uses debug geometry/on-screen debug text. The project module currently declares Core, CoreUObject, Engine, InputCore, and DeveloperSettings plus construction runtime dependencies; Enhanced Input is disabled and Slate/UMG are not listed.

The selected prototype uses a native UMG widget tree, UMG as a public module dependency for the exported widget, and private Slate/SlateCore dependencies for input/focus. The installed engine APIs have been checked; build and behavior evidence remain required. No Enhanced Input migration or Blueprint assets are needed for this iteration. UMG/Widget Blueprints can later provide visual layouts and styling around the same C++ actions; construction geometry, validation and transactions remain in C++.

A cursor-based ray is a new targeting path. Identify how the chosen UI path obtains a viewport cursor position and creates a world ray, and how that ray is checked against the existing pawn-view origin, reach, and visibility policy. Preserve the existing evaluator and make no State authority changes in this assignment unless a concrete incompatibility requires a separately reviewed design change. Do not claim the cursor ray has been implemented if the environment cannot support or validate it.

## Focused acceptance checks

Add or adapt focused sandbox controls tests for these behaviors; keep them within the existing `ShatteredFrontier.Construction.` automation prefix and test harness:

1. Free cursor is active in construction mode; moving it updates the cursor ray/ghost and leaves the camera rotation unchanged.
2. Holding RMB enables look input and changes camera rotation from mouse delta; releasing RMB restores free cursor behavior. RMB press/release never changes State revision or removes a piece in either mode. LMB deletes only in Demolition mode.
3. The dedicated demolition action previews the cursor-targeted piece; explicit confirmation removes only the captured target/revision. Cancellation, stale revision, or a blocked/occluded cursor ray leaves State unchanged.
4. Cursor clicks over palette controls only operate the UI. Text entry and UI focus consume gameplay actions; clicking outside UI resumes world placement. No click-through or duplicated inherited E interaction occurs.
5. Plane and depth controls move the structural ghost in exact 0.5 m lattice increments, report the selected plane/depth, respect the sandbox bounds, and do not bypass reach/sight/clearance/support/revision rejection.
6. Line placement retains start/revision fencing and complete-line validation; focus loss during RMB hold does not commit, delete, or leave the cursor captured.
7. Focus/window deactivation and reactivation leave the cursor visible/free and all mouse-button states released.

Automated tests must not warp or reposition the operating-system cursor while a user-visible preview process is alive. Exercise cursor rays through a testable viewport/world-ray seam or controlled synthetic viewport input; do not manipulate the real desktop pointer. Keep any manual cursor interaction in a separate human preview step.

Camera look clears the sampled endpoint and any demolition confirmation but retains a current line seed with its original revision; revision changes, explicit cancel, focus loss, selection and depth changes still invalidate it. X/Delete toggle Demolition mode; LMB removes only in that mode. P switches surface/layer; Page Up/Down selects horizontal Z layers 0–19 in the existing 0.5 m lattice. These remain prototype bindings pending comfort review.

Return the prototype to the user for a comfort check with a short record of tested actions and limitations. Authors do not build; the parent serializes the required build and focused tests. Packaging and normal E1 startup/configuration changes are outside this packet.
