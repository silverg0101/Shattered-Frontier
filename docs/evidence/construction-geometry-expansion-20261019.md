# Construction geometry expansion candidate — 2026-10-19

The package-free construction geometry fixture now emits canonical Q65536 payloads for six exact one-cell solids: `cell-full`, `slab-half`, `slab-quarter`, `post-quarter`, `slope-1to1`, and `corner-outer`. The three new definitions follow the nominal solids in `player-construction-kit-contract.md`; all six payloads pass exact signed-volume, outward planar face, closed opposite-edge, convexity, canonical-byte, contact, and allowed-rotation checks.

Canonical payloads are at `Artifacts/ConstructionGeometry/candidate-six-shape-20261019-01/`. Blender 5.1.1 generated editable `.blend`, `.glb`, `.fbx` and `asset-view.png` for all six shapes at `Artifacts/ConstructionGeometry/six-shape-blender-candidate-20261019-03/`; its manifest pins source and export hashes. Fresh GLB and FBX imports pass bounds (0.1 mm tolerance), one mesh object, one coincident-welded connected component, zero loose vertices, zero boundary edges and zero nonmanifold/unused edges. Geometry and model outputs are deliberately marked `prototype-subset` with `acceptance_claim=false`. The tracked v001 Blender/GLB/FBX package and its hashes are unchanged. This candidate does not authorize final meshes and does not establish GEO-01 or GEO-02 acceptance.

The model-export gate took three runs (one initial and two repairs): first baseline export passed bounds/import only; second added index-edge topology checking and falsely rejected importer seam-split vertices; third changed the read-only topology check to weld coincident positions at 1 μm and passed all six fresh imports. The false rejection did not change or repair any model geometry. Exact logs and first two outputs remain in their separate ignored attempt folders. This exhausts the candidate export stage's three attempts; do not rebuild these outputs under this stage record.

| Shape | Exact occupied fraction | Canonical payload SHA-256 |
|---|---:|---|
| `cell-full` | 1 | `9455bd04dab9df9f4643f175c95fb17cb7e25e6248c239975a8f1280f040061a` |
| `slab-half` | 1/2 | `e6236a7b600eccb0a2f558f74524e183ef57154a278938694629ed247ecfd154` |
| `slab-quarter` | 1/4 | `d355fbd51a05d423f3ae212736b3c40c5aca93ebdd294af57b7379d5df5d1749` |
| `post-quarter` | 1/4 | `d46e4ebdfe2d20723d337e3886431621579fa6c3d764acc23cb5d544c0035144` |
| `slope-1to1` | 1/2 | `e55a54e6824119dffb929bf7de536eba786e7ce145efa732620da147cca4cdda` |
| `corner-outer` | 1/3 | `53fc0704c2407d0530716cee82ac9a22c74104ee3b0b8be487b69458fb1b00f1` |

Focused checks: `python tests/test_construction_geometry.py` — 6 tests passed; `python tests/test_construction_compatibility.py` — 10 tests passed. Existing golden hashes for the three v001 shapes remain unchanged. Exact face contacts for `slab-quarter`, `post-quarter`, and `corner-outer` are included in the compatibility fixture. Canonical source-to-export surface correspondence, material quality, full GEO-02 assemblies, and GEO-03 Unreal import/collision remain open. Remaining geometry references are `corner-inner`, `step-two`, `arch-2m`, and `curve-quarter-r2`; non-convex forms need multi-part boundary support, and arch/curve need an accepted approximation-error threshold.
