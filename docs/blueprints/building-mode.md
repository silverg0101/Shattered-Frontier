# Building-mode claim grid blueprint

Scope: the ground grid that build mode (B) projects around the player to show owned, affordable, limit, blocked and out-of-band claim cells. Spec and states: [CLM-01](../decisions/CLM-01-claim-points-and-build-grid.md) §5. Concepts: `docs/Game-Design/concept-art/UI/building-claim-grid-reference.webp` (owner reference) and `docs/Game-Design/concept-art/UI/v5/build-mode.png`. Shared rules: [README.md](README.md); HUD panel rows: [ui-hud.md](ui-hud.md).

## A. Build the grid (in order)
| # | Step | Command / value | Done when |
|---|---|---|---|
| 1 | State mask | `UTexture2D` R8 64 × 64, `TextureFilter::Nearest`, no mips, `SRGB` off, one byte per 1 m cell centred on the player cell: 0 none, 1 owned, 2 affordable, 3 blocked, 4 limit, 5 out of band; rebuilt by `SFPlayerController::RefreshPlacementPreview` on player-cell change, placement/removal receipt, claim replication or points change; upload with `UpdateTextureRegions` (one 4 KB region) | mask changes on the frame the receipt arrives; never a full texture recreate |
| 2 | Material | `M_ClaimGrid` (`/Game/Frontier/UI/Build/`), domain Deferred Decal, blend `DBuffer Translucent Color`, two-sided off. Inputs: `StateMask`, `GridOriginCm` (vector, the mask's cell 0,0 world corner), `CellCm` 100, `LineCm` 3, `MajorEvery` 4, `MajorLineCm` 5, `FadeStartCm` 1600, `FadeEndCm` 2400, `LimitFadeCm` 300, colours `Owned` #E8E1CE, `Affordable` #688267, `Blocked` #A34842, `OutOfBand` #B69A55. Lines: `frac((WorldXY − GridOriginCm) / CellCm)` within `LineCm / CellCm` of 0 or 1, widened to a 1.5 px minimum with `fwidth`; major every 4th cell; blocked fill 25 % plus a 45° hatch `frac((X + Y) / 25 cm) < 0.5`; limit cells multiply alpha by `1 − dist_to_last_affordable / LimitFadeCm`; whole decal alpha × `saturate((FadeEndCm − dist_to_player) / (FadeEndCm − FadeStartCm))` | `MI_ClaimGrid` instance with the defaults above |
| 3 | Decal | one `UDecalComponent` on the pawn while `IsBuildModeActive()`, size 2400 × 2400 × 1000 cm, rotated to project down, `SortOrder` 10, destroyed on leaving build mode; `GridOriginCm` and the mask follow the player cell (snap to whole metres, never per frame) | no decal in the world outside build mode |
| 4 | HUD row | cost panel gains `Claim  +N pt  (allocated / total)` under the materials in the have/need style (`ui-hud.md` §3), crimson underline when short; status line is "Valid placement" or "Invalid placement" plus the server reason text; no dev strings | row present in `build-live.png` |
| 5 | Capture | `ui-hud.md` §4 protocol, E1 session, `Shot showui`, three views from the river-trail plot: `build-grid-owned` (stand on the slab), `build-grid-limit` (walk to the budget edge), `build-grid-blocked` (face the road); `make_pairs.py` with the reference on the left | three `*-compare.png` in `docs/evidence/ui-v5/build-grid/` |
| 6 | Review | fresh Fable with `docs/visual-review.md`; fix list implemented until PASS | `REVIEW.md` PASS |

## B. Values that must hold
| Item | Value | Rule |
|---|---|---|
| Cell | 1 m (= 2 × 2 BLD-01 50 cm cells) | claim cells, cost and the mask share it |
| Line weight | 3 cm minor, 5 cm major every 4 m, 1.5 px screen minimum | lines must read at 20 m without turning into a sheet of colour |
| Alpha | owned 35 % / major 50 %; affordable 55 % lines + 8 % fill; blocked 60 % lines + 25 % hatched fill; out of band 40 % lines | the grid stays about a quarter of the ghost's brightness, as in the reference |
| Fades | distance 16 → 24 m to zero; limit 3 m past the last affordable cell; nothing drawn beyond | an empty budget shows only the owned area |
| Blocked | never colour alone: hatch plus the reason in the status line | colour-blind rule 3 of `ui-hud.md` |
| Authority | client mask is a preview; a refused placement refreshes it from the server reply | the grid never disagrees with the server for more than one round trip |

## C. Known failure -> rule
| Failure | Rule |
|---|---|
| Grid drawn as a procedural mesh or line batch | one DBuffer decal and one material (CLM-01 §5.3) |
| Texture recreated per change, or mask updated per frame | region upload on change only; snap the origin to whole metres |
| Decal survives leaving build mode | lifetime bound to `SetBuildMode` |
| Red cells without a reason | every blocked cell has a hatch and the status line names the reason |
