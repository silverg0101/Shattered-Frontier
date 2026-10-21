# Legends of the Shattered Frontier — Peoples Brief
### Version 0.1 — draft for owner edit (2026-10-21)

Closes CHR-01 decision 6 ([CHR-01](../../decisions/CHR-01-character-creation-and-appearance.md) §2): 2–4 human peoples, appearance-only. Anything not cited is **PROPOSED**; nothing is canonical until the owner edits it.

## 1. Setting premise (5 lines)

1. The launch world is one continent, `continent/shattered_mainland`, with four biomes: Greenlands, Frozen North, Desert Wastes and the Corrupted Frontier, the "dangerous endgame region" ([Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md) §2; `concept.md` Launch World).
2. **PROPOSED:** "Shattered" names an event, the Shattering, a cataclysm within living memory that broke the old roads and realms and left the ancient ruins and sealed portals the Greenlands doc places in every frontier zone ([Greenlands](../Regions/Legends_Shattered_Frontier_Region_Greenlands_v0.1.md) §6; `concept.md` Exploration Gates). What caused it is left open (§5).
3. **PROPOSED:** the Corrupted Frontier is where the break is still open; the rest of the mainland is being resettled from its edges inward.
4. The Greenlands is "the heart of growth, opportunity and civilization in the Shattered Frontier", and Silverbrook is the northern gateway town whose banner reads Honor, Work, Hospitality and Opportunity ([Greenlands](../Regions/Legends_Shattered_Frontier_Region_Greenlands_v0.1.md) §2; [Silverbrook](../Regions/Legends_Shattered_Frontier_Settlement_Silverbrook_v0.1.md) §1).
5. People come for land, work and trade; no lore may advertise a biome or faction war that is not implemented ([Vertical Slice](../Legends_Shattered_Frontier_Vertical_Slice_Specification_v1.0.md), Narrative and content constraints).

## 2. Human peoples for the MVP (all PROPOSED)

Three peoples, each tied to a place the region doc names. Looks and flavour only: no stats, no advantage, every part and colour selectable by everyone, every people authored for both body types (CHR-01 §2 decision 5). Skin indices refer to the 16-entry human palette, light to deep; ranges overlap on purpose.

### 2.1 Valefolk — the river valleys

- **Homeland:** the main river from Valemere down to Rivermoor and Meadowgate ([Greenlands](../Regions/Legends_Shattered_Frontier_Region_Greenlands_v0.1.md) §3–4).
- **Why the frontier:** younger children of farms and mills heading north for a plot of their own; Silverbrook's half-timbered style is theirs ([Silverbrook](../Regions/Legends_Shattered_Frontier_Settlement_Silverbrook_v0.1.md) §3).
- **Temperament:** practical, hospitable, slow to anger, fond of a fair deal.
- **Look:** skin 2–10 (fair to warm olive); hair chestnut, brown, dark blond, auburn; eyes brown, hazel, green, grey. Rounded features, freckles common. Hair: short crop, tied back, single braid, loose shoulder length. Beards: clean, stubble, trimmed short.
- **Outfit motifs:** undyed linen, wool in Silverbrook navy with a gold hem, leather apron or belt, stag-and-river stitching, worn boots.
- **Names:** Alder, Brenna, Tomas, Wenna, Hal. Two syllables, soft consonants, plant and river words. A spoken byname of trade or home ("Wenna of Rivermoor") is flavour; the stored name is one word.

### 2.2 Northkin — the mountain border

- **Homeland:** Silvermere and the passes under the northern range, on the Frozen North border ([Greenlands](../Regions/Legends_Shattered_Frontier_Region_Greenlands_v0.1.md) §3–4).
- **Why the frontier:** hard winters push families south; Silverbrook is the first town below the snow line.
- **Temperament:** reserved, loyal, dry humour, respect earned by work.
- **Look:** skin 0–8 (pale to light olive), wind-reddened cheeks; hair ash blond, pale red, dark brown, early grey; eyes grey, pale blue, brown. Strong brows, long faces, kept subtle. Hair: long braided, half-up, undercut, cropped. Beards: full, long, braided, forked.
- **Outfit motifs:** layered wool, fur trim, antler or bone toggles, cold greys and deep blues, wrapped legs, a hood.
- **Names:** Hrothe, Asny, Kelda, Varn, Ottar. Hard consonants, one or two syllables, "-rid", "-vald", "-ny" endings. Parent bynames ("Varn's daughter") are spoken, never stored.

### 2.3 Seaborn — the islands and coasts

- **Homeland:** the Whispering Isles, the Azure Sea coast and the Emerald Archipelago ([Greenlands](../Regions/Legends_Shattered_Frontier_Region_Greenlands_v0.1.md) §3), reached by the ocean routes `concept.md` lists as exploration gates.
- **Why the frontier:** fishers and traders following the rivers inland; they fill the Riverside docks and mill district ([Silverbrook](../Regions/Legends_Shattered_Frontier_Settlement_Silverbrook_v0.1.md) §4).
- **Temperament:** curious, talkative, quick to bargain, superstitious about weather.
- **Look:** skin 6–15 (warm tan to deep brown); hair black, dark brown, sun-bleached tips; eyes dark brown, amber, green. Broad smiles, strong cheekbones, kept subtle. Hair: tight curls, locs, close crop, wrapped scarf. Beards: short shaped, chin strap, clean.
- **Outfit motifs:** sailcloth and oilcloth, salt-faded teal and ochre, knotted cord, shell and bead ties, bare forearms, rope belt.
- **Names:** Kaelo, Nima, Oren, Sefa, Tavi. Vowel endings, two syllables, sea and wind words. Ship or crew bynames are spoken only.

**Palette overlap:** skin 2–8 is shared by all three; every hair, beard and eye colour is in the single human palette and never locked.

## 3. How peoples map onto data

- One race: `race = 0` (human) and one allowlist, `content/appearance/human.json` ([CHR-01](../../decisions/CHR-01-character-creation-and-appearance.md) §3.1–3.2). A people is a sub-selection inside that file, not a second race.
- Each people is a **preset**: a preferred skin index range, a first-shown subset of the 10 hair, 6 beard and 4 brow parts, suggested hair and eye indices, and one **starter-outfit variant** (three recolours and trim swaps of the one `Adventurer-01` set, fitted to both bodies; CHR-01 §4).
- The player picks a people on creator screen 1; the preset fills the sliders and every part and colour stays freely selectable. Validation is unchanged: the server checks the human allowlist, not the people.
- **PROPOSED:** add `people` (u8, `0 = unset`) to the appearance schema so the starter grant picks the outfit variant and NPC lines can read it. Otherwise the people lives only in the creator and the grant.
- Names follow the Backend rule: one word, `^[\p{L}][\p{L}'\-]{2,15}$`, one script (CHR-01 §1). Bynames are flavour for bios and NPC lines.

## 4. Future races hook (all PROPOSED)

CHR-01 decision 1 requires later races to fit without rework via a new `USFRaceDefinition` and fit profile (§3.2, §3.5).

| Candidate | Implied by | Technical need |
| --- | --- | --- |
| Beastfolk | `GameClient/ArtSource/NPC-BeastfolkWarrior-01/` (1.85 m, fur, ears, claws, mannequin skeleton) | Plantigrade: human skeleton plus head, `tail`, ear slots; digitigrade: own skeleton and IK Retargeter |
| Frostborn | Frozen North biome, Silvermere border | Taller proportions on the shared skeleton, own fit profile |
| Ruinborn | Desert Wastes ruins and sealed portals | Non-human head set, `horn` slot, armour mask cutouts |
| Void-touched | Corrupted Frontier; Voidreign death-mount motif (`GameClient/ArtSource/Voidreign/ConceptPrototype04/README.md`) | Human skeleton, skin palette with emissive channel, optional `horn` slot |
| Isle-kin | Whispering Isles, Emerald Archipelago, sea gates | Head set with fins or gills, same skeleton |

## 5. Open questions for the owner

1. What was shattered, and how long ago: living memory or myth? This fixes how much ruins and portals may explain.
2. Three peoples or four? A Desert Wastes people (dust-road traders) is the natural fourth.
3. Keep Valefolk, Northkin, Seaborn, or rename? Do they appear in game text, or only in art and NPC flavour?
4. Does a people get a stored `people` byte, or stay creator-only?
5. Should the starter-outfit variants be only recolours of `Adventurer-01`, or three distinct silhouettes (doubles fit work per CHR-01 §2 decision 5)?
6. Which illustrative factions become real (`greenhold_wardens`, `free_prospectors`, `ironroot_traders`, [Factions](../AI/Legends_Shattered_Frontier_AI_Factions_and_Faction_Service_v1.0.md) §1.1), and do any align with a people? This draft keeps them separate.
7. Are the beastfolk neighbours, a hostile faction, or future players? This decides whether the warrior art stays NPC-only.
8. Does Silverbrook's white stag belong to the Valefolk, the town, or a Greenlands crown no doc yet names?
