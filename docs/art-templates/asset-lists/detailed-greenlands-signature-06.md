# Detailed Greenlands signature asset — batch 06

- Phase / scope revision: Detailed / 1 (2026-10-18)
- Sources: `docs/dependency-map.md` — Optional art prototype; `GameClient/ArtSource/GreenlandsWaystone/README.md` — optional arrival-landmark study; `docs/Game-Design/Legends_Shattered_Frontier_Vertical_Slice_Specification_v1.0.md` — Arrival landmark, spawn, orientation and reconnect fallback.
- Owner: Codex
- Dependencies: reuse the existing Waystone turnaround unchanged as the concept master; package a newly generated clean single-asset view separately. Do not modify the existing prototype or benchmark references.

| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |
|---|---|---|---|---|---|---|
| PROP-GreenlandsWaystone-01 | Prop / signature weathered stone arrival waystone with bronze compass medallion | Dependency map optional art prototype; existing Waystone package README; vertical slice Arrival landmark | P1; reuse existing design artwork; detailed signature concept already exists | Codex / image-complete | agent-selected existing concept / ready-for-model-test | [v001 package](../../GameClient/ArtSource/GreenlandsWaystone/References/v001/); [clean asset view](../../GameClient/ArtSource/GreenlandsWaystone/References/v001/asset-view.png); [manifest](../../GameClient/ArtSource/GreenlandsWaystone/References/v001/manifest.json) |

## Component boundaries

One freestanding stone waystone with its base and attached compass medallion. Reuse the existing image-generated design; do not alter or overwrite original art or Blender model. The existing model preview is context, not a substitute clean asset view because its material/detail treatment differs from the concept master.

## Progress

- Existing sources inspected: `Waystone-Turnaround.png` and `Waystone-Preview.png`.
- v001 package contains the unchanged source turnaround, a separate generated/inspected clean view, exact source and clean-view prompts, provenance, hashes, image dimensions/format/alpha, and review findings.
- Detailed batch gate passed: 1/1 documented signature asset image-complete. No additional detailed asset is currently queued by an identified source; next asset ID is therefore unassigned pending the next documented need.
- Approval and production-import status remain separate and unclaimed.
