# Playable combat loop: attack, visible boars, camera pitch

**Area:** gameplay · **Worktree:** any free lane · **From:** owner live playtest 2026-10-26 on fb58ecc5 (Deploy-Lan, both dev characters 15 m from `boar_east_gate`)

## Defects (owner, live)
1. **No way to attack.** Owner targeted a boar (Tab; target frame "Boar 489/489") and found no swing or attack. The server logged no combat ability request at all. `ASFE1Character` binds number keys 1-7 to `GetBindableCombatAbilities()` on production maps, but the hotbar shows build pieces 1-4 and tools 5-6 on those same keys, and no ability is shown anywhere. Find why no request reached the server, and give the player an obvious attack: a visible combat ability on the bar (or a default attack on left mouse with a target), with the key shown, plus a swing animation.
0. **Root cause found (fix first): boars spawn on the treetops.** `Grounded()` (`SFCombatPilot.cpp:62`) traces down from 200 m on `ECC_WorldStatic` and takes the first hit. Since 9538fcb7 gave PCG trees and rocks `BlockAll`, that first hit is the canopy, so boars (and anything else placed with `Grounded`, including player respawns) appear up in the trees; the owner saw boxes "fly up out of the sky". The camera aiming at a target up there would also explain the sky-only view. Fix it once in the shared helper: ground on the terrain, not on foliage (e.g. a multi-trace taking the terrain hit, or a terrain-only object channel). Check every other caller with the same pattern, and add a test that grounds a point under a tree to the terrain.
2. **Boars can't be seen.** The owner stood 15 m from the group and saw no boar, even though the target frame found one. The boar is an engine cube (`ASFPilotBoar` Body, `SFCombatPilot.cpp:341`), probably lost in the brush. Make boars readable: a visible stand-in at boar size with a nameplate and health bar overhead. The Forge boar replaces it later; don't wait for it. Move one boar group onto open ground in plain view near the East Gate (outside the protected volume, 80 m wall rule) in `content/world/zones/greenlands_silverbrook.json`.
3. **Camera goes under the character.** Mouse-look pitches until the camera sits at ground level looking at the sky. Add pitch limits as typed `USFE1Settings` values with shipped defaults, so the boom never ends below the feet.
4. **Server log spam.** The dedicated server logs `GetSocketInfoByName(hand_r): No SkeletalMesh` many times a second. The tool attach from 797eedba must not run on the server.

## Checks
- An automated netcheck: a player targets a boar, attacks it with the visible ability and kills it, and loot appears. Extend the existing loot-handoff netcheck if it fits.
- Pitch-limit test; zone content tests; a capture of the new boar spot from the gate and of an attack in progress.

## State
Queued; owner blocked on it for the live loot test. Needs client and server packages plus `Deploy-Lan.ps1` after it lands.
