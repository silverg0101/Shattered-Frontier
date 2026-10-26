# Primary anatomical joint pilot

Diagnostic actual-mesh candidate: Mount-PrimaryRigPilot.blend. Visible autoplay review PID45104; previous unsaved windows preserved. This is a23bone primary skeleton and60frame flexion exercise, NOT idle/walk/run/turn locomotion or a grounded stance.

## Evidence

- Original indexed geometry, UVs, normals, colors, materials and images pass PrimaryRigPilotRetention-v001. Added primary weights/binds intentionally change:283623assigned vertices,1139unresolved nonhair vertices left unweighted. Existing secondary/harness modifiers disabled in this candidate; old source unchanged. Bone/weight differences are expected and are not covered by the geometry-only retention pass.
- Five fresh-reopened poses: restframes1/30/60 return within0.000638mm; fore-left flex at15 moves up to76.069mm; hind-right plus neck flex at45 up to164.751mm. Coincident seam separation is0 at these five checked poses. These are deliberate joint movements, not hair-step acceptance tests.
- Numerical primary weights: at most4influences, assigned sums within floating point tolerance. primary-weight-check.json records the exact measurement. No automatic anatomical acceptance.
- Parent inspected three textured poses plus domain closeups. The leg and neck deformation is visible without the old rigid horse harness. Lighting is a diagnostic setup. Joint volume, upper-leg blending, planted contacts and continuous intermediate frames still need review.

## Ownership and reproducibility

PrimaryAnatomyAtlas-v001 supplies six source-rest orthographic/perspective views. PrimaryAnatomyProposal-v001 has authored joint markers and box domains; boxes mistakenly include nearby hanging straps. PrimaryDomains-v001 replaces those boxes with authored tube seeds and mesh-geodesic ownership: fore.L9244, fore.R9876, hind.L12632, hind.R7174, body119606, unresolved1139. Existing Mane/Tail semantics remain separate. PrimaryDomainsReview-v001 closeups show the hanging straps excluded while connected limb feathering remains in limb proposals. This is pilot-level review, not final disposition for all hanging items.

Profiles with source hashes, authored joint positions, seed dimensions, weight settings, bone hierarchy and five poses are docs/art-templates/model-profiles/voidreign-primary-*-20261019.json. Exact tool/profile snapshots are adjacent to each output. Shared procedure is docs/art-templates/mount-primary-motion.md. Voidreign values must not become defaults for another mount.

## Next

VR-RIG-01: define source-bound hoof/sole patches and foot controls; propose a standing contact pose, preserving leg lengths and proportions, then check reachability, joint deformation and unwanted tack motion. Current forefeet remain raised from the source pose. Only then author real gait clips and transitions. Add secondary roots/colliders driven by the new primary rig afterwards. Unresolved islands remain a tracked attachment inventory, not implicit body/leg weights or deletion candidates. Historical hair failure/budgets remain unchanged.
