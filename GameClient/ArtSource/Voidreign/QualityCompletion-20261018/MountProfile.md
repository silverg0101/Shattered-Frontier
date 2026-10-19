# Voidreign mount profile and evidence index



This file indexes Voidreign-specific inputs and current evidence for the reusable [mount secondary-motion procedure](../../../../docs/art-templates/mount-secondary-motion.md). It records authored prototype settings and diagnostics, not defaults for other mounts or selected production settings. The controlling job record is [voidreign-quality-20261018.json](../../../../docs/art-templates/model-jobs/voidreign-quality-20261018.json); its quality target is `prototype`, and geometry, topology/UV, materials, rig, and secondary motion remain `needs-review`.



## Bound source and authored inputs



Current editable source:



- `DegenerateCleanup-v001/Voidreign-CompleteHair.blend`

- SHA-256: `6c6044a533c16fc7fad23d42c0407dde37a0252aca84f9be1bc9b9a2367364f6`

- Mesh `geometry_0`, source mesh hash `ba2d283db0be5e8cff4cc7549f1f51916cc0a4d18213773300976b11512ececa`.



Current authored configuration and correspondence records:



| Purpose | Path | Binding/status |

| --- | --- | --- |

| Semantic selections, roots, and protected regions | `HygieneSemanticSelection-v001/selection-report.json`, `selections.npz`, `guide-proposals.json` | Source and mesh hashed; report remains `needs-review`. Uses `docs/art-templates/model-profiles/voidreign-hair-selections-v003-20261018.json`. |

| Root repair and geometry correspondence | `RootRepair-v003/root-repair-report.json`, `root-validation.json` | Five authored clump-root continuations; this is a saved repair report, not a general root detector. Authored by `docs/art-templates/model-profiles/voidreign-clump-root-repair-v002-20261018.json`. |

| Attachment proposals | `HygieneAttachmentProposals-v001/attachment-proposals.json` | Proposal-only; authored profile `docs/art-templates/model-profiles/voidreign-feature-binding-20261018.json`. Proximity does not approve attachment semantics. |

| Guide paths and weight field | `HygieneGuideProposal-v001/guide-report.json`, `weight-field.npz`; prior/current baked guides at `GuideProposal-v003/guides.json` | Source-hashed proposal/evidence. The current simulation report binds its baked guide file. `HygieneGuideProposal-v001` is a refreshed semantic proposal and has not been simulated. |

| Motion, colliders, and skin settings | `docs/art-templates/model-profiles/voidreign-complete-hair-motion-v003-20261018.json` | Job-bound profile SHA-256 `9df3ed982c0f5f571210a98d42c1e6b395b80713c139f0b7b162ad861d5cca47`; contains authored timing, solver, collider, skinning/protection, and required-contact settings. Effective for a fresh offline bake only. |

| Prototype surface limits | `docs/art-templates/model-profiles/voidreign-hair-surface-check-20261018.json` | SHA-256 `f42b54b785653cd13262405b39e26a48e1fc606edeafb392b1372121e29e1abb`; diagnostic prototype contract, not production clearance policy. |



The current saved guide bake is `Simulation-v003/` and is tied to `voidreign-complete-hair-motion-v003-20261018.json`. `CompleteHair-v003/` is a diagnostic rig candidate; it does not replace the current source above or establish full-mount acceptance. The hygiene cleanup made a narrow geometry change and the reports were refreshed against it; no new rig or motion bake followed that cleanup.



## Diagnostic profiles and studies



These profiles and reports preserve investigations; none is a selected production default or authorization to deliver a replacement rig.



| Study | Input/profile or evidence | What it establishes and does not establish |

| --- | --- | --- |

| Full-duration guide trajectory | `docs/art-templates/model-profiles/voidreign-guide-trajectory-check-20261018.json`; `TrajectoryCheck-v002/trajectory-check.json` | Read-only saved-bake guide-step diagnostics. Guide movement is not surface movement. |

| Root feasibility | `GuideClearanceBaseline-v004/guide-clearance.json`; authored surface profile above | Free-guide and fixed-root feasibility are distinct. Current status is needs-review. |

| Coverage proposals | `docs/art-templates/model-profiles/voidreign-coverage-guide-study-v002-20261018.json`; `CoverageGuideStudy-v002/coverage-report.json`, `CoverageViews-v002/render-report.json` | Rest guide-coverage proposals and depth-tested views; paths are not reviewed clump identities, skinning, or finite-width clearance. |

| Frozen turn trace | `docs/art-templates/model-profiles/voidreign-turn-trace-20261018.json`; `TurnTrace-v001/trace-report.json` | Observation-only exact replay of saved substeps; observed displacements do not assign force/energy causation. |

| Split-contact velocity design | `docs/art-templates/model-profiles/split-contact-velocity-study-20261018.json`; `ContactVelocityStudy-v002/study-report.json` | Analytic fixtures for proposed helper code. The module is not integrated into the delivered mesh motion. |

| Damping/spring ablations | `MotionDesignStudy-v001/study-plan.json`; `study-results-v002.json`; `MotionDesignReproductionKit-v001/manifest.json` | Frozen, single-variable diagnostics with failed contact/style outcomes retained; not selected production tuning. |

| Coupled contact study | `CoupledContactStudy-v001/` and `CoupledContactStudy-v002/`; each contains its own `design-profile.json`, solver/helper snapshots, guide data, and per-case outputs | Experiments against frozen inputs. v002 and v003 fail the coupled moving-collider case; v004 uses net reconstruction and passes the finite fixture contracts. These remain limited diagnostics. Neither version repairs or replaces the full mount. |



Current particle checks do not clear actual-surface checks. The current source remains unchanged while these studies run; study outputs must not be presented as a new delivered rig or as proof that the full model is fixed.



## Reproduction lock and limits



The job's reproduction lock is `docs/art-templates/model-profiles/voidreign-quality-reproduction-20261018.json`; its recorded execution is `Artifacts/Art/Voidreign/quality-completion-20261018/Reproduction-v001/run-report.json`. This lock reproduces a bounded CPU workflow against pinned source/profile/tool inputs. It does not attest to GPU/model regeneration, a fresh source change, a replacement rig, production acceptance, or engine behavior. Read individual run/check reports and their input hashes; a successful process exit is not a gate summary. Existing snapshot kits preserve earlier tool/profile copies and their historical inputs rather than updating to the newest source automatically.



## Open acceptance gates



- **Actual mesh motion:** `HygieneSurfaceCheck-v001/surface-report.json` freshly retains surface failures. Inspect `surface-review.npz`, worst vertices/frames, root behavior, skin weights, gaps, and penetration. Particle and centerline checks cannot close this gate.

- **Materials:** `OppositeDetailCheck-v001/validation.json` and `OppositeDetailImport-v001/validation.json` cover a bounded color detail revision. Painted detail/highlights, eye treatment, opposite-side coverage, and full PBR/material review remain open.

- **Anatomy and stance:** the legs remain static in a rigid mount harness. Intended movement, contacts, and stance quality across real gait poses have not been accepted.

- **Root, coverage, and representation:** the fixed-root feasibility review and guide/clump correspondence remain open. Candidate paths and point-distance images do not prove surface coverage, skin behavior, or contact feasibility.

- **Runtime and engine:** bone/runtime budgets are not approved; engine import/playback, triangle-accurate or swept/self collision, and runtime behavior remain unvalidated.

- **Delivery:** current exchanges and import checks are diagnostic evidence only. A source-bound export/reimport comparison and the required appearance, surface, stance, and engine gates are needed before any stronger status.



## Reauthor for another mount



Reuse the procedure in the shared mount document, but author and review the following for each new mount from its own source and use case:



- Source, mesh/object identity, units, rest pose, topology, animation and reference provenance; regenerate all source-hashed selections after source or topology changes.

- Feature/clump semantics, protected anatomy/equipment, roots, attachments, cut/feather regions, guide paths, and skin/weight correspondence. Do not transfer Voidreign vertex IDs, component IDs, coordinates, masks, guide counts, or weight fields.

- Rig/collider identities, collider shapes and dimensions, coordinate frames, rider/environment contact cases, gait/turn/stop timing, and required animation interval.

- Motion and surface profiles, including typed values, units, defaults, bounds, permissions, effective-time rules, acceptance limits, and the evidence needed to support each value. Establish limits for that mount and product policy; this profile supplies no universal threshold.

- Representation choice, runtime/bone budget, export/import contract, and engine validation scope.

- Finite stage attempt budget, repair ownership, provenance, and stop conditions. Keep failed attempts and diagnostics with the mount's own evidence index.



## Current contact-design continuation



The shared workflow is docs/art-templates/mount-secondary-motion.md; this index owns Voidreign settings/evidence. ContactDesignRevision-v001/design-plan.json preregisters three full-guide design slots while preserving the parent failed-stage counts. CoupledContactStudy-v002/v003 preserve failed split formulations; CoupledContactFailure-v002 records their counterfactual diagnosis. ContactVelocityStudy-v003 has22isolated checks. Net reconstruction/minimum-change velocity in CoupledContactStudy-v004 passes four finite fixtures; it is not a surface pass.



Asset-specific profiles: voidreign-net-contact-solver-design-20261018.json (net reconstruction), voidreign-style-frequency-damping-20261018.json (nominal ratio1), voidreign-mild-overdamping-20261018.json (final preregistered ratio1.25), all under docs/art-templates/model-profiles/. The original authored drag remains5/s; the derived damping algorithm has declared ratio/effective-coefficient bounds and reports its effective coefficient. It is not a silent override of the old authored-drag range, nor a universal next-mount default. Continuous-oscillator critical/overdamping language does not establish the discrete coupled strand response.



NetContactTrajectory-v001 remains155mmpeak with118guide warnings. StyleDampingTrajectory-v001 improves to82.694mmpeak with14warnings/9frames, still above80mm. EndSTOP RMS improves89.28→80.49mm and max309.15→202.14mm, but transient STOPmax728.7mm remains important. Both retain22mmfixed-envelope conflict and unaccepted actual-surface behavior. CoupledContactStudy-v005 validates the nominal damping on finite fixtures. Final mild-overdamping fixtures/trajectory are pending; no replacement rig is accepted. GuidePathReview-v002 binds the explicit assembly frame to semantic coordinates (max1.98e-7m); visual paths remain unapproved.



### Final bounded guide result and vertex evidence



MildOverdampingTrajectory-v001 completes slot3: guide peak79.000078mm, zero step warnings; all four finite coupled fixtures pass in CoupledContactStudy-v006. Effective damping23.048861/s, authored drag5/s and spring85/s² unchanged. Guide clearance still fails the fixed22.086202mm envelope condition; free-node residual0.004901mm. EndSTOP RMS80.535mm/max202.497mm; transient TURNmax803.948mm/STOPmax770.153mm require style/lag review. No further damping parameter search is authorized by this design plan.



SurfaceVertexReview-v002 has16 CPU rest/posed paired patch views bound to the old actual-mesh surface failures. All camera bounds/depth checks pass and measured coordinates reproduce within1.20e-7m. Ground patch appears to be a narrow tail tip; neck full-mesh views remain largely occluded, so topology-local isolation does not establish ownership. No selection was reclassified. v001 contains a failed positive-X ground framing view and replaced initial images before freezing; v002 is separate and preserves the final v001 evidence.



The one preregistered actual-mesh experiment uses voidreign-contact-mesh-experiment-20261018.json and Build-FeatureRigExperiment.py. Known fixed-envelope, semantics, skin, materials, stance, import and runtime failures remain explicit; no accepted model is replaced by this experiment.



### Actual-mesh contact-design experiment



ContactMeshExperiment-v001 is the single preregistered diagnostic evaluation of the final guide design, not an accepted replacement. ContactMeshRetention-v001 verifies exact indexed rest positions/faces/edges/loops/UVs/colors/normals, listed material signature and connected image pixels. Regenerated named weights are exactly identical (0 changed vertices) and bind data identical. The common material signature is not an arbitrary nested-shader equivalence proof.



ContactMeshSurfaceCheck-v001: all330frames actual body-relative surface step69.572449mm PASS versus old~156mm; startup0.206558mm PASS, proposed gap increase0.033781mm PASS, protectedrigidity/seams PASS. Sampled capsule clearance remainsFAIL: neck42.976mm,fararm66.310mm,post67.197mm,ground59.964mm, all versus unchanged5mm ceiling. Post penetration regressed from8.247mm despite the guide-step improvement. ContactMeshImport-v001 passes defaultGLB/FBX roundtrip, no connectivity restore required; this is exchange evidence only. Current accepted-source pointer remains DegenerateCleanup-v001.



FailureFeaturePaths-v001 traces exactly four current failed vertices through existing semantic/root graphs, without changing geometry/selections/guides/weights. FailurePathReview-v002 renders these proposals against rest mesh. Tail targets sit309.293/246.847mm from oldguides, exposing a concrete guide-to-surface coverage issue; their graphpaths contain0virtualedges. Neck path is obscured/semanticallyunresolved; fararm path contains1virtualedge. Paths are not approved complete clumps or centerlines. Initial path renderer v001 stopped before creating a folder due unsupported new diagnostic status; failed log retained. v002 adds that explicit unreviewed status to renderer; no acceptance check relaxed.


### Actual-surface and attachment continuation

TailLocalDomains-v001 isolates a232vertex post-side branch and658vertex forked lower branch after source-bound plane cuts; TailLocalDomainReview-v001 shows exact selected surfaces in rest/pose/context/isolation. These are local branch domains, not whole-lock or rig approval. Earlier35mm tubes were partial surface strips. The broad TailBranchDomains-v001 lower cut included8371canonical nodes and is unsuitable as a single lock.

TailCrossSections-v001/TailLocalSections-v001 record nested plane contours and candidate centers/widths; no shell deletion. Capture-FeatureAttachmentMotion.py initially included source seam/cap boundaries (TailAttachmentMotion-v001, unsuitable root fit). Canonical seam-aware cut-band extraction in TailRootMotion-v001 still finds83.865/14.321mm nonrigid root residuals, above the authored5mm diagnostic criterion. A body-fixed or arbitrary rigid local root is incompatible with these existing skin boundaries.

Build-ActualSurfaceClothPilot.py creates a canonical copy of actual selected hair solely for CPU simulation, retains original indexed mesh/UV/weights, and samples simulated coordinates back by exact source indices. ActualSurfaceClothPilot-v001 had inherited4.352624scale and hidden collider evaluation; v002 corrects to metre scale and evaluated wire colliders, source/proxy evaluated positions match0.000417mm.30frame attachment/seam/protected/startup/step checks pass; initial ground overlap32.888mm atframe1 and untouched Mane collisions remain. ActualTailCloth-v001 is the finite330frame Tail-only comparison, same cloth controls and original collision exclusions. Disk cache is required and this is not a skeletal exchange.

SurfaceContactCorrective-v001 uses3passes of actual free-surface analytic capsule/ground projection. It clears sampled vertex collision to~0.000610mm, BUT FAILS full-duration steps241.644mm and attachmentgap121.515mm. It is not a fix or accepted source. Per-feature SurfaceContactCorrectiveDiagnostics-v001: Tail gap0.000615mm butstep241.644mm/frame205;Mane step133.917mm andgap121.515mm/Mane:B066/frame161. Both B066 endpoints are tested free Mane vertices, separated11.282mm atrest; independent nearest-side projection separates the linked root. This is evidence for coupled attachment constraints and collision history, not for exempting or deleting the clump.


### Coupled surfaces and checked contact correction

The current scoped result is `CompleteMotionCache-v001/Voidreign-ActualSurfacePilot.blend`, with two adjacent active MDD dependencies. This remains a prototype, with no production/import-candidate promotion. Full330 original-surface results are `ManeSurfaceContactsAllFrames-v001`; final cache-conversion recheck is `CompleteMotionCacheCheck-v001`.

| Evidence | Result |
|---|---|
| Tail |330contact frames clear;76.444849mm maxstep,12.436173mm startup,0.000427mm proposed-gap increase |
| Mane |330contact frames clear to0.000573mm numerical residual;55.953648mm maxstep,10.099876mm startup,0.033795mm proposed-gap increase |
| Source retention | ActualSurfaceRetention-v001: indexed geometry/UV/colors/normals/common material signature/images exact;0weight changes;binds exact |
| Held startup |240positive held frames;16.580mm/s RMS passes20,301.066mm/s peak FAILS80. Not settled acceptance |

Mane uses26758canonical proxy nodes and89explicit proposed sewing relations; max sewing force200 Blender solver units. Original source UV/topology/weights are unchanged. ActualTailCloth-v001 uses33233canonical nodes with original root/binding pins. These settings and feature labels are Voidreign-specific adapters, not universal defaults. Self/cross-hair collision is off. Existing masks,3projection passes and acceptance thresholds stayed fixed.

Owning sequence: actual Tail cloth -> Tail-only startup contact projection -> coupled Mane cloth with positive held startup -> measured MDD retime -> Mane-only contact projection -> full unchanged surface checks -> measured output capture -> cache conversion. The first global projection failed jumps/gaps; coupling before projection avoids those measured failures in this candidate. It does not prove physical stability, arbitrary clump semantics or swept/triangle/runtime contacts.

Negative-clock warmup did not produce effective dynamics. Renaming native BPhys files failed fresh-frame comparison by680mm. Freeze measured points into MDD instead. CompleteTailCache-v001 failed relative path packaging; v002 uses destination-relative cache paths with save-as remapping disabled and verifies every frame. Do not reuse failed candidates. Both cache converters disable old deformation to prevent double motion; inherited active MDD files are copied/hash-bound into the output folder. Historical full-cloth report wording says endpoints pinned, but attachment_mode=sewing is authoritative; producer wording corrected in later code snapshot.

Replay the exact profile and producer snapshot associated with each attempt; profiles pin existing source/report/NPZ/cache hashes. Use Blender5.1.1 with --background --factory-startup --threads 4 --python-exit-code 1. Stage tools and profiles are linked from the dependency map. Cache conversion is not a physics rerun. Start outputs in new immutable folders. Every profile retains old exhausted parent-stage counts.

Remaining: held peak convergence, semantic approval of proposed links/domains, strain/style review, material/PBR and head/saddle review, stance/sole authority, actual animated engine exchange/runtime. Default skeletal FBX/GLB checks from ContactMeshImport-v001 do not validate cloth/MDD animation. The source horse harness remains rigid; RUN RESPONSE is a stimulus label, not a leg gait.


Final CompleteMotionCacheCheck-v001: six checks PASS at every330frames, maxcapsule residual0.000682mm. CompleteMotionCacheRetention-v001 exact indexedrest/appearance,0weight/bindchanges. CompleteMotionAlembic-v001 full284762vertex/330frame fresh reimport max0.000544mm; geometry cache only. ManeWarmupDiagnostic-v001 exactly reproduces failed peak and locates distal patch133921 (canonical91569,rootdistance269.4mm), not a sewn endpoint, awayfrombodycapsules; no automatic pin/delete/tuning justified. ContactSurfaceDelivery-v001/README.md is the current concise evidence index.


### Attachment support continuation

AttachmentSupport-v001/README.md and queue.json index five exact source islands and remaining actions. C0122/B06716-point leaf has one link: rest visibility0, but posed122/299visibility92.97/99.22% with full controls. Root-frame proposal parent11780/11782/11772 restores rest offsets without deleting/weight changes; child peak301.066→3.180mm/s, global remains128.246>80. Full330surface/retention passes. Original spring28components all pin-reachable;117geometricislands include45unpinned single-link cases. This is a representation/ownership problem, not permission to pin/delete them all.

ManeWarmupDiagnostic-v005 and ManeIslandMotionAnalysis-v001 separate rigid motion from shape change. C0007/C0048 are mostly whole-island motion; C0002has6stationary non-collinear pins and real shape motion. The full240held-frame recording and original3-8s convergence curve show no passing frame. No warmup-limit extension, early-frame selection or blanket damping change. Exact bounds/settings and conditional status remain in pinned profiles; shared procedure contains no mount IDs/defaults. Original accepted-source pointer and parent attempts remain unchanged.


### Corrected primary inputs and fixed rest

See PrimaryMotionDelivery-v001/README.md and DeformationInputRepair-v001/README.md. Full240held+330output Mane/Tail use the rigid-assembly-rest adapter, unchanged response/masks/sewing/limits. Both settle and replay exactly; corrected combined source passes collision/gaps/startup/seams/protected and rest retention. Tail frame175/sourcevertex281103 has86.830mm step versus80mm, retained as failed. This rigid harness adapter must not freeze articulated primary anatomy. Older guide/contact/attachment experiments above are historical, not the current preferred deformation path.

Full combined exchange passed330frames/284762vertices with0.000573mm error. Full-strain maxima are7.715mm Mane and3.616mm Tail versus390.856/393.500mm legacy; local relative maxima still require review. Delivery index retains failed Tail motion limit and remaining semantic/material/stance/runtime gates. Four textured and four diagnostic saved-source previews retained.


### Primary anatomical joint pilot

PrimaryRigPilot-v001/README.md indexes23primary bones, four independently authored leg chains and60frame flexion exercise. Five fresh poses have0seam separation; rest returns within0.000638mm. Raw indexed rest/UV/material retention passes; new283623vertex weights and binds intentionally differ,1139unresolved nonhair vertices remain unweighted. Source/geodesic domains, landmarks, bone/weight/pose settings are in dated20261019profiles; shared procedure is docs/art-templates/mount-primary-motion.md. PrimaryRigReproductionKit-v001 verified78entries, notselfcontained. This is1of3new primary-rig attempts, not a reset of old hair budgets. Next footcontrols/standing contacts and skin review before gait clips; existing forefeet remain raised. GUI45104 shows primary joint pilot; earlier unsaved windows preserved.


### Standing foot controls and skin followup

PrimaryStandingPilot-v001 has four fixed-lengthIK/hoof-orientation targets and110mm authored body lowering. Reviewed sole-face patches start103/86/49/10mm aboveground. Scoped sole contact/rigidity, target residual, bone lengths, neutralrest and plantedhold0slip pass; full60frame reimport error0.000258mm. Wholemountground remains failed (Tail89.634mm, nonhair48.704mm), not hidden by hoof passes.

Full-source strain reveals hard body/limb skin cuts. PrimaryStandingSkinRepair-v001 changes only a120mm geodesic ownership-boundary band: first bellyedgeerror103.090→1.011mm. Global worst remains71.090mm at163795/163796 across same-body-owner hind.R root-box eligibility minimumZ1.08m. Jacobi512solver ended0.00018511>0.00001 tolerance. PrimarySkinBoundaryReview-v001 records exactcause and nextcoverage/solver design. Primarybudget3of3needs-review; no blind retries/limit changes or gait acceptance. Rawgeometry/UV/material retention passes; primarybinds unchanged from first23bonepilot while weights intentionally differ.

LooseItemInventory-v001 indexes1139unresolvednonhair vertices across23components/subsets, with exactrebuild verification and source-scoped IDs. Context views of largestthree are occluded; isolation/attachment review remains. These are not23deletion candidates; fused hanging tack is outside that subset. Sharedtools/settings boundary remains mount-primary-motion.md versus datedVoidreignprofiles.

### Articulated gait and collar review — current20261019

Actual idle181frames, walk73frames and walking turn73frames have scoped contact/seam/bone/loop and exchange evidence. They remain prototypes. Walk v002 continuous top4 reduction worsens worst edge65.38 to85.18mm and is rejected as an improvement. Walk v003 fixes the original binary15mm XY hoof cutoff edge27.08 to4.06mm, but regional review finds up to20.605mm newly introduced hoof-shell stretch; it is not accepted as a full hoof fix. Body/shoulder max65.381mm remains. Anatomical hoof selection and body/joint skin design precede further rig corrections. Primary automatic budget3/3 and walk budget3/3 remain exhausted;4separate code reviews /8totalprimarycandidates are recorded explicitly.

All23 disconnected subsets have isolated/context views and unsigned support proposals; no semantic deletion/binding approval.22sampled minima exceed5mm, which is not exact triangle separation. Fused tack remains separate. Shared steps stay in mount-primary-motion.md; settings and decisions are in dated profiles and attempt folders.

MaterialChannelAudit-v002 confirms retained4K albedo and packed roughness/metallic, no normal input and zero emission strength. Existing opposite-side head/saddle refinements are retained; full material classes/relief/eye treatment still need work. Only walk v003 GUI62448 remains open; older reviews closed and the first hair prototype's unsaved state was preserved. No production, skeletal runtime or Unreal acceptance.

## Rigid-core preflight and material channel finding
Two read-only proposals on WalkPilot-v001 preserve reviewed rigid hoof cores and4influences. Both match Blender to0.000442mm;1,654 changed proposal vertices/4,235 incident edges/73frames. Linear regional maximum27.585 to19.178mm but1,636edges regress (largest+7.080mm). Harmonic Dirichlet solves converge below1e-8, maximum15.650mm but1,504edges regress (largest+8.053mm). Core/core errors fall to numerical noise; transition collars remain the cause. Neither is a saved skin or promoted fix. Primary8candidate and3/3walk counts unchanged. RigidSkinPreflightDecision-v001 indexes evidence.

Six head/saddle emission previews and MaterialChannelAudit-v003 confirm packed Blue metallic mean0.997903, Green roughness mean0.844575; camera views show near-uniform metal response on nonmetal surfaces. MaterialChannelReview-v001 owns reviewed limitations and semantic mask/relief/emission work. No material/source changed.

## Corrected pose evidence and hair material prototype
RigidSkinPoseReview-v001 is INVALIDATED: raw object-coordinate lengths/radius were not metres. v002 fixes this and requires exact agreement with frozen preflight; RigidSkinEdgePoseReview-v001 isolates2canonical rings and exposes core/collar and collar/exterior stretches. Three cases/nine views in each corrected set were inspected. No new skin/rig/gait.

HairDielectricMaterial-v001 is explicit material attempt2/3:124288whole Mane/Tail proposal faces receive a copied shader with metallic0;2713mixed faces retain original. Fresh assignment and common node-structure checks pass; source geometry/UV/image pixels/weights/binds retained. Raw exact-material retention intentionally fails only face_materials and material_signature; separate scoped summary retains that result. Five CPU saved-source previews reviewed, mane response improved, mixed roots/otherclasses and full animation/export/runtime checks remain open. No accepted-source pointer or GUI replacement.

## Appearance/motion ownership correction
Hair material boundary inventory2713mixed faces/65patches;13views inspected (9isolates,56patches still individually unreviewed). Direct support inventory166selected components found3Tail/leg conflicts. Six context/support images confirm three regions195vertices on hind.L-attached fringe; reviewed label-only correction moves2to3, preserves allother/coincident labels and invalidates derived motion. Corrected expected-support inventory has0conflicts; not complete semantics or bindings. Main Tail failure vertex281103 remains in main Tail, so no speed/collision fix claimed. Material prototype2/3 and rig/walk counts unchanged.

Guide/diagnostic-rig producer guards now reject unknown labels/missing derived fields before authoring; actual corrected-input preflight stops under old2feature and incomplete3feature contracts. Parent field contract remains compatible. Other historical adapters are not automatically covered.

## Full leg-fringe primary-follow verification
FullLegFringeRegion-v001 selects363vertices; upper support band empty/rejected. Exact cut roots:202canonical nodes/37root nodes,33external support nodes allhind.L,361full/72boundary faces, maxrootdistance185.958mm.59root and61support source vertices use only correct4hind.Lbones over73frames. Full363vertex region already weighted: source parent skin is correct despite prior Tail semantics. Hypothetical304vertex weight changes are numerical only (maxpose delta0.000144mm), so no skin authored.

LegFringePrimaryFollowReview-v001 saves exact source-index baked local actual geometry, all73reopened frames within0.0000596mm;13frame GIF v002 lasts2.43seconds and is explicitly PRIMARY FOLLOW ONLY. Three stills inspected. Original fast GIF invalidated independently. No secondary dynamics/collision or new rig/gait/material source; current GUI preserved. Next local pin-boundary review/fixed-rest secondary/deforming colliders.

## Local secondary feasibility2/3
LocalFringeClothProof-v001 fixed-rest surface cloth: heldspeed0.795m/s, maxstep82.891mm, edgechange27.001mm. v002 isolated3components with internal springs: heldspeed0.440m/s, maxstep67.859mm, edgechange22.798mm, sampled leggap5.46micrometres. Both rootswithin0.0000298mm, source sampling/reopened replay exact; neither promoted. Topology101/65/36nodes with16/12/9boundary nodes all pinned; no free boundaries. Initialroot/contact hypothesisruledout (allbaseline free gaps>=3mm,no incompatibleedges); do not expandexclusions.

Worstv2gap canonical22042/sourcebodyface245249 atsource14/scene44, rootdistance11.080mm, orientedlocalnormal gap negative5.46micrometres (not solid penetrationproof). LocalFringePhysicsDecision-v001 owns remaining1localattempt and exact next diagnosis. Global budgets, mainmodel and GUI unchanged. NativeRNA clarification: objectcollision group excludes all-roottriangles, not individualvertices; storedsettingsunchanged.

### Local contact topology follow-up
LocalFringeContactInspection-v003 binds canonical22042/frame14/bodyface245249 to seven collision-eligible incident simulation triangles. Raw indexed boundary distance0.356mm is a UV-seam-sensitive measurement; canonical boundary distance11.912mm with exact coincident seam copies. Collider244 canonical boundary edges/42 nonmanifold edges remains open. Held free RMS speed0.09417 to0.14041m/s overframes26–30 increases; no settled-start approval. v002 terminology clarification saved independently. No unique collision cause yet; review a closed deforming proxy and volume representation before final local attempt. Local budget2/3 unchanged. LocalFringePhysicsReproductionKit-v001 verified488files/11,853,144copied bytes, external sources required.

### Collider fit and orientation correction
Whole-context convex envelope rejected:73frames, worst7.266mm intrusion. Anatomical triangle partitions reduce intrusion to4.598mm;14free vertices inside and26below3mm. Neither installed. Limb-only context assumption was false (pelvis/spine influences); proposed parts retain all source deformation.
RestVolumeInspection-v003 identifies65node component1 reversed orientation:0/53eligible inward rays,43reverse hits, negative fanvolume. Temporary automatic cap fails manifold and is not accepted. WindingCorrectionReview binds exact canonical hash; LocalFringeClothProof-v003 reverses only target simulation faces. Verification gives43eligible/0reverse and positive fanvolume; others unchanged. Heldspeed0.289625m/s improves, maxfreestep71.917mm worsens, edge22.591mm and sampledgap6.14micrometres remain. Local3/3 exhausted, no promotion/fourth bake, original mesh/weights/GUI unchanged. Decision-v002 owns disposition. Shared orientation/collider procedure is mount-secondary-motion.md.

### Complete mixed-boundary visual inventory
HairMaterialBoundaryAlbedoReview-v001 has224paired context images; IsolationAlbedoReview-v001 adds166images including all65isolations and prior9contexts. Sheets-v001 indexes390images on10plates, all inspected. Latest HairMaterialBoundaryReview-v003 records65individual observations:45dark/red strand candidates and20interfaces/candidate nonhair classes. Includes bone ornaments, ear and harness; do not fill all2713mixed faces as hair. No shader/material attempt3 or assignments changed. Next exactID c95bf113bd82 bone/ornament interface, then hardware/ear/mixed transitions and fullhead/saddle masks.
UVCheck-v001 verifies fullsourceUV onbothatlases and130isolated meshes. Firstbatch endtoolhash mismatched because canonical renderer changed while running; immutable clarification binds actual saved execution snapshot. SourceUVMap is solelayer and exact; raw hash mismatch not rewritten. Renderer now records saved execution snapshot hash; launch frozen runners going forward. Review-v002 one suffix typo misclassified219022738900, explicitly superseded byv003; face IDs/source images unchanged.

### Reviewed nonmetal subset and upstream PBR evidence
ReviewedDielectricFaces-v001 binds exact955mixed faceIDs on49patches forMetallic0 authoring response:892strand and63bone. Fourbonecandidateisolations inspected fullresolution, plus paired contexts. This is not a shader/source edit or motion class;1758mixed faces across16interfaces remain unapproved. Keep last materialattempt until masks are ready.
PixalVolumeChannelAudit-v001 checks2,745,908x6rawCPUattributes from historical volumeSHA b8931bd8...:metallicchannel3mean0.999711/std0.001583,roughnesschannel4mean0.843855/std0.001787. Savedlayout/installedpipeline and exact historicalrebakerSHA5f65031e... agree; packingRGB=[0,Roughness,Metallic]. Alpha separate5 varies. Highmetallicalreadyraw; no channel-swap evidence, no upstreamtrainingcause inferred. NoGPUcontext/inference/bake/environment change. Future materialfix needs masks or reviewedpainting, not just moretexels.

### UV mask inputs and resolution control
SourceUVTopology-v001 exports exact297119retained triangles/UVMap. ReviewedMaterialMask2K-v001/4K-v001 are class-ID data, no material edit. MaterialMaskResolutionReview-v002 checks frozen arrays:28approved faces miss2Kpixel centers,1misses4K; no955approved faces participate in sampled overlap. Missing4Ksourceface119671/SMB-0d7599c806-0ded486c5b5e has0.389768pixelUVarea and6.57763e-6m2restarea. Keep exactface response fallback.
Globaloverlap78pixels2K/303pixels4K staysunknown.37triangles havezeroUVarea butnonzero3Darea; sourceIDs/areas inreviewv002, no automaticdelete/reunwrap/exception. Atlas4Ksampledoccupancy~44%, not a packing-quality judgement. Greyunknown dominates becauseonly955mixed faces were approved; inherited wholehairprototype is not newlyapproved bythismask. Reusable face/texel and image-painter rules inmaterial-mask-authoring.md. No stagecounterreset or shader/source/geometry/UV/GUI change. Next measureactualhead/saddleprojection UVallocation, then resolve16interfaces/fullPBR masks; do not redrawblindly.

## Projection UV allocation (2026-10-19)

ProjectionUVTopology-v001 checks four retained Head/Saddle +/-X projection attributes and exactly matches SourceUVTopology-v001 triangles/UVs. ProjectionUVAllocation-v001 measures the max-at-vertex diagnostic union, excluding overlaps. At2K/4K head support124081/496140pixel centers and weight-equivalent area60137/240624pixels; saddle224780/899666support and111289/445469weightedpixels. Area-equivalent square sides245/491(head),334/667(saddle) are scattered footprint statistics, not contiguous detail resolution or complete anatomical coverage.4K preview inspected. No evidence here alone justifies UV rebuild, source repaint or budget reset. Final material attempt3 remains reserved for complete reviewed masks; no shader/source/geometry/UV change. Profiles: voidreign-projection-uv-export-20261019.json and voidreign-projection-uv-measurement-20261019.json. Reusable measurement procedure stays in material-mask-authoring.md.

## Additional reviewed nonmetal responses (2026-10-19)

ReviewedDielectricFaces-v003 adds260faces without changing previous955: ear67a2125f483c40 and2516a986745a34, rear root/body strand6c0faadfb656172 share organic Metallic0 response; hairtip5f9296f0a8b714 adds strand response. Totals1215approved (906strand/63bone/246organic),1498unapproved/12patches. The earlier scalp candidate name on6c0faadfb656 is incorrect; overlays locate the rear rump/tail-root. No motion ownership edit follows that appearance observation.

ReviewedMaterialMask2K/4K-v002 uses class contractv2 with green code3 common organic response and exact frozen ownership/conflicts/counts fromv001. Revision-check validates all cached arrays and additive approvals.32/2approvedfaces lack samples at2K/4K;4Kfaces45532(ear) and119671(strand) can use exact constant face response. None1215approvedfaces has conflicted samples;37zeroUVfaces remain open elsewhere. ResolutionReview-v003 authoritative for new selection. No shader/source/UV edit or material3 attempt. Remaining-review.json owns12exact boundaries; connector/tack uncertainty is preserved, not an approval by albedo hue.

## Anatomical ankle transition design

JointTopologyInspection-v003 corrects the declared walk assemblyZ-0.11m atframe1; fullrest maxerror3.584e-7m underunchanged1e-6tolerance. Failedv001/v002diagnostics archivedwithsource/profile/log. Canonicalcore/collarconflicts0; alloutercollarnodes reachable. Directcore/exteriorbypassfaces1/1/8/0 forfore.L/fore.R/hind.L/hind.R. Current hindcollar tops0.173460/0.172082m vs authoredfetlock0.200m are26.540/27.918mmbelowjoint. Manyouterboundaries only1graphhopfromcore; graphhopsarenotphysicalrings. No automaticwelddesign fromnonmanifoldcounts.

JointTransitionAtlas-v003 fourviews reviewed: exactrigidcoreblue/collarorange/exteriorgrey with orthographicX-projectedredjoint/purpleendpoint/greenbone. Originalmarkercoordinates saved; onlycurrentfootguides shown. v001internalguideocclusion/v002overlappingotherfootguides retainedwithclarification. JointTransitionDesign-v001 defines nexthind.L exactboundarylayout beginning8bypassfaces, with flexibleankleskin/rigidshell/featherseparation and exactretopotransferrequirementsifneeded. This is design evidence, not a newskin/mesh/gaitcandidate or proofmovingjoint/subdivisionfixesdeformation. No automaticbudgetreset: primary/walk/topology remain3/3.

HindLeftAnkleLayout-v001 (Propose-JointTransitionBand.py, outer plane Z0.26 = 6cm above authored joint, label0 only): core894/oldcollar440 retained, band1673 (+1233), 0 bypass faces, 135 outer crossing faces. HindLeftAnkleLayoutReview-v001 eight isolated CPU views reviewed: crossing ring continuous; band covers visible ankle skin but also a rear pendant tuft (to Z0.118) and a front ruffle over the hoof wall. 647 band vertices below joint in 94 fragments (below-joint-band.json). Not approved; next is source-indexed ownership of those fragments, not a lower plane/width sweep. No weights/candidate; budgets unchanged.

Production pass 2026-10-19 (user-authorized). Skin: ProxyHeatSkin-v001 rejected (bone heat moved torso to limb-root bones; body p99 537%). SmoothedLegSkin-v002 adopted: diffusion of existing weights over leg domains + 30-hop margin, 300 iterations, hooves 100% fetlock bone; under WalkPilot-v003 motion body p99 27.4->21.5%, hoof core edge change 4.8mm->0, max abs 15.0->16.6mm. Clips: ClipLibrary-v001, 26 clips (gaits crouch walk .07/trot .09/canter .12/gallop .13 m plus reach clamp at 0.995 chain length), all checks pass. Delivery-v001: SK_Voidreign.fbx, root bone, 26 takes, Metallic=0 override, reimport pose error <1.5e-6 m.
ClipLibrary-v002 (2026-10-19): 55 clips, all checks pass (adds turns walk 30/canter 25 deg per cycle, pivots 90/180, start/stop walk+canter, hit_back, hit.m, rear kick, stun, knockdown/getup, injured idle, rest/rise, training approach/hold, rally, fidget, acknowledge, jump/fall/land; defeat rebuilt as sternal lie-down, root -0.7m, legs tucked fore +0.35/hind -0.3m). Delivery-v002 SK_Voidreign.fbx 55 takes verified.
