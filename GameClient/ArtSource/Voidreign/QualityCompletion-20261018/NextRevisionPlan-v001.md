# Next geometry and motion revision — proposal

This is a concrete redesign proposal, not approval or another bake. The existing topology/rig/motion attempt counts remain intact. Current evidence and ownership live in the quality job. Keep local work on CPU while Greenlands owns the GPU.

## Why the approach needs revision

The third rig fixes proposed attachment drift to 0.044 mm and preserves protected anatomy. Fresh default exchanges pass. It still fails 156 mm maximum surface motion and up to 69 mm penetration. A guide itself moves 154 mm at frame 210, so replacing skin weights alone cannot fix that motion result. Skin vertices up to roughly 0.18–0.31 m from assigned guide segments explain other clearance failures.

Rest component visibility is also insufficient: the controlled sampler finds 180 low-visibility candidates and 18 unresolved controls, while 31–32 rest-occluded parts become exposed in tested poses. Some may be intended nested parts; some may be buried generated shells pulled out by the provisional rig. Visibility, semantics and unwanted deformation must remain separate decisions.

## Proposed order and required evidence

| Step | Work | Gate before continuing |
| --- | --- | --- |
| 1. Rest component dispositions | Resume the exact 219-ID ledger. Inspect isolated/depth-tested pairs, first-hit identities, controls and relevant poses. Classify ornaments, visible hair, nested shells, debris and unresolved parts. | No mass deletion. An occlusion sample cannot authorize removal. Record explicit retained/rebuilt/removed selections and reasons. |
| 2. Geometry correction | Remove only confirmed artifacts; rebuild only missing external roots. Recheck the five previous continuations for actual need after shell classification. | Source-hashed selections, exact retained geometry/UV/material checks and inspected before/after silhouette. New topology invalidates downstream selections and evidence. |
| 3. Complete visible feature map | Define each intended clump/strip, root band, protected anatomy and ambiguous bridge. Include fetlock feathering or moving tack only when identified and in scope. | Reviewed both-side/front/rear/top coverage. Anchored roots remain semantic hair. Unresolved fused geometry is repaired or stays open. |
| 4. Strand-local guides and skin | Assign a clump to guides through its reviewed graph/root relationship. Blend neighboring guides only within the same intended feature region. | No unrelated nearby lock influence. Measure guide-to-surface offsets and cross sections; retain seam continuity, coherent attachments, four-influence and protected-rigidity gates. |
| 5. Contact solver revision | Diagnose full-duration motion and contact release. Evaluate constraint/contact velocity handling, kinematic-relative contacts and suitable swept surface representation. | Full-duration guide step/velocity/acceleration report, settled startup, feasible anchored roots, no missing required contact cases. No pass from contact counts alone. |
| 6. Actual surface validation | Evaluate the saved mesh through all required motion cases. Review full surfaces, attachment bands, contact boundaries and render-visible exposure. | Existing fixed limits remain unchanged. Do not hide internal artifacts, unrecognized anatomy or missed contacts by expanding root exemptions. |
| 7. Materials and pose | Harmonize view transitions and material channels; establish the intended idle/standing pose and sole boundaries. | Color-only checks remain exact; normal/roughness/metallic/emission need their own evidence. Front hoof candidates are 86–103 mm raised; preserve proportions and correct pose through a reviewed primary rig. |
| 8. Delivery | Fresh source renders, controlled relighting, oblique/motion review, default exchanges and any actually needed import contract. | All required prototype gates pass. Gait, primary skeleton and engine/runtime acceptance remain explicit separate scope until implemented. |

## Controls and algorithm ownership

Keep solver configuration in a new typed, pinned authoring profile with defaults, safe bounds, units, author permissions and next-bake effective time. Retain the existing profile and failed reports. Contact algorithms, component correspondence and skin assignment belong in versioned code, not editable expressions or undocumented console tweaks. Do not increase guide/bone counts without reviewing applicability and the intended target budget; the current 229-bone diagnostic is not an approved engine budget.

Increasing contact envelopes is useful only when the guide actually represents the intended clump. A large envelope must not compensate for a guide assigned across unrelated strands. Likewise, more solver iterations cannot fix a root placed inside an incompatible rigid volume.

## Small cases before another full model bake

Use finite CPU fixtures covering a finite-width strip on a plane, a moving capsule, pinned-root feasibility, contact release, a near-vertical orientation transition and two spatially close but semantically separate locks. Check actual strip surfaces and exact attachment roots. Then exercise the selected Voidreign contact window and full-duration trajectory. Passing these fixtures would support a new design review; it would not erase the failed model attempts or establish production behavior.

The 11-second diagnostic is a rigid horse harness: its labels “walk/run response” describe stimulus phases, not animated legs. A playable mount needs a primary body/neck/leg rig and deliberate gait, with secondary hair evaluated relative to the appropriate moving body part. Do not package this diagnostic as a locomotion asset.
