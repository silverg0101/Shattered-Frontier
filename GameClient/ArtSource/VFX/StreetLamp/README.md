# Silverbrook street-lamp VFX (GFX-01)

`lamp_fx.py` finds every placed Silverbrook mesh whose committed `lamp-sockets.json` has lantern glass and creates one warm, shadowless point light per glass chamber. The socket map is generated from dedicated `*LanternGlow` / `*_Glow` material islands in the source GLBs, including baked building lanterns. Four street-lamp manifests were used to check the coordinates (largest difference 3 mm). Source art and the town builder are untouched.

| Effect | Implementation |
| --- | --- |
| Flame and emissive glass | Procedural Pillow flame texture on a two-sided additive card at each socket, with a two-frequency material-time flicker. The original mesh glass remains amber. |
| Warm light | 90 lumen movable point light, 2.8 m attenuation, no shadows, 25 m draw cap. Light-function material uses the same flicker speed and amount. |
| Halo | Two crossing soft additive cards; bloom comes from the existing Silverbrook post process. |
| Smoke | One soft alpha card 20 cm above each glass center; slow panning. |
| Ember | One tiny additive card per lamp with a short pulse about every 9 seconds. |
| Fireflies | A deterministic subset of lamps (28% default), 1–3 instanced sprite quads each; per-instance random blinking and slow WPO drift. HISM ends at 25 m. |

The material graph and shared HISM cards avoid Niagara Python graph editing. All graphics are client cosmetic and have no Python tick or replication. On a dedicated server there is no rendering, but the baked map actors may still load; server actor exclusion needs a client-only runtime class in a later integration. The 0.5 ms GPU target at 1440p and in-game look remain for the editor lane to measure and tune. No shadows are cast, including at close range. Small attenuation provides the ground pool.

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

This imports/rebuilds only `/Game/Frontier/VFX/StreetLamp/` assets, replaces `FX_StreetLamp_*` actors in the open Silverbrook map, saves the map, and prints `LAMP_FX` counts. Inspect a Lamp 2 with its two glass chambers and a building lantern, review dusk/day/night captures, and measure the 1440p GPU delta against the 0.5 ms budget. Adjust only bounded JSON values and rerun. The editor run and tuning are **not done in this handoff**.

Builder-owner hook after `for spawn in pending:` finishes (the builder owns its file):

```python
import lamp_fx; importlib.reload(lamp_fx); lamp_fx.apply()
```

The hook assumes `lamp_fx.py` is on the builder's existing Greenlands Python path. The editor map must be saved by the builder or by `apply(save=True)`. Rerunning is idempotent: previous `FX_StreetLamp_*` actors are removed before replacement. Only meshes listed in `lamp-sockets.json` are eligible; a new lamp mesh requires the offline socket extraction to be rerun and committed.
