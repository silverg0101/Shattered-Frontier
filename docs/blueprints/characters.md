# Characters blueprint (what is known)

Scope: the player character and townsfolk bodies. Depth: `GameClient/ArtSource/PlayerCharacter/README.md` (MetaHuman plan, owner steps), CHR-01 decisions, CTL-01 camera. Shared rules: [README.md](README.md).

## 1. State
| Item | Value |
|---|---|
| In game | UE4 `SK_Mannequin` + `ThirdPerson_AnimBP` from `/Game/Mannequin` (`Characters/SFE1Character.*`, GFX-01 WP7 step 1); townsfolk are grey mannequins |
| Rejected | `Adventurer-01` image-to-3D (Pixal3D): shredded alpha on hair/shirt/vest, smeared face, broken silhouette up close. Assets stay in `GameClient/ArtSource/PlayerCharacter/Adventurer-01/`; nothing loads them |
| Next | one MetaHuman prototype on the UE5 skeleton (`/Game/Frontier/Characters/Player/MH_Adventurer/`), gameplay quality: card hair, LODs kept, no skinned Nanite; groom hair on the local player only; nothing cosmetic on the dedicated server. Steps 3, 5-7 need the owner's Epic sign-in (cloud textures, face auto-rig, wardrobe) |
| Evidence | `docs/evidence/graphics/wp7-player/`, `wp7-adventurer/` (5 views each: default zoom, walking, first person, near zoom, far zoom + `player-review.json`), `wp7-shadow-trail/` |

## 2. Pipeline (for the MetaHuman prototype; [script]/[editor]/[owner] per the README table)
1. Enable `MetaHumanCharacter` (+ `MetaHumanSDK`, `RigLogic`, `HairStrands`) in `ShatteredFrontier.uproject` for client and editor only; confirm the dedicated-server target still compiles and cooks. [script + build]
2. Owner signs in to the Epic account in the editor's MetaHuman Creator. [owner]
3. Create `MHC_Adventurer`: rugged male ~30, short dark hair, trimmed beard, per `Adventurer-01/concept-v001.png`. [editor]
4. Request high-res textures and face auto-rig (cloud). [owner, then editor]
5. Wardrobe from Epic's MetaHuman content (rough linen shirt, leather vest, trousers, boots; hairstyle with card LODs): downloaded content stays git-ignored and read-only; only the assembled output goes under `/Game/Frontier/`. [owner]
6. Assemble with output root `/Game/Frontier/Characters/Player/MH_Adventurer/`, gameplay (optimized) quality. [editor]
7. Check script: UE5 MetaHuman body skeleton, LOD count, card hair at gameplay LODs, triangle counts, every asset under `/Game/Frontier/Characters/Player/`. [script]
8. Runtime: `ASFE1Character` loads body/face/clothing with a UE5 locomotion AnimBP; replace the `/Game/Mannequin` cook entry. [script + build]
9. Review: `UnrealEditor.exe <uproject> /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -RenderOffscreen -ResX=2560 -ResY=1440 "-ExecCmds=py GameClient/ArtSource/PlayerCharacter/review_player_in_game.py <evidence folder>"` at the WP8 forest bookmark: near and far captures, walking, first person, FPS chart; pairs vs the concept; fresh Fable review, then the owner's look sign-off. [script, then owner]

## 3. Gates and rules
| Rule | Detail |
|---|---|
| Shadow trail | any skinned mesh: `SetBoundsScale(2)` (or bounds that contain the running pose); review capture looks back along the walked path for VSM trails (`cc93d96`, `b34e72e`); `diagnose_shadow_trail.py` is the repro |
| No image-to-3D for heroes | Pixal3D/Hunyuan geometry is acceptable only for props retextured in Blender with CC0 scans; never for faces, hair or clothing |
| Hair | groom only on the local player (~1.5 ms budget); cards on remote players; no groom on the server |
| Nanite | no skinned Nanite (`docs/evidence/npc-nanite-bench.md`) |
| Licensing | MetaHuman wardrobe and Fab clothing are read-only, git-ignored; the assembled output is the only committed content |
| Foliage push | `MPC_Interaction.PlayerPosition` is written by C++ each tick (WP7); keep it when the character class changes |
| Capture | 2560x1440 for the character review (the only exception to 1920x1080), `-game`, timed; concept left, live right |
| Never | ask the owner for screenshots; sign in to Epic yourself; load `/Game/Mannequin` in a new feature; grade your own capture |

## 4. Concept refs
`GameClient/ArtSource/PlayerCharacter/Adventurer-01/concept-v001.png`, `concept-art/UI/v5/gameplay-hud.png` (third-person framing and scale), `docs/Game-Design/concept-art/Greenlands-Forest-Look-v1.webp` (lighting the character stands in).
