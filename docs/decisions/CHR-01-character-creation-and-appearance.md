# CHR-01 — Character creation and appearance

Status: design proposal, 2026-10-21. Owner decisions in §2; nothing here is implemented. Paths are repository-relative; `Backend P1` = `docs/Game-Design/Backend/Legends_Shattered_Frontier_Backend_Identity_Gateway_and_Characters_v1.0.md`.

## 1. Plan as written today

| Claim | Source | Status |
| --- | --- | --- |
| `character-service` owns name, slot, `appearance jsonb` ("schema owned by the client appearance system"), `bio` (≤500 chars, p2), location, presence | Backend P1 §4.1, §4.9 | Spec only |
| `CreateCharacter(realm_id, name, appearance, initial_skills[], attribute_allocation, idempotency_key)`; errors `NAME_INVALID`, `NAME_TAKEN`, `SLOTS_FULL`, `REALM_CLOSED`; saga calls `InitializeCharacter`, then the starter grant; `CREATION_PENDING` after 8 s | Backend P1 §4.5, §4.10 | Spec only |
| Names: `^[\p{L}][\p{L}'\-]{2,15}$` after NFKC, one script, per-realm uniqueness on a confusable skeleton, denylist (profanity, `gm`/`admin`, content-pack `reserved_name`), 90-day reuse quarantine | Backend P1 §4.3 | No denylist content exists |
| Rename only when `rename_required`; 8 slots per realm, 16 per account + `slot_bonus` | Backend P1 §4.3–4.4 | No voluntary rename or appearance-change RPC |
| Creation: ≤3 skills over 150 points (≤100 each); 40 attribute points (≤+30 on one) on 10×8 | `Character-Progression/…Service_Interfaces_v1.0.md` InitializeCharacter row; `…Leveling_Attributes_Endgame_v1.0.md` "Start" | `BackendServices/Progression` has no creation entry |
| MVP: "One starter profile; character customization can remain basic" | `…First_MVP_Milestone_v1.0.md` Player row | — |
| Vertical slice: "basic character selection" in; "Extensive appearance editor, account monetization" out | `…Vertical_Slice_Specification_v1.0.md` Entry row | — |
| `cosmetic` items fill client "appearance slots", account-bound; 18 `equip_slot` values | `Itemization/…Item_Model_and_Lifecycle_v1.0.md` category table; Shared Registry | Appearance slots never enumerated |
| Store `convenience`: character slots, name/appearance change, extra wardrobe slots | `LiveOps/…Store_Compensation_Community_v1.0.md` §1.2 | Spec only |
| Auth, session and admission remain a reviewed proposal | `docs/decisions/DEC-04-session-applicability.md` | Blocks real character select |
| Name is `p0_public`; chat moderation is chat-service's; bios and appearance uncovered | Security threat model data classes | Gap |
| Player is `SK_Mannequin` + `ThirdPerson_AnimBP` hard-coded; only `PilotHealth` replicates | `Private/Characters/SFE1Character.cpp:49-58, 318-321` | Code |
| Persisted today: progression, quests, receipts, positions; no name, appearance or slot | `BackendServices/CharacterState.Postgres/Schema*.sql` | Code |
| GFX-01 WP7: "Manny/Quinn now, then a MetaHuman with clothing. Groom hair for the local player only: about 1.5 ms" | `docs/decisions/GFX-01-graphics-quality-plan.md` item 7 | Mannequin done (`docs/evidence/graphics/wp7-player/`) |
| Townsfolk `Looks`: archetype → skeletal mesh, mannequin fallback; design proposes MetaHuman Crowds kits and notes the EULA | `docs/Game-Design/AI/…Townsfolk_Ambient_Life_v0.1.md` look kits, line 211 | Prototype on mannequins |
| Race lore | none | No design doc names any race; "beastfolk" is only `GameClient/ArtSource/NPC-BeastfolkWarrior-01/` |

Out of MVP scope by the specs: extensive appearance editor, store, voluntary rename, realm transfer.

## 2. Decisions (owner, 2026-10-21)

1. Human only at launch; any later race (own skeleton and proportions, non-human head, tail, horns, digitigrade legs) must fit without rework.
2. Hybrid: MetaHuman-quality heads baked into our modular body and armour system; no full MetaHuman runtime per player.
3. "Polished basic" MVP: name, 2 body types, ~8 faces, ~10 hairstyles, beards, colour pickers, starting skills and attributes, rotating 3D preview in a styled scene.
4. Skeleton: UE5 / MetaHuman-compatible skeleton for every player race, adopted now while few clips exist. The AutoRig target and mannequin clips migrate once. This supersedes unknown 11.
5. Body types: male and female. Heads, hair and outfits are authored or fitted per body, so the asset list's fit work doubles for those items.
6. Lore: a Fable-drafted brief of 2-4 human peoples, appearance-only (palettes, hair, starting outfits; no stats), edited by the owner. It closes unknown 3 for humans.
7. MetaHuman: from UE 5.6 MetaHumans are under the UE EULA and may be used with other engines and creative software, so the Blender bake step is permitted (re-check the current EULA before ship). Before committing to all 8 heads, prototype one: export, bake to our topology, attach to the body, then compare quality and cost near and far. Hybrid versus fallback is decided on that evidence. This narrows unknown 2 to bake fidelity.

## 3. Architecture

### 3.1 Appearance schema

Stored in `appearance jsonb` by name; packed to indices on the wire.

| Field | Type | Notes |
| --- | --- | --- |
| `v` | u8 | version; unknown versions rejected |
| `race` | u8 | `0 = human` |
| `body` | u8 | body type within the race (MVP 0/1) |
| `morphs` | u8[N] | quantized per race morph slot (MVP N ≤ 16) |
| `parts` | u16 per slot | head, hair, beard, brows; the race decides which slots exist |
| `colors` | u8 per channel | skin, hair, beard, eyes: indices into race palettes |
| `cosmetics` | u16 per `equip_slot` | wardrobe overrides; 0 = none |

- Validation is server-only, against the race allowlist `content/appearance/<race>.json`: parts exist and are unlocked, palette indices in range, morph count matches, cosmetics owned. `character-service` returns a new `INVALID_ARGUMENT APPEARANCE_INVALID`.
- Budget: ≤64 bytes packed, replicated once per pawn (`FSFAppearance`, initial-only plus hash, `OnRep` builds the mesh); cosmetic swaps replicate one slot. 100 players ≈ 6 KB on join.
- Indices resolve against the same content-pack version on both sides, so a pack change cannot silently remap.

### 3.2 Race definition (data)

One `USFRaceDefinition` data asset per race, generated from the same JSON the server validates:

| Member | Human MVP |
| --- | --- |
| Skeleton | UE4 `SK_Mannequin` (what `tools/Build-NpcAutoRig.py` and all clips target) |
| Base bodies | `SK_Human_BodyA`, `SK_Human_BodyB` |
| Part slots / allowed parts | head[8], hair[10], beard[6], brows[4] |
| Morph list | ≤16 named head morph targets, schema order |
| Palettes | skin 16, hair 16, eye 8 |
| Armour-fit profile | one fit per body type; per-slot body-hide mask |
| Animation | one ABP + IK Rig; new races get an IK Retargeter, not new clips |

A beastfolk race later adds its skeleton, `tail`/`horn` slots and a fit profile; schema and creator UI are unchanged.

### 3.3 Modular assembly

- Near players: body `USkeletalMeshComponent` as leader; head, hair, beard, brows, armour as children via `SetLeaderPoseComponent`. The body skeleton gains the head's face bones so one skeleton serves both.
- Remote players beyond ~30 m: `FSkeletalMeshMerge` at `OnRep` into one mesh with engine LODs; card hair.
- LOD and crowds: engine LODs, Nanite off, per `docs/evidence/npc-nanite-bench.md` (100 skinned NPCs: LOD 2.9 ms vs Nanite 3.5–3.7 ms). Players stay skeletal actors; MassCrowd stays townsfolk-only. Target 100 visible players; WP9 measures it.
- The dedicated server loads no mesh (today's guard stays).

### 3.4 Head pipeline

| Step | Tool |
| --- | --- |
| Author 8 presets and morph deltas | MetaHuman Creator (editor plugin); owner picks faces |
| Export head mesh and 2K textures, strip the face rig | new `tools/Export-MetaHumanHead.py` (editor Python) |
| Bake to our head topology with shared UVs; morphs as morph targets; bind to the body skeleton | new Blender script beside `Build-NpcAutoRig.py` |
| Fallback if fidelity fails | Codex sheet → Pixal3D → Blender cleanup (the `Adventurer-01` route) |

- Licensing: MetaHuman content falls under the UE EULA since 5.6 (Townsfolk doc line 211). Baked heads ship only inside the UE product; Blender is an intermediate tool. Confirm the current text before WP3.
- Hair: cards for everyone at MVP (10 styles, 6 beards, 4 brows); groom for the local player is a later package (GFX-01: ~1.5 ms).
- Variety: 8 heads × 16 morphs × 16 skins; sliders map straight to the quantized bytes.

### 3.5 Armour fitting

- MVP: shared skeleton; pieces authored on Body A, fitted to Body B by a Blender shrinkwrap/lattice script, reviewed, exported `_B`; body-hide masks per piece.
- Future races: the same script with a cage deform from the human base to the race base; tails and horns get mask cutouts; pieces that cannot deform are flagged `fit: manual` in the race profile. Expect one script pass plus hours of cleanup per race and armour set, not re-authoring.

### 3.6 Creator flow and UI (HUD-v1 kit)

| Screen | Content | Call |
| --- | --- | --- |
| Character select | ≤8 slot cards: name, level, zone, live portrait; Enter, Create, Delete (typed name at level ≥20), Restore | `ListCharacters`, `EnterWorld`, `DeleteCharacter`, `RestoreCharacter` |
| 1 Body and face | body type, 8 faces, skin, morph sliders | local |
| 2 Hair | hair, beard, brows, colours | local |
| 3 Skills and attributes | 3 of 35 skills over 150 points; 40 attribute points, ≤+30 each | local; mirrors `InitializeCharacter` limits |
| 4 Name and bio | pattern checked while typing; uniqueness and denylist on submit | `CreateCharacter` with `Idempotency-Key`; on `CREATION_PENDING` poll `GetCharacter`; `NAME_TAKEN` returns here with the name kept |

- Preview: map `/Game/Frontier/Maps/UI/CharacterCreate`, Silverbrook backdrop (`GameClient/ArtSource/backdrop.py`), the game's own assembly on a preview pawn, RMB rotates, face/body camera presets, styled by `Framework/SFHudStyle.h` and `/Game/Frontier/UI/Kit/`.
- Until DEC-04 lands the UI runs against a local in-process store with the same validator, so creation is testable without auth.

## 4. Asset list for the MVP

| Asset | Count | Pipeline we have | Owner supplies / approves | Effort |
| --- | --- | --- | --- | --- |
| Race definition and allowlist JSON | 1 | hand | palettes, slot lists | S |
| Base bodies A, B + underwear | 2 | Codex sheet → Pixal3D → Blender retopo; skin hand-painted (no Poly Haven skin) | silhouettes | M |
| Heads | 8 | MetaHuman Creator → bake (new scripts) | pick faces | M |
| Face morph targets | ≤16 | MetaHuman deltas → Blender bake | slider list | M |
| Hair cards | 10 | MetaHuman hair or Blender cards | styles | L |
| Beards / brows | 6 / 4 | same | approve | M |
| Palettes | 16/16/8 | Codex swatch sheet → JSON | approve | S |
| Starter outfit fitted to A and B | 1 set | `PlayerCharacter/Adventurer-01` PixalMV/AutoRig exists, unreviewed, needs retexture; fit script | look | M |
| Skeleton, clips, ABP | 0 new | UE4 mannequin + existing clips | — | — |
| Preview map | 1 | `backdrop.py`, `silverbrook-lighting.json` | approve | S |
| Creator UI elements (slider, swatch, dropdown, portrait frame, slot card, text field) | ~8 | `UI/HUD-Kit-v1/generate.sh` (Codex) → `Process-HudKit.ps1` → `Import-HudKit.py` | approve | S |
| Skill and attribute icons | 35 + 8 | Codex icon route | approve | M |
| Name denylist, `reserved_name` tags | 1 pack | hand; NPC names from the census | word list | S |
| 100-player evidence scene | 1 | `tools/Build-NpcNaniteBench.py` pattern | — | S |

New tooling: two scripts (MetaHuman head export, armour fit) and one data asset type.

## 5. What you don't know yet (ranked)

1. **Auth and character select.** No account, session or character service exists (DEC-04 pending); real select needs the gateway path.
2. **MetaHuman licence and bake fidelity.** Whether re-topologized heads keep quality and whether the EULA allows the Blender step. Check before WP3.
3. **Race lore.** No document names playable or NPC races, proportions or heads; the race model is designed blind.
4. **Armour-fit cost per race.** Script fitting is untested; tails, horns and digitigrade legs may need manual sets.
5. **Appearance change and rename rules.** Store sells both; Backend has only forced rename. Cooldown, price, history and jsonb migration are undecided.
6. **Naming policy.** Denylist content and owner; no `CheckName` RPC exists for live pre-checks.
7. **Crowd cost.** 100 modular players with merged meshes is unmeasured; the bench covered one NPC mesh.
8. **Moderation.** Bio is free p2 text with no classifier; names have only a denylist; no report or forced-rename UI.
9. **Accessibility.** Colour-blind-safe palettes, text scale and controller navigation in the creator are unspecified.
10. **Body-type semantics.** Neutral builds versus masculine/feminine changes head and voice assets.
11. **Skeleton choice.** UE4 mannequin keeps the AutoRig path; the UE5 skeleton retargets better; switching later costs every clip.

## 6. Work packages

| WP | Owns | Done when | Where |
| --- | --- | --- | --- |
| CHR-WP1 Schema and validator | `content/appearance/human.json`, `BackendServices/CharacterAppearance` + tests | Rejects bad versions, unknown parts, wrong morph counts; tests pass | Codex |
| CHR-WP2 Base bodies | `ArtSource/PlayerCharacter/Human-Body-01/` A, B, underwear | Both bodies pass `review_player_in_game.py` on the mannequin skeleton | Codex (Blender headless) |
| CHR-WP3 Heads and morphs | `tools/Export-MetaHumanHead.py`, bake script, 8 heads | Heads attach to both bodies; morphs render; licence note recorded | Editor + Blender |
| CHR-WP4 Hair, beards, brows, palettes | card assets, palette JSON | Every allowlisted part imports and swaps at runtime | Editor + Blender |
| CHR-WP5 Runtime assembly | `USFRaceDefinition`, `FSFAppearance`, `SFE1Character` built from data | Two clients see each other's appearance; server loads no mesh; focused `ShatteredFrontier.Character.Appearance` tests | Editor |
| CHR-WP6 Armour fit | fit script A→B, starter outfit retextured and fitted | Outfit equips on both bodies without clipping in captures | Codex (Blender) |
| CHR-WP7 Creator UI and preview | preview map, UMG screens, kit elements, icons | Full flow yields a valid packet against the local store; captures in `docs/evidence/graphics/chr-create/` | Editor |
| CHR-WP8 Select and CreateCharacter | select screen, gateway adapter, name pack | Create/select/enter on the local store; swaps to the gateway when DEC-04 lands | Editor + Codex (.NET) |
| CHR-WP9 Evidence | 100-player scene, bytes per join, captures | Frame cost and size recorded against the GFX-01 budget | Editor |

Order: WP1 → WP2 ∥ WP3 ∥ WP4 → WP5 → WP6 → WP7 → WP8 → WP9. WP1, WP2 and WP6 can run as Codex background tasks; the rest need the editor or MetaHuman Creator.
