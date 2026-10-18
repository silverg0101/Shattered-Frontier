# SBX-06J: Doorway targeting and safe closing displacement

Status: architecture assignment; implementation and evidence pending. User playtest requests the whole doorway as the interaction target and player displacement when closing. Preserve the released v1 door geometry, standalone scope, shared revision, support lifecycle and transactional publication.

## Doorway interaction

Select the nearest owned door whose full world-space Envelope intersects the interaction ray within existing reach. This is a virtual target, not an additional physical or invisible blocking component. An open leaf need not be hit. Reject nonfinite/degenerate rays; output stays unchanged on failure. A closer wall, prop or other door must occlude the target. Resolve ties deterministically. Keep existing inherited E interaction exactly once when no doorway is targeted; a targeted door's rejected transaction must consume E without falling through.

Update State's authoritative sight check too: a clear ray into the virtual doorway is sufficient; requiring a hit on the first physical frame box would retain the old defect. Continue owner, reach, revision, environment and grace checks. Neither UI nor virtual targeting grants authority. Cursor targeting follows UI-01A when implemented; this assignment must not invent a second input model.

## Closing displacement

Only closing an existing open door may displace its current controller's owned upright construction character capsule. Opening, placement, other pawns and other blocking components retain current behavior. Do not ignore an entire actor or temporarily disable the old door/player collision. Keep physical old-door colliders active during the swept move.

Plan a bounded translation to the nearest front/back side outside the whole Motion reservation, expanded by the actual capsule radius. Keep height/rotation unchanged. Derive distances from released geometry and actual validated capsule dimensions, not a tunable force or console setting. Try the other side only with a fresh sweep from the original pose. If neither full swept path and endpoint are safe, leave the door open and the player unchanged. No teleport, penetration correction through obstacles, movement-mode change or velocity injection.

If a capsule exception is needed for the reservation query, use a State-private, current-revision sealed context for exactly that owned capsule, door and captured pose/dimensions. All other capsule-owner components, props, structural occupancy, other doors, reservations and unrelated pawns must still block. Validate endpoint clearance separately against both the active world and the candidate closed geometry; include the old open leaf in path collision.

Stage and preflight the candidate projection/support transaction before publishing. Use an ordinary deferred FScopedMovementUpdate for the capsule and retain rollback until all fallible work completes. Publish the closed projection/common revision before deferred movement notifications are delivered. On any failure, revert movement and discard staged geometry without changing support clocks, confirmation or revision. Character movement bookkeeping must remain valid on success and unchanged on failure.

Installed UE5.8 source review found that deferred movement can leave the physics BodyInstance at its old pose. Therefore moving the capsule and rerunning the unmodified world-overlap check inside that scope is insufficient. Do not end the scope early to make that query work: that would expose callbacks before transaction publication. A precise sealed capsule exception plus independent swept-path/endpoint checks is required unless another approach is proven against installed engine behavior.

## Assignments and evidence

Parent owns architecture, shared documentation, Controller integration and serial builds/runners. A stronger bounded author may own State/Clearance and new focused door interaction/displacement tests; no overlapping writers. Simple UI packet drafting uses Luna. Independent review must inspect the authority/rollback source and raw focused evidence before acceptance. No author builds/commits. Preserve the user's copied preview and its Saved directory, normal E1 maps/defaults/cook list, live LAN service and concurrent art.

Precise tests must cover open/closed doorway-center targeting without a leaf/frame hit, all four yaws, nearest/occluded/out-of-reach/invalid rays, exactly-once E routing, close displacement from both sides, alternate-side obstruction, both sides trapped, old-leaf path obstruction, extra owned blocking component, unrelated pawn, stale intent, unsupported grace, projection staging failure after planning/movement, and unchanged world/player/revision/clocks on rejection. Use the real construction character capsule for displacement. Adapt the existing door-input expectation that a player always blocks closing; retain unrelated closed/open walking checks. No OS cursor movement or manipulation of the live preview in automation.

Report implementation and automated acceptance separately from human comfort. This first endpoint-door prototype does not establish animated hinge physics, networking or production behavior. UMG/Widget Blueprints may present controls; C++ retains construction authority. Door animation, sound and appearance can later be Blueprint presentation calling the same C++ action.
