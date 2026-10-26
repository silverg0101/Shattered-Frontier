# WORLD: door-lantern light, realistic broadleaf trees, felling art for every species

Date: 2026-10-26. Status: implemented; visual review in `docs/evidence/graphics/2026-10-26-silverbrook-lanterns-trees/`.

Owner decisions (answers to the water decision's questions): light each house front from its own front-door lantern; replace the low-poly `SM_Oak` dressing with realistic trees already in the project; give every harvestable tree and rock species felling art.

- **Door lanterns.** `lamp_fx.py` picks one front-door lantern per building (`door_index`: front face, nearest the centre line) and gives it a shadowless spot light aimed out of the wall (the cone stays on the street side, so no light leaks into rooms). Values are in `lamp-fx.json` (`door_light_*`): 20 cd, 7 m reach, 20 deg tilt, 25 m draw distance, at most 48. Door lantern panes glow all day with their light (`door_lantern_lit`). No global lighting change.
- **Trees.** `tree_oak` places `SM_PH_Broadleaf` (Poly Haven `tree_small_02`, 249k tris, 4 LODs, trunk collision) at scale 1.5-2.0 (7-9 m). Pines and young broadleaf were already the generated realistic families. Candidate ids of districts with oaks change with the new transforms.
- **Felling.** One generic method (`SFSpeciesFelling`) for any mesh, client-only: scars are pale cut-wood (or fresh-fracture) patches projected onto the mesh's own collision, one per hit (up to `MaxNotches`), with chips; the last hit tips the mesh about its base away from a seeded direction (trees leave a stump) or squashes it into a remnant (rocks). Hits taken are `Capacity - Charges` on a fully grown node; late joiners see the felled state. Timing and sizes are `USFSpeciesFellingSettings`.
- **Check.** `ShatteredFrontier.Gathering.SpeciesFellingPaths` loads every harvestable role's dressing meshes from the look profile and fails if one cannot be planned.

Open owner questions: (1) Should the door lamps burn by day (now yes, as asked) or follow the street-lamp sun schedule (`door_lantern_lit`)? (2) The scar is geometry laid on the surface, not a cut into it; the hero MightyOak keeps true boolean cuts only where the server owns the cut.
