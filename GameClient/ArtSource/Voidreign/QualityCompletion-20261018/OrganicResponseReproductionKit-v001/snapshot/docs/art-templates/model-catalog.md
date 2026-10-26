# Model work catalog



Model-only index. Image production remains in [asset-catalog.md](asset-catalog.md). Procedure: [model-pipeline.md](model-pipeline.md); records: [model-job.schema.json](model-job.schema.json). Open a named job and its evidence directly; do not reconstruct status by scanning folders.



Every resume executes the [required acceptance gates](model-production-gates.md), including provenance reconciliation and author visual/manual inspection. A separate agent review is optional. Current entries summarize historical evidence; a recorded tool pass does not override open quality gates. Correct inconsistent current stage summaries with a linked reconciliation while retaining their original attempt evidence.



| Job | Asset | State | Current stage | Exact next action |

| --- | --- | --- | --- | --- |

| [voidreign-quality-20261018](model-jobs/voidreign-quality-20261018.json) | Voidreign quality revision | in-progress | secondary_motion | Idle/walk/turn contact and exchange checks pass; skin not accepted. Third walk improves target hoof edge27.08 to4.06mm but introduces hoof-shell regressions. Primary/walk budgets3/3;4reviewed code corrections /8primarycandidates separate.4K material channels and head/saddle lighting views audited. Ground, hair, semantics and runtime remain open |

| [voidreign-motion-20261018](model-jobs/voidreign-motion-20261018.json) | Voidreign earlier benchmark | needs-review | topology_uv | Preserved20-bone baseline and Blender FBX source-contract evidence. Continue current geometry/complete-hair work through voidreign-quality-20261018 |

| [greenlands-waystone-reuse-20261018](model-jobs/greenlands-waystone-reuse-20261018.json) | Existing Greenlands Waystone | needs-review | geometry | Star attachment repaired in v003; geometry capped 3/3. Four crown degeneracies, emblem-to-shaft contact, source compatibility, material and UE 5.8 gates remain open |

| [greenlands-oak-node-model-20261018](model-jobs/greenlands-oak-node-model-20261018.json) | Oak tree resource prop | needs-review | geometry | Use the 18–22 m envelope; attempt 2/3. CPU review maps root sheet to mixed C0000 but no closed cut loop/underside closure exists; preserve final attempt until authored |

| [greenlands-iron-vein-model-20261018](model-jobs/greenlands-iron-vein-model-20261018.json) | Iron-bearing rock outcrop | needs-review | geometry | Geometry attempt 2/3. CPU review maps ribbons/skirt to mixed thin-shell C0000; no safe closed face boundary/replacement surface exists, so preserve final attempt |

| [greenlands-wayfinding-marker-model-20261018](model-jobs/greenlands-wayfinding-marker-model-20261018.json) | Wooden trail marker | needs-review | geometry | v003 removed 11 reviewed debris islands; geometry attempt cap 3/3 is exhausted. Principal lean and fused sheets remain. Define scale/support and authorize a new geometry revision before further repair. |



Active finite batch: [Greenlands model batch](../../GameClient/ArtSource/GreenlandsModelBatch-20261018/README.md) owns three serialized Pixal jobs and reuses the existing waystone. Source image catalog rows and image-session completion history remain unchanged. Structural work stays queued behind approved GEO-01 manifests and applicable GEO-02 evidence.

Construction 3D backlog: [construction-model-backlog.md](construction-model-backlog.md) is the living stable-ID queue. The first three material jobs ([rough timber](model-jobs/construction-rt-cellfull-material-20261019.json), [stone masonry](model-jobs/construction-sm-cellfull-material-20261019.json), [worked timber](model-jobs/construction-wt-cellfull-material-20261019.json)) exhausted 3/3 attempts in Model-v009. The next three ([ornamented worked timber](model-jobs/construction-wt-ornamented-material-20261019.json), [rough-timber roof slope](model-jobs/construction-rt-roofslope-material-20261019.json), [stone roof slope](model-jobs/construction-sm-roofslope-material-20261019.json)) exhausted 3/3 attempts in Model-v011. [Revision E review](../../GameClient/ArtSource/ConstructionGeometryPilot-20261018/Model-v011/review-notes-gpt-6.1-sol.md) records the exact failures; none is complete or production accepted. Do not retry these jobs without a new attempt budget. Remaining structural queue items require approved GEO-01/GEO-02 evidence and cannot be modeled from appearance-only references. Every started job has an authoritative schema-shaped record here.







Voidreign latest diagnostic: [rigid-core preflight decision](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/RigidSkinPreflightDecision-v001/decision.json) records two read-only proposals, neither promoted; [material channel review](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/MaterialChannelReview-v001/review.json) confirms nearly uniform metallic on nonmetal surfaces. Authoritative job remains in progress; anatomical collars and material masks are next.

Voidreign material attempt2/3: [hair dielectric prototype](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/HairDielectricMaterial-v001/README.md) has saved-source previews and scoped geometry/rig retention. Remaining mixed faces/material classes and shape/collision gates are open; not promoted. [Corrected edge pose review](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/RigidSkinEdgePoseReview-v001/visual-review.json) exposes the ankle transition regressions.

Voidreign ownership finding: [hind-leg fringe label correction](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/LegFringeLabelCorrection-v001/README.md) separates195misowned vertices from Tail; derived roots/motion remain invalid. [65patch boundary review](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/HairMaterialBoundaryReview-v001/review.json) tracks9reviewed isolates and56pending. No model promotion or material attempt3.

Voidreign parent verification: [actual fringe primary-follow review](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/LegFringePrimaryFollowReview-v001/README.md) confirms existing leg skin already works; no rebind authored. Saved local Blender review and corrected2.43sGIF show existing motion only. Secondary dynamics/collision and whole-model gates remain open.

Voidreign local secondary2/3: [physics decision](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/LocalFringePhysicsDecision-v001/decision.json) retains two unsettled/contact-limited actual-fringe proofs, neither promoted. Source sampling and cache replay pass; final local attempt reserved for a reviewed cause correction. Global budgets and accepted source unchanged.

Voidreign local contact follow-up: LocalFringeContactInspection-v003 rules out incident triangle root exclusion and distinguishes UV seams from canonical boundaries; held RMS motion still increases. Local2/3 unchanged; neither proof promoted. Reproduction kit488files verified with external inputs.

Voidreign local-fringe cloth branch closed3/3 needs-review: source-bound simulation winding corrected/verified, but startup/contact/shape remain unapproved. LocalFringePhysicsDecision-v002 owns disposition; no fourth tune or promotion. Closed convex alternatives rejected for source intrusion. Shared reusable orientation/collider preflight updated separately from MountProfile.md.

Voidreign mixed material boundaries: all65stableIDs reviewed using390paired context/isolation images on10plates; sourceUV2atlases/130isolation checks pass. Latest HairMaterialBoundaryReview-v003 keeps assignments open and distinguishes20interface/nonhair candidates from45strand-like proposals. No material attempt3 or model edit.

Voidreign: ReviewedDielectricFaces-v002 binds955mixed-face nonmetal response approvals (892strand/63bone) to source topology;1758mixed faces remain open, no shader edit/materialattempt3. PixalVolumeChannelAudit-v001 finds highmetallic/nearlyconstant roughness already in rawcached attributes, with historical cache/packer hashes matched and no GPU context. Do not swap channels or rebake blindly.

Voidreign UV mask authoring inputs: SourceUVTopology-v001, ReviewedMaterialMask2K/4K-v001, MaterialMaskResolutionReview-v002.4K reducesapprovedfaceswithoutpixels28to1; allsampledoverlapoutside955approved subset.37zeroUVfaces retained asreviewitems, no source/UV/material edit. Shared procedure material-mask-authoring.md linked fromREADME.

Voidreign projection allocation: [measurement](../../GameClient/ArtSource/Voidreign/QualityCompletion-20261018/ProjectionUVAllocation-v001/measurement.json) verifies retained edit weights and exact baseline UVs;4K provides expected fourfold pixel area. Fragmented head/saddle edit footprints are not full anatomical masks or contiguous resolution. No UV rebuild justified by this statistic alone; next resolve16material interfaces/full masks before final material attempt3.

Voidreign latest material responses: ReviewedDielectricFaces-v003 adds260reviewed faces (1215total;1498faces/12boundaries remain), preserving955earlier approvals. Mask2K/4K-v002 contractv2 adds common organic response and reuses hash-bound pixel ownership exactly; no shader/source or stage-budget change. Read the named job/remaining-review.json next.
