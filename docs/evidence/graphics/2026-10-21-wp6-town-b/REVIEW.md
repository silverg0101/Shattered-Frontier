# WP6b review — Silverbrook town, pass 2 (foundations, debug overlays, street dressing)

Reviewer: independent (Fable), did not build the work. Judged per `docs/visual-review.md` against `Silverbrook-Design-Board-v1.webp` (GFX-01 board), `UI/v5/gameplay-hud.png`, the owner's screenshots 16-19 and the job brief `ed-wp6b.txt`. Commits 19ea35c, b260935. Every image below was read at full resolution plus crops.

**Result: 1 PASS / 4 FAIL.** The owner's foundation complaint is fixed. The debug overlays are gone from the captures and 117 actors are asserted hidden, but the capture script also forces `ShowFlag.TextRender 0`, so the captures do not themselves prove the label fix. The street is still not dense or lived-in.

## Owner complaints

| Complaint | Status | Evidence |
|---|---|---|
| 16.webp: foundation ~20 cm, glossy plank, one-sided ledge, gap under the wall | **Fixed** | `foundation-close.png`: dark matte rubble course ~60 cm visible, proud 4 cm on all sides, wall sits on it with no slit; `foundation-check.json`: 312 houses, gap 0.0, min visible 53.4 cm, proud 4.0 cm, burial >= 100 cm, 0 failures. Stoop steps added at the door. |
| 16/18/19.webp: yellow/blue/pink ground lines, 3D district labels, "Tt" sprites | **Fixed in the map, not proven by the capture** | No line or label in any capture. `check_hidden_in_game.py` asserts `hidden` on 117 TextRender/ZoneShape/ZoneGraphData/Labels/Lanes/Marker actors and the builder sets it on rebuild. But `capture_game.py:37` runs `ShowFlag.TextRender 0` before every shot, which hides labels whether or not `bHiddenInGame` is set. Fix 6 removes it. |
| 17.webp and pass-1 review: bare grass strip, one cart, identical rows | **Not fixed** | `main-street.png`: about 3 prop groups per 50 m (brief: 6-10); a 6-8 m lawn between the cobble and the left house row; two house meshes repeated at the same yaw with tints that differ by < 10 %; no smoke, washing, signs, ladders, planters or vines visible. `market.png`: the frame is an empty lawn with houses standing in it. |

## main-street — FAIL

1. **What's wrong:** Dressing density. Along the ~120 m of street in frame there are 7-8 groups (sack/barrel/cart cluster at 12 m, hay cart at 20 m, sacks at 45 m, two carts at 70-90 m, a lamp-base barrel, the hitching post and sacks the camera stands in). That is ~3 per 50 m against the brief's 6-10, and every group is one mesh. The right-hand earth margin (x 1150-1900, y 560-700 at 1080p) is empty for 40 m.
   **Fix type:** LEVEL. **Instruction:** see fix 1.
2. **What's wrong:** The left frontage strip is still a lawn: from the cobble edge (x ~1380 at y 1080 to x ~1520 at y 560) to the left house plinths is 6-8 m of flat green with five tufts. The earth paint stops 4.5 m past the road edge (`STREET_EARTH_EXTRA_M = 4.5`) while the left row is set back further. The GFX-01 board's streets run packed earth to the wall line.
   **Fix type:** LEVEL. **Instruction:** see fix 2.
3. **What's wrong:** The right-hand row is the same two townhouse meshes at the same yaw, 5 in a line, and the "HouseTone" tints (max 0.96-1.04 per channel) are invisible at 1080p: the five roofs are one grey and the five plaster fields one cream.
   **Fix type:** LEVEL. **Instruction:** see fix 3.
4. **What's wrong:** No chimney smoke visible on any of the ~20 chimneys in frame (the brief asks for ~40 %). The cards exist (`ChimneySmoke_*`) but are 1.6 x 0.8 m and read as nothing (see chimney-smoke view).
   **Fix type:** CODE. **Instruction:** see fix 4.
5. **What's wrong:** The capture camera stands inside a prop group: the hitching post bar crosses the frame at y 430-560 and a sack pile fills the lower right 400 px. A third of the view is blocked.
   **Fix type:** CODE (capture). **Instruction:** see fix 6.
6. **What's wrong:** Nothing on the walls: no bracket signs except the inn's, no washing lines, no ladders, no sill planters, no ivy or moss at the plinth line. No assets for these exist (`Meshes/SM_SilverbrookProp*` has 15 props, none of these).
   **Fix type:** ART. **Instruction:** see fix 8 (Codex).

## market — FAIL

1. **What's wrong:** The view is 60 % lawn (y 480-1080 across the full width) with sparse tufts, houses standing in open grass with no yards, walls, paths or fences between them; one 1.5 m fence section is visible at the second house. The board's blocks are packed with walled yards, gardens and lanes. The named subject, the market, is two stalls 55 m away occupying 80 x 60 px.
   **Fix type:** LEVEL (yards and paths) + CODE (capture framing). **Instruction:** fixes 5 and 6.
2. **What's wrong:** Identical houses: the four townhouses in the middle ground are two meshes, each twice, same roof grey and plaster cream.
   **Fix type:** LEVEL. **Instruction:** fix 3.
3. **What's wrong:** The left house's windows glow orange at noon (x 60-170, y 640-740). Deferred: GFX-02 J5 owns lamp/window emissive by time of day.

## house-frontage — FAIL

1. **What's wrong:** A pale, smooth block 0.45 m wide x ~1.1 m tall stands right of the door (x 1300-1540, y 180-520 in `foundation-close.png`; x 1560-1650, y 390-480 in `house-frontage.png`) with the ashlar texture smeared into vertical streaks and a dark slab cap. It is part of the townhouse mesh, not the plinth or stoop, and reads as an untextured placeholder.
   **Fix type:** CODE (kit). **Instruction:** fix 7.
2. **What's wrong:** Wall base bare: no planter on the bay-window sill, no tub on the stoop, no moss/ivy at the plinth line; the brief asks for all three. The flower tub placed by `dress_house_details` is round the side at `(side*170, b.min.y*.43)`, out of the frontage view.
   **Fix type:** LEVEL + ART. **Instruction:** fixes 5 and 8.

Foundation, stoop, door, bay window, bracket sign and lantern otherwise match the board's townhouse elevation. The far houses on the left skyline stand in open lawn (same as market item 1).

## foundation-close — PASS

The plinth is a matte dark-grey rubble course (roughness 0.86, `M_SB_FoundationStone`), ~60 cm visible, 4 cm proud on every side, the ashlar sits on it with no slit, and the three rubble steps rise to the doorstep. This is what the owner asked for. Two notes, not fails: the triplanar projection means the stones do not wrap the corner (a seam at x ~1310, y 540-830); and the cobble (left) and lawn (right) ground are flat tiles with no relief, deferred to GFX-02 J2.

## chimney-smoke — FAIL

1. **What's wrong:** No smoke is visible. The card (`ChimneySmoke_*`, `/Engine/BasicShapes/Plane` with `M_LampSmoke`, scale 1.6 x 0.8 m, pitch 90) sits 0.8 m above a chimney that is itself 1.2 m wide; at 20 m it is at most a 40 px grey blur indistinguishable from the sky. The gameplay-hud concept shows a plume ~3 chimney heights tall, soft white, drifting.
   **Fix type:** CODE. **Instruction:** fix 4.
2. Far white low-poly mountain silhouettes at left and right (x 0-500 and 1300-1920, y 700-1080): deferred to GFX-02 (J1b review / J2 outer-landscape material).

## Deferred to GFX-02 / GFX-03 (seen, not failed here)

- Ground: flat cobble and lawn tiles without relief, hard cobble/grass edge (`foundation-close.png`, `main-street.png`) — GFX-02 J2.
- Lamps and window glow lit at noon (`main-street.png` lamps, `market.png` left house) — GFX-02 J5.
- Far mountains: white faceted shapes (`main-street`, `chimney-smoke`), smooth brown domes (`market`) — GFX-02 J1b/J2.
- Wall ashlar tiles repeat and mortar runs unbroken round the corner; plaster is flat — GFX-03 §1-2.
- Market stall is the stand-in `SM_MarketStall` with flat striped cloth — GFX-03 gap 7.
- No decals, no per-instance weathering, pixel-identical packages — GFX-03 gaps 5-6.

## Fix list (shared first)

1. **Street group density** — Engineer, LEVEL. `GameClient/ArtSource/Silverbrook/silverbrook-props.json` `street_groups`: `spacing_m.value` 7 → 4, `offset_m.value` 1.8 → 1.2, `max_groups.value` 520 → 700. `silverbrook_props.py` `dress_street_groups`: place on both sides of every station and make each group a cluster. Replace
   ```python
   key=f'group:{road}:{k}';side=1 if k%2 else -1
   mesh=meshes[int(_h(key)*len(meshes))%len(meshes)]
   for side,shift in ((side,0),(side,0.8),(-side,0),(-side,0.8)):
   ```
   with
   ```python
   key=f'group:{road}:{k}'
   mesh=meshes[int(_h(key)*len(meshes))%len(meshes)]
   for side,shift in ((1,0),(1,0.8),(-1,0),(-1,0.8)):
   ```
   and remove the trailing `break` so both sides get a group. After the main `a=...` spawn, add one or two satellites: `for j in range(1+int(_h(key+'n')*2)): m2=meshes[int(_h(key+str(j))*2)]` (RainBarrel or SackPile) at `(px+side*uy*(60+40*j), py-side*ux*(60+40*j), z)`, yaw `+ _h(key+'y'+str(j))*360`, same label with `_{j}` suffix, same tag and folder; skip if `pp._free` fails. Gate: `STREET_GROUPS` total >= 6 per 50 m of primary+secondary length (`sum(len(_stations(pts,50)) for ...)*6`).
2. **Packed earth to the house line** — Engineer, LEVEL. `GameClient/ArtSource/Greenlands/build_silverbrook.py:741` `STREET_EARTH_EXTRA_M = 4.5` → `8.0`; `paint_street_edges.py:12` `COBBLE_EXTRA, EARTH_EXTRA = 2.0, 4.5` → `2.0, 8.0`. This brings the B channel to the plinth line of both rows on the primary street (half width 4 m + 8 m). Rerun `paint_street_edges.py` and `check_ground.py`.
3. **House variation that reads at 1080p** — Engineer, LEVEL. `silverbrook_props.py` `vary_house_finishes` palettes → five tones with >= 20 % luminance spread:
   ```python
   palettes={'MI_SB_LimePlaster':[(1.00,.93,.80),(.84,.86,.88),(1.00,.86,.66),(.93,.95,.84),(.78,.74,.70)],
             'MI_SB_Slate':[(.78,.86,1.02),(1.06,.90,.74),(.86,.98,.88),(.66,.68,.72),(1.00,1.00,1.00)]}
   ```
   and `i=int(_h(a.get_actor_label()+'tone')*5)%5`; keep the MI naming `_HouseTone1..5`. Also break the rows: in `build_silverbrook.py` where townhouse meshes are queued by label hash (line ~164 `Queue a delivered building mesh`), add the hash to the mesh choice so two adjacent lots never draw the same package (`if mesh==prev_mesh: mesh=next candidate`).
4. **Visible chimney smoke** — Engineer, CODE. `silverbrook_props.py` `dress_house_details`: the two smoke planes `spawn(f'ChimneySmoke_{label}_{k}',plane,x,y,p.z+sz*100+80,yaw+35+90*k,(1.6,.8,1))` → `spawn(..., x,y,p.z+sz*100+230,yaw+35+90*k,(1.4,4.2,1))` (1.4 m wide, 4.2 m tall card whose base sits 20 cm above the pot). In `spawn`, after `set_material(0,smoke)`, create or reuse `MI_SB_ChimneySmoke` from `M_LampSmoke` via `_instance` and set `Opacity` 0.45 and `Tint` (0.80,0.80,0.83); if `M_LampSmoke` exposes neither parameter, add a `ScalarParameter Opacity` (default 0.45, bounds 0.2-0.7) multiplied into its opacity output and a `VectorParameter Tint` multiplied into emissive/base, with the texture panned +0.12 UV/s in V so the plume rises. Acceptance: in `chimney-smoke.png` the plume covers >= 150 x 400 px above the pot at 20 m.
5. **Yards, planters and paths per house** — Engineer, LEVEL. `dress_house_details`: gardens for `h<.38` → `h<.75`; replace the single fence with an L of three `SM_SilverbrookPropFenceSection` (at `sx`, `sx` + 1.6 m along the house axis, and one turned 90° at the far corner), crops 4 → 10 in two rows at 0.6 m pitch (`for i in range(10): cx,cy=pt(sx+side*(55+(i%2)*85),(i//2-2)*60)`), and a `SM_SilverbrookPropFirewoodStack` at the rear corner for `h<.5`. Move the flower tub to the frontage: `pt(side*170, b.min.y*.43)` → `pt(side*(b.max.x-b.min.x)*.35, b.min.y-45)` (beside the door on the street face) and add a second tub on the stoop top step when a `_Stoop0` exists. Paths: in `paint_street_edges.py`, for each `*_Stoop0` actor paint B = 1 in a 1.2 m wide strip from the stoop to the nearest road station (same `cell` lookup, `f=1` within 0.6 m of the segment).
6. **Capture validity and framing** — Engineer, CODE. `capture_game.py`: delete `'ShowFlag.TextRender 0'` from the console list (the labels must be absent because `bHiddenInGame` is set, not because the show flag is off). Views: `main-street` eye `(22, 330, 1.7)` → `(24.5, 336, 1.7)` (2.5 m off the group, same look target); `market` eye `(55, -82, 5)` → `(30, -62, 3.0)`, look `(0, -30, 1.2)` so the plaza and its eight stalls fill the frame. Recapture all five views and remake the pairs.
7. **Smeared pier block beside the townhouse door** — Engineer, CODE (kit). Find the element in the frontage townhouse package (`GameClient/ArtSource/Silverbrook/Townhouse*/build_townhouse*.py`; the frontage house is the first townhouse queued in `build_silverbrook.py:179`) that is a 0.45 x 0.45 x ~1.1 m box beside the door with a cap slab, and give it box-projected UVs at the ashlar's 1.8 m tile (`Geo.box(..., uv='box')` or the package's equivalent) or assign it `M['rubble']`/the plinth material so it reads as a stone pier. Rebuild the package, reimport with `import_silverbrook.py`.
8. **Missing wall dressing assets** — Codex, ART. Five new props under `GameClient/ArtSource/Silverbrook/Prop*/` built the same way as `PropRainBarrel` (Blender, 1k-2k albedo/normal/ORM, <= 8k tris each, pivot at the mount point), imported as `SM_SilverbrookProp<Name>` and placed by `dress_house_details` on 40 % of houses (sign on the street face at 2.6 m, washing line between adjacent houses < 4 m apart, ladder leaning at 75° beside a side wall, planter on each bay-window sill, ivy clump at a plinth corner):
   - `SM_SilverbrookPropShopSign`: iron bracket 0.7 m + hanging board 0.6 x 0.45 m, three board textures (bread, boots, ale). Prompt: "Medieval hanging shop sign on a wrought-iron scroll bracket, weathered oak board with painted loaf of bread, cracked paint, 3/4 view, neutral grey background, asset sheet with front/side/top, reference the LAMPS & STREETSIGNS tile of the Silverbrook design board (stag-crest lantern sign)."
   - `SM_SilverbrookPropWashingLine`: two 2.4 m posts, 3.5 m rope, six cloth pieces (linen, undyed wool, one blue). Prompt: "Medieval washing line, rough rope between two wooden posts, six hanging linen and wool garments, muted undyed colours with one indigo tunic, slight sag and wind, asset sheet, neutral background."
   - `SM_SilverbrookPropLadder`: 3.2 m ash ladder, 10 rungs, rope-bound. Prompt: "Hand-made medieval wooden ladder, ten round rungs, rope lashing at joints, worn grey-brown ash, asset sheet front/side, neutral background."
   - `SM_SilverbrookPropSillPlanter`: 0.9 m terracotta trough with geraniums and trailing herbs, two colour variants. Prompt: "Terracotta window-sill planter with red geraniums and trailing thyme, chipped rim, damp soil, asset sheet, reference the FLOWERS & FOLIAGE tile of the Silverbrook design board."
   - `SM_SilverbrookPropIvyClump`: four crossed alpha cards, 1.2 x 1.0 m, 1k alpha atlas of ivy and moss. Prompt: "Climbing ivy and moss clump growing from the base of a stone wall, dense dark-green ivy leaves with lighter moss at the root line, flat front view on a neutral grey background for alpha cutout, 1024 x 1024."

Resubmit with all five views recaptured; the review is repeated against the same concepts.
