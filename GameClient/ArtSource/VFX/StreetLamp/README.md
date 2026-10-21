# Silverbrook street-lamp VFX (GFX-01)

`lamp_fx.py` finds every placed Silverbrook mesh whose committed `lamp-sockets.json` has lantern glass and creates one warm, shadowless point light per glass chamber. The socket map is generated from dedicated `*LanternGlow` / `*_Glow` material islands in the source GLBs, including baked building lanterns. Four street-lamp manifests were used to check the coordinates (largest difference 3 mm). The builder calls the VFX hook after it places the town.

| Effect | Implementation |
| --- | --- |
| Flame and glass | Two crossing additive flame cards at each socket, with two-frequency flicker, inside translucent amber glass on street lamps. |
| Warm light | 900 lumen movable point light, 4 m attenuation, no shadows, 25 m draw cap. Named RGB arguments prevent UE Python's positional BGRA mapping from turning it blue. |
| Halo | Two crossing 26 cm soft amber cards inside each glass chamber. |
| Smoke | One faint 16 × 25 cm alpha card 45 cm above each glass center. |
| Fireflies | At 28% of lamps, 1–3 tiny soft dots 1.3–2.6 m away, sun-elevation gated to dusk/night; no ribbons or velocity alignment. HISM ends at 25 m. |

The material graph and shared HISM cards avoid Niagara Python graph editing. All graphics are client cosmetic and have no Python tick or replication. On a dedicated server there is no rendering, but the baked map actors may still load; server actor exclusion needs a client-only runtime class in a later integration. No shadows are cast, including at close range. Small attenuation provides the ground pool.

`Greenlands/lamp-fx.json` is the shipped, typed, bounded art config. It has labels, help, units, defaults, fallback, owner, permission, scope, apply time and rollback metadata. These are release-bound client visuals, not an editable server console. The `time-of-day` enum is reserved for GFX WP6 and deliberately refuses to build until the runtime signal exists. Current default is `always`.

## Offline regeneration

From repository root, with Python 3, Pillow, NumPy and SciPy installed:

```powershell
py -3 GameClient/ArtSource/VFX/StreetLamp/generate_textures.py
py -3 GameClient/ArtSource/VFX/StreetLamp/extract_sockets.py
```

Commit the generated PNGs and JSON with changes to the source script. `lamp-sockets.json` is the shipped socket declaration; no GLB parsing occurs in the editor.

## In-editor run when the serialized editor lane is free

Open the ShatteredFrontier full editor, enable Python remote execution, and open `/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook`. Save or consciously discard existing map edits first. From the repository root run:

```powershell
py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/lamp_fx.py --save
```

This imports/rebuilds only `/Game/Frontier/VFX/StreetLamp/` assets, replaces `FX_StreetLamp_*` actors in the open Silverbrook map, saves the map, and prints `LAMP_FX` counts. The UE 5.8 editor run completed with 938 sockets/lights. The Python component transform and HISM component creation use UE 5.8 exposed APIs. Street-lamp components disable Nanite because Nanite does not render the translucent glass material.

Review captures and concept/live pairs are in `docs/evidence/graphics/lamp-fx/`: 1920x1080 dusk, day and night views of a street lamp, a townhouse lantern area and a street with several lamps. Temporary sun, sky and exposure overrides were used only in `-game`; they were not saved. The final 1440p `stat gpu` street view measured Graphics Queue Total 5.38 ms on and 5.17 ms off (about **0.21 ms** delta, under the 0.5 ms budget) on an RX 7900 XTX. This is an on/off snapshot, not a distribution across multiple routes. The brightness value was raised within its declared bound from 90 to 220 lumens, then the assets and map were rebuilt and the captures and GPU pair repeated.

**Pass 2 verdict: not a concept match.** The cold light came from a positional `unreal.Color` call: intended `(r=255, g=150, b=65)` became `(r=65, g=150, b=255)`. Pass 3 correction and new evidence are in `docs/evidence/graphics/lamp-fx-3/`.

Builder-owner hook after `for spawn in pending:` finishes (the builder owns its file):

```python
import lamp_fx; importlib.reload(lamp_fx); lamp_fx.apply()
```

The hook assumes `lamp_fx.py` is on the builder's existing Greenlands Python path. The editor map must be saved by the builder or by `apply(save=True)`. Rerunning is idempotent: previous `FX_StreetLamp_*` actors are removed before replacement. Only meshes listed in `lamp-sockets.json` are eligible; a new lamp mesh requires the offline socket extraction to be rerun and committed.
