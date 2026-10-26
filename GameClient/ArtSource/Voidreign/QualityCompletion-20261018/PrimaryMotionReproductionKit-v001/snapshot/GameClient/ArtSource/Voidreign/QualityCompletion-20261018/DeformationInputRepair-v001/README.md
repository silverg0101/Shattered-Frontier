# Primary motion and fixed-rest repair

This is the current repair priority. Keep the old contact-passing preview and all attachment studies as historical controls; do not adopt the B067 rigid-fragment proposal before checking the corrected simulation inputs.

## Evidence

`SourceSurfaceStrain-v001` measures all 330 frames of CompleteMotionCache-v001. Despite contact/gap passes, its maximum absolute edge changes are390.9mm Mane and393.5mm Tail. Neither rest edges nor rest triangles are zero. `EdgeStrainStageComparison-v001` probes the same four exact edges through six stages: the distortion already exists in guide-only skin and is carried into raw cloth, contact correction and final MDD. In particular, both endpoints of the20.53mm Mane root-band edge9095/9114 are outside the original free mask, yet guide skin makes it411.39mm long.

The old cloth proxy inherited the secondary hair Armature as its goal input and used Dynamic Mesh, so that deformation also changed its rest shape. The repair keeps primary rigid assembly motion, disables the proxy's secondary Armature, and uses fixed original rest geometry. Source geometry/UV/weights, root/free masks, cloth response, sewing force, colliders and acceptance limits are unchanged. This adapter requires source-hashed protected-rigidity evidence and is restricted to the existing rigid-body harness. An articulated mount needs a primary-skeleton/root-target adapter.

## Current validation

- ManePrimaryOnlyFull-v001:240held+330output; settling RMS1.912mm/s, peak39.594mm/s pass20/80. Fresh330MDD replay exact0. ManePrimaryOnlyFullCheck-v001 passes all six original330frame checks: maximum step43.139mm, startup1.318mm, penetration1.921mm versus5mm limit. Its Tail is still historical.
- ManePrimaryOnlyFullAlembic-v001: all284762indexed vertices across330frames reimport within0.000544mm. Offline geometry only.
- Matched first30frame strain: Mane maximum edge error212.03 to2.245mm; worst-frame p99 relative change482.3% to0.401%. Diagnostics, not invented acceptance limits.
- TailPrimaryOnlyPilot-v001: settling RMS3.724mm/s, peak47.717mm/s pass20/80. Fresh30replay exact0; all33233canonical source IDs exactly match existing Tail playback, no remapping or discarded points.
- PrimaryOnlyCombinedPilot-v001 and its Check-v001 combine corrected Mane and30frame Tail; old Tail corrective disabled. All six checks pass; Tail maximum step2.926mm, startup1.742mm, penetration0.

## Finite completion sequence

TailPrimaryOnlyFull-v001 is active. PrimaryMotionDelivery-v001/finalize.py waits at most20minutes for that existing producer, verifies settling/point correspondence, combines the full caches, checks all330surfaces and rest retention, reimports animated exchange, measures full strain and renders four textured/diagnostic frames. It stops on failure, preserves attempts and logs exact commands. Run from repository root with `py -3 GameClient/ArtSource/Voidreign/QualityCompletion-20261018/PrimaryMotionDelivery-v001/finalize.py`; existing output directories are never overwritten. Inspect the exact active process before replaying; no duplicate jobs.

Finish with visual review and a new reproduction kit. No further blind physical tuning, attachment-fragment experiments or diagnostic expansion. Original parent counts remain3of3 and accepted-source pointer stays on the original base. These are architecture repairs, not production approval. Material, semantic, stance, gait and runtime/triangle/swept/self-collision gates remain explicit.
