# Player character art

## Status
- **In game now:** the UE4 `SK_Mannequin` with `ThirdPerson_AnimBP` (`ASFE1Character`, GFX-01 WP7 step 1).
- **Rejected:** [Adventurer-01](Adventurer-01/README.md), an image-to-3D (Pixal3D) body. Up close it showed shredded alpha shards on the hair, shirt and vest, a smeared face and broken silhouettes, so image-to-3D is not used for hero characters. Its assets stay in the repo, but nothing loads them.
- **Next:** one MetaHuman prototype on the UE5 skeleton (CHR-01 decisions 4-7), planned below.

## MetaHuman prototype plan
Each step is marked **[script]** (committed, repeatable), **[editor]** (done in the open editor; ask Task Runner for the editor window first) or **[owner]** (needs the owner's Epic account or a decision; Claude never signs in).

Installed with UE 5.8: the `MetaHumanCharacter` plugin (in-editor MetaHuman Creator, the assembly pipelines and the `DefaultGroomPipeline`/`DefaultOutfitPipeline`), plus `MetaHumanSDK`, `RigLogic` and `HairStrands`. The plugin ships no wardrobe content (hairstyles, outfits). It does call **MetaHuman Cloud Services** for high-resolution face and body textures (`RequestHighResolutionTextures`) and for face auto-rigging (`RequestAutoRigging`).

| # | Step | Who |
|---|---|---|
| 1 | MetaHuman licence: done, recorded in CHR-01 decision 7. | done |
| 2 | Enable the `MetaHumanCharacter` plugin (and its dependencies) in `ShatteredFrontier.uproject`, client and editor only, and check that a dedicated-server build still compiles and cooks without it. | [script] + build |
| 3 | Sign in to the Epic account inside the editor's MetaHuman Creator so the cloud services work. | [owner] |
| 4 | Create `/Game/Frontier/Characters/Player/MH_Adventurer/MHC_Adventurer` (a MetaHuman Character asset): rugged male, about 30, short dark hair and a trimmed beard, matching `Adventurer-01/concept-v001.png`. | [editor] |
| 5 | Request the high-resolution face and body textures (cloud). | [owner] (needs step 3), then [editor] |
| 6 | Auto-rig the face (cloud) so the face has a working rig. | [owner] (needs step 3), then [editor] |
| 7 | Get the wardrobe: a hairstyle with card LODs, and an outfit of a rough linen shirt, leather vest, trousers and boots. These come from Epic's MetaHuman wardrobe content (Fab or the Creator's downloads), not the engine plugin, and need the signed-in account. Anything downloaded stays read-only and git-ignored as licensed content; only our assembled output goes under `/Game/Frontier/`. | [owner] |
| 8 | Assemble the character with the output root set to `/Game/Frontier/Characters/Player/MH_Adventurer/`, using the gameplay (optimized) quality: hair as cards, LODs kept, no skinned Nanite (`docs/evidence/npc-nanite-bench.md`). | [editor] |
| 9 | A script that checks the assembled output: skeleton is the UE5 MetaHuman body skeleton, LOD count, card hair at gameplay LODs, triangle counts, every asset under `/Game/Frontier/Characters/Player/`. | [script] |
| 10 | Runtime: `ASFE1Character` loads the assembled body, face and clothing with a UE5 locomotion AnimBP on the MetaHuman skeleton. This is the CHR skeleton package (AnimBP and clips on the UE5 skeleton), replacing the UE4 mannequin and its `/Game/Mannequin` cook entry. Groom hair goes on the local player only and card hair on remote players; nothing cosmetic loads on a dedicated server. | [script] + build |
| 11 | `review_player_in_game.py` at the WP8 forest bookmark, with near and far captures and an FPS chart, then the owner's look sign-off. | [script] then [owner] |

### Owner steps, in order
1. Sign in to the Epic account in the editor's MetaHuman Creator.
2. Request the cloud high-resolution textures and the face auto-rig for `MHC_Adventurer` (or let Claude press the buttons once the account is signed in).
3. Get the hairstyle and the outfit pieces (linen shirt, leather vest, trousers, boots) from the MetaHuman wardrobe or Fab.
4. Approve the look in the step 11 captures.
