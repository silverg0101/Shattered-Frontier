# Corrected primary-motion delivery

Status: **offline diagnostic prototype; motion gate failed; no promotion**.

## Open source and evidence

- Current source: ../PrimaryOnlyCombinedFull-v001/Voidreign-ActualSurfacePilot.blend. Adjacent Feature-actual-motion.mdd and Mane-primary-motion.mdd are required dependencies.
- Visible Blender review PID63656 requested autoplay; original unsaved windows preserved. Launch evidence: Artifacts/Art/Voidreign/primary-motion-review-launch-v001.json. The unsaved overlay is diagnostic only.
- TailPrimaryOnlyFull-v001 completed240held+330output in701.8s CPU. Settling RMS3.724mm/s, peak47.717mm/s pass original20/80. Fresh330cache point replay error0. Source IDs match all33233old canonical playback IDs exactly.
- ManePrimaryOnlyFull-v001 passes settling; corrected fullMane+Tail source assembled without old Tail point projection.
- PrimaryOnlyCombinedFullCheck-v001: five of six checks pass across330frames. Protected rigidity0.000846mm, coincident seams0, startup1.742mm, proposed gap increase0.000650mm, maximum declared capsule penetration1.921mm versus5mm.
- FAIL: maximum tail step86.830mm versus80mm, frame175, original vertex281103. Native source-indexed replay shows a smooth peak across frames172-178, not a cache jump. This is on positional component0, Tail label2, about2.198m from semantic root; not a detached-component deletion candidate. Keep the failed limit unchanged. Diagnosis does not establish acceptable appearance or response.
- PrimaryOnlyCombinedFullRetention-v001: all indexed rest geometry/UV/color/normal/material/image checks pass; named weights unchanged and armature binds exact.

## Reproduction

`finalize-failed-v001.py` preserves the original finite runner which stopped on the failed full surface gate. `finalize.py --document-motion-failure` explicitly resumes evidence collection without accepting that failure: it verifies the existing source hash and requires the only failed scoped check to be maximum_surface_step. Existing profiles/outputs are preserved. It then measures retention, animated exchange and full strain, and renders four textured/diagnostic frames. Exact invoked argument arrays are in commands.json; per-stage logs are adjacent. Run from C:/Projects/Shattered-Frontier with py -3 and the full script path. Do not rerun against existing output directories.

Primary input/rest-shape fix is explained in ../DeformationInputRepair-v001/README.md. Shared process lives in docs/art-templates/mount-secondary-motion.md; all Voidreign coordinates, masks, profiles and measurements stay here or in ../MountProfile.md.

## Remaining work

Inspect final renders and measured full strain before any further response change. One declared maximum-step gate still fails. Semantic attachment ownership, lower-leg/ornament coverage, stance/sole authority, head/saddle/PBR, articulated gait and skeletal/runtime delivery remain open. Vertex/capsule checks do not establish triangle, swept or self collision. The new primary adapter is restricted to the verified rigid harness. No new parameter search or arbitrary smoothing is authorized by this evidence.

## Completed exchange, shape and visual evidence

PrimaryOnlyCombinedFullAlembic-v001 passed all284762indexed vertices across330frames after fresh reimport, maximum0.000573mm versus0.01mm. Geometry-cache evidence only.

Full330frame strain, same indexed source rest: maximum edge error Mane390.856 to7.715mm and Tail393.500 to3.616mm; worst-frame p99 relative edge change Mane733.003% to1.570%, Tail753.436% to2.854%. No zero rest edges/faces. Local maxima remain76.09% on Mane edge24284/24285 (10.140mm rest, frame121) and30.99% on Tail edge227311/227342 (4.103mm rest, frame190); local shape requires review, no strain acceptance threshold invented.

Parent inspected all four textured saved-source frames1/121/211/301. Strands retain coherent thickness and attachment appearance in these views; the tail swings and bunches near the moving post. These stills do not prove continuous contact, hidden roots or self collision. Head/saddle painted lighting, bright red/orange tail, static/raised feet and mannequin remain visible prototype limitations. Textured and diagnostic PNGs/render reports are in PrimaryOnlyCombinedFullPreview-v001 and PrimaryOnlyCombinedFullDiagnostic-v001. Source motion and texture retention are separate from material quality.

Exact replay commands, failure logs, reports and original/continued runner sources are retained. New PrimaryMotionReproductionKit-v001 freezes this delivery and direct inputs; large files remain external references. Use its manifest for lookup, not folder-wide discovery. Verification is recorded separately after freezing.
